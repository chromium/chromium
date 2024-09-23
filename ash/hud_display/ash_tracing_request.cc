// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/ash_tracing_request.h"

#include <sys/mman.h>
#include <sys/sendfile.h>

#include "ash/hud_display/ash_tracing_handler.h"
#include "ash/hud_display/ash_tracing_manager.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/posix/safe_strerror.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace ash {
namespace hud_display {
namespace {

// Tests supply their own IO layer generator.
static std::unique_ptr<AshTraceDestinationIO> (
    *test_ash_trace_destination_io_creator)(void) = nullptr;

class DefaultAshTraceDestinationIO : public AshTraceDestinationIO {
 public:
  ~DefaultAshTraceDestinationIO() override = default;

  // Overrides base::CreateDirectory.
  bool CreateDirectory(const base::FilePath& path) override {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(path, &error)) {
      PLOG(ERROR) << "Failed to create Ash trace file directory '"
                  << path.value() << "'";
      return false;
    }
    return true;
  }

  // Overrides base::File::File(). Returns pair {File file, bool success}.
  std::tuple<base::File, bool> CreateTracingFile(
      const base::FilePath& path) override {
    base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    const bool success = file.IsValid();
    return std::make_tuple(std::move(file), success);
  }

  // Implements memfd_create(2).
  std::tuple<base::PlatformFile, bool> CreateMemFD(
      const char* name,
      unsigned int flags) override {
    base::PlatformFile memfd = memfd_create(name, flags);
    return std::make_tuple(memfd, memfd != base::kInvalidPlatformFile);
  }

  bool CanWriteFile(base::PlatformFile fd) override {
    return fd != base::kInvalidPlatformFile;
  }

  int fstat(base::PlatformFile fd, struct stat* statbuf) override {
    return ::fstat(fd, statbuf);
  }

  ssize_t sendfile(base::PlatformFile out_fd,
                   base::PlatformFile in_fd,
                   off_t* offset,
                   size_t size) override {
    return ::sendfile(out_fd, in_fd, offset, size);
  }
};

std::string GenerateTraceFileName(base::Time timestamp) {
  return base::UnlocalizedTimeFormatWithPattern(
      timestamp, "'ash-trace_'yyMMdd-HHmmss.SSS'.dat'");
}

std::unique_ptr<AshTraceDestination> GenerateTraceDestinationFile(
    std::unique_ptr<AshTraceDestinationIO> io,
    const base::FilePath& tracng_directory_path,
    base::Time timestamp) {
  if (!io->CreateDirectory(tracng_directory_path))
    return nullptr;

  base::FilePath path =
      tracng_directory_path.AppendASCII(GenerateTraceFileName(timestamp));
  auto [file, success] = io->CreateTracingFile(path);
  if (!success) {
    LOG(ERROR) << "Failed to create Ash trace '" << path.value() << "' : error "
               << file.error_details();
    return nullptr;
  }

  return std::make_unique<AshTraceDestination>(std::move(io), std::move(path),
                                               std::move(file),
                                               base::kInvalidPlatformFile);
}

std::unique_ptr<AshTraceDestination> GenerateTraceDestinationMemFD(
    std::unique_ptr<AshTraceDestinationIO> io) {
  constexpr char kMemFDDebugName[] = "ash-trace-buffer.dat";
  auto [memfd, success] = io->CreateMemFD(kMemFDDebugName, MFD_CLOEXEC);
  if (!success) {
    PLOG(ERROR) << "Failed to create memfd for '" << kMemFDDebugName << "'";
    return nullptr;
  }
  return std::make_unique<AshTraceDestination>(std::move(io), base::FilePath(),
                                               base::File(), memfd);
}

// Must be called with blocking allowed (i.e. from the thread pool).
// Returns null pointer in case of error.
std::unique_ptr<AshTraceDestination> GenerateTraceDestination(
    std::unique_ptr<AshTraceDestinationIO> io,
    base::Time timestamp,
    bool is_logging_redirect_disabled,
    const base::FilePath& user_downloads_folder) {
  constexpr char kTracingDir[] = "tracing";
  constexpr char kGlobalTracingPath[] = "/run/chrome/";
  if (is_logging_redirect_disabled) {
    return GenerateTraceDestinationFile(
        std::move(io),
        base::FilePath(kGlobalTracingPath).AppendASCII(kTracingDir), timestamp);
  }
  if (!user_downloads_folder.empty()) {
    // User already logged in.
    return GenerateTraceDestinationFile(
        std::move(io),
        base::FilePath(user_downloads_folder.AppendASCII(kTracingDir)),
        timestamp);
  }
  // Need to write trace to the user Downloads folder, but it is not available
  // yet. Create memfd.
  return GenerateTraceDestinationMemFD(std::move(io));
}

base::FilePath GetUserDownloadsFolder() {
  return Shell::Get()->shell_delegate()->GetPrimaryUserDownloadsFolder();
}

bool IsLoggingRedirectDisabled() {
  return Shell::Get()->shell_delegate()->IsLoggingRedirectDisabled();
}

struct ExportStatus {
  std::unique_ptr<AshTraceDestination> destination;
  bool success = false;
  std::string error_message;
};

ExportStatus ExportDataOnThreadPool(base::PlatformFile memfd,
                                    const base::FilePath& user_downloads_folder,
                                    base::Time timestamp) {
  ExportStatus result;
  result.destination = GenerateTraceDestination(
      test_ash_trace_destination_io_creator
          ? test_ash_trace_destination_io_creator()
          : std::make_unique<DefaultAshTraceDestinationIO>(),
      timestamp,
      /*is_logging_redirect_disabled=*/false, user_downloads_folder);

  DCHECK(!result.destination->path().empty());

  struct stat statbuf;
  if (result.destination->io()->fstat(memfd, &statbuf)) {
    result.error_message = std::string("Failed to stat memfd, error: ") +
                           base::safe_strerror(errno);
    LOG(ERROR) << result.error_message;
    return result;
  }
  off_t offset = 0;
  const ssize_t written = result.destination->io()->sendfile(
      result.destination->GetPlatformFile(), memfd, &offset, statbuf.st_size);
  if (written != statbuf.st_size) {
    const std::string system_error = base::safe_strerror(errno);
    result.error_message =
        base::StringPrintf("Stored only %zd trace bytes of %" PRId64 " to '",
                           written, static_cast<int64_t>(statbuf.st_size)) +
        result.destination->path().value() + "', error: " + system_error;
    LOG(ERROR) << result.error_message;
    return result;
  }
  result.success = true;
  return result;
}

AshTracingRequest::GenerateTraceDestinationTask
CreateGenerateTraceDestinationTask(std::unique_ptr<AshTraceDestinationIO> io,
                                   base::Time timestamp) {
  return base::BindOnce(&GenerateTraceDestination, std::move(io), timestamp,
                        IsLoggingRedirectDisabled(), GetUserDownloadsFolder());
}

}  // anonymous namespace

AshTraceDestinationIO::~AshTraceDestinationIO() = default;

AshTraceDestination::AshTraceDestination() = default;

AshTraceDestination::AshTraceDestination(
    std::unique_ptr<AshTraceDestinationIO> io,
    const base::FilePath& path,
    base::File&& file,
    base::PlatformFile fd)
    : io_(std::move(io)), path_(path), file_(std::move(file)), memfd_(fd) {}

AshTraceDestination::~AshTraceDestination() {
  Done();
}

base::PlatformFile AshTraceDestination::GetPlatformFile() const {
  if (memfd_ != base::kInvalidPlatformFile)
    return memfd_;

  return file_.GetPlatformFile();
}

bool AshTraceDestination::CanWriteFile() const {
  return io_->CanWriteFile(GetPlatformFile());
}

void AshTraceDestination::Done() {
  if (memfd_ != base::kInvalidPlatformFile) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce([](base::PlatformFile fd) { close(fd); }, memfd_));
    memfd_ = base::kInvalidPlatformFile;
  }
  if (file_.IsValid()) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce([](base::File fd) { fd.Close(); }, std::move(file_)));
  }
}

AshTracingRequest::AshTracingRequest(AshTracingManager* manager)
    : timestamp_(base::Time::Now()), tracing_manager_(manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  std::unique_ptr<AshTraceDestinationIO> io =
      test_ash_trace_destination_io_creator
          ? test_ash_trace_destination_io_creator()
          : std::make_unique<DefaultAshTraceDestinationIO>();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      CreateGenerateTraceDestinationTask(std::move(io), timestamp_),
      base::BindOnce(&AshTracingRequest::OnTraceDestinationInitialized,
                     weak_factory_.GetWeakPtr()));
}

AshTracingRequest::~AshTracingRequest() = default;

void AshTracingRequest::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK(tracing_handler_);
  status_ = Status::kStopping;
  tracing_manager_->OnRequestStatusChanged(this);
  tracing_handler_->Stop();
}

void AshTracingRequest::OnTracingStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  status_ = Status::kStarted;
  tracing_manager_->OnRequestStatusChanged(this);
}

void AshTracingRequest::OnTracingFinished() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  tracing_handler_.reset();
  if (!trace_destination_->path().empty()) {
    // Trace was already stored to the real file.
    status_ = Status::kCompleted;
    trace_destination_->Done();
    tracing_manager_->OnRequestStatusChanged(this);
    return;
  }
  status_ = Status::kPendingMount;
  tracing_manager_->OnRequestStatusChanged(this);

  // User logged in while tracing. Need to start saving file immediately.
  if (user_logged_in_)
    StorePendingFile();
}

// Will trigger trace file write if needed.
// If trace is already finalized, `on_completed` will be called immediately.
void AshTracingRequest::OnUserLoggedIn() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  user_logged_in_ = true;
  StorePendingFile();
}

base::PlatformFile AshTracingRequest::GetPlatformFile() const {
  return trace_destination_->GetPlatformFile();
}

//  static
void AshTracingRequest::SetAshTraceDestinationIOCreatorForTesting(
    std::unique_ptr<AshTraceDestinationIO> (*creator)(void)) {
  CHECK(!test_ash_trace_destination_io_creator);
  test_ash_trace_destination_io_creator = creator;
}

//  static
void AshTracingRequest::ResetAshTraceDestinationIOCreatorForTesting() {
  test_ash_trace_destination_io_creator = nullptr;
}

// static
AshTracingRequest::GenerateTraceDestinationTask
AshTracingRequest::CreateGenerateTraceDestinationTaskForTesting(
    std::unique_ptr<AshTraceDestinationIO> io,
    base::Time timestamp) {
  return CreateGenerateTraceDestinationTask(std::move(io), timestamp);
}

const AshTraceDestination* AshTracingRequest::GetTraceDestinationForTesting()
    const {
  return trace_destination_.get();
}

void AshTracingRequest::OnTraceDestinationInitialized(
    std::unique_ptr<AshTraceDestination> destination) {
  DCHECK(!trace_destination_.get());
  trace_destination_ = std::move(destination);
  status_ = Status::kInitialized;
  tracing_manager_->OnRequestStatusChanged(this);

  tracing_handler_ = std::make_unique<AshTracingHandler>();
  tracing_handler_->Start(this);
}

void AshTracingRequest::OnPendingFileStored(
    std::unique_ptr<AshTraceDestination> destination,
    bool success,
    std::string error_message) {
  trace_destination_ = std::move(destination);
  // Cleanup possible errors.
  trace_destination_->Done();
  status_ = Status::kCompleted;
  error_message_ = error_message;
  tracing_manager_->OnRequestStatusChanged(this);
}

void AshTracingRequest::StorePendingFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  if (status_ != Status::kPendingMount)
    return;

  if (!user_logged_in_)
    return;

  base::FilePath user_downloads_folder = GetUserDownloadsFolder();
  if (user_downloads_folder.empty()) {
    error_message_ = "No user Downloads folder.";
    status_ = Status::kCompleted;
    trace_destination_->Done();
    tracing_manager_->OnRequestStatusChanged(this);
    return;
  }

  status_ = Status::kWritingFile;
  tracing_manager_->OnRequestStatusChanged(this);

  DCHECK(trace_destination_->CanWriteFile());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ExportDataOnThreadPool, GetPlatformFile(),
                     GetUserDownloadsFolder(), timestamp_),
      base::BindOnce(
          [](base::WeakPtr<AshTracingRequest> self,
             ExportStatus export_status) {
            if (!self)
              return;

            self->OnPendingFileStored(std::move(export_status.destination),
                                      export_status.success,
                                      export_status.error_message);
          },
          weak_factory_.GetWeakPtr()));
}

}  // namespace hud_display
}  // namespace ash
