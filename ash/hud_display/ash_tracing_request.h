// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_ASH_TRACING_REQUEST_H_
#define ASH_HUD_DISPLAY_ASH_TRACING_REQUEST_H_

#include <sys/stat.h>

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/platform_file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

#include "ash/ash_export.h"

namespace ash {
namespace hud_display {

class AshTracingManager;
class AshTracingHandler;

// This is needed for testing to override File IO.
class ASH_EXPORT AshTraceDestinationIO {
 public:
  virtual ~AshTraceDestinationIO();

  // Overrides base::CreateDirectory.
  virtual bool CreateDirectory(const base::FilePath& path) = 0;

  // Overrides base::File::File(). Returns pair {File file, bool success}.
  // Test implementation may return success with invalid file.
  virtual std::tuple<base::File, bool> CreateTracingFile(
      const base::FilePath& path) = 0;

  // Implements memfd_create(2). Returns pair {int fd, bool success}.
  // Test implementation may return success with invalid fd.
  virtual std::tuple<base::PlatformFile, bool> CreateMemFD(
      const char* name,
      unsigned int flags) = 0;

  // Takes GetPlatformFile() from AshTraceDestination and returns true if
  // given fd is valid for storing traces. Checks for -1 in regular case,
  // and checks internal status in tests.
  virtual bool CanWriteFile(base::PlatformFile fd) = 0;

  virtual int fstat(base::PlatformFile fd, struct stat* statbuf) = 0;

  virtual ssize_t sendfile(base::PlatformFile out_fd,
                           base::PlatformFile in_fd,
                           off_t* offset,
                           size_t size) = 0;
};

class ASH_EXPORT AshTraceDestination {
 public:
  AshTraceDestination();
  AshTraceDestination(std::unique_ptr<AshTraceDestinationIO> io,
                      const base::FilePath& path,
                      base::File&& file,
                      base::PlatformFile memfd);

  AshTraceDestination(const AshTraceDestination&) = delete;
  AshTraceDestination& operator=(const AshTraceDestination&) = delete;

  ~AshTraceDestination();

  const base::FilePath& path() const { return path_; }

  // Returns PlatformFile for storing trace.
  // Can be memfd or file depending on the setup.
  base::PlatformFile GetPlatformFile() const;

  // Reurns true if GetPlatformFile() will return valid file descriptor.
  // In tests when test IO layer is used returns true if test IO layer will
  // succeed saving file.
  bool CanWriteFile() const;

  // Close all files.
  void Done();

  AshTraceDestinationIO* io() const { return io_.get(); }

 private:
  std::unique_ptr<AshTraceDestinationIO> io_;

  base::FilePath path_;
  base::File file_;
  base::PlatformFile memfd_ = base::kInvalidPlatformFile;
};

class AshTracingRequest {
 public:
  enum class Status {
    kEmpty,         // Object created.
    kInitialized,   // File data is initialized
    kStarted,       // Tracing is in progress.
    kStopping,      // Tracing is being stopped.
    kPendingMount,  // Tracing is complete, waiting for home directory mount.
    kWritingFile,   // Writing trace file from memory to file after user login.
    kCompleted,     // Trace file is written. Object has valid path.
  };

  // Will start tracing (asynchronously).
  explicit AshTracingRequest(AshTracingManager* tracing_manager);
  AshTracingRequest(const AshTracingRequest&) = delete;
  AshTracingRequest& operator=(const AshTracingRequest&) = delete;

  ~AshTracingRequest();

  void Stop();

  // Receive notifications from AshTracingHandler.
  void OnTracingStarted();
  void OnTracingFinished();

  // Will trigger trace file write if needed.
  void OnUserLoggedIn();
  // Returns file descriptor that will actually be used for tracing.
  base::PlatformFile GetPlatformFile() const;

  Status status() const { return status_; }
  const std::string& error_message() const { return error_message_; }

  // Tests generate specific fake IO.
  static ASH_EXPORT void SetAshTraceDestinationIOCreatorForTesting(
      std::unique_ptr<AshTraceDestinationIO> (*creator)(void));
  static ASH_EXPORT void ResetAshTraceDestinationIOCreatorForTesting();

  // Tests explicitly check AshTraceDestination behavior and they need to
  // be able to generate ThreadPool tasks to crete AshTraceDestination.
  // So this function will return a task that can be sent to IO-enabled
  // sequence runner to create AshTraceDestination.
  using AshTraceDestinationUniquePtr = std::unique_ptr<AshTraceDestination>;
  using GenerateTraceDestinationTask =
      base::OnceCallback<AshTraceDestinationUniquePtr(void)>;
  ASH_EXPORT static GenerateTraceDestinationTask
  CreateGenerateTraceDestinationTaskForTesting(
      std::unique_ptr<AshTraceDestinationIO> io,
      base::Time timestamp);

  ASH_EXPORT const AshTraceDestination* GetTraceDestinationForTesting() const;

 private:
  // Starts tracing after `destination` was initialized on the ThreadPool.
  void OnTraceDestinationInitialized(
      std::unique_ptr<AshTraceDestination> destination);

  // Marks file export operation completed.
  void OnPendingFileStored(std::unique_ptr<AshTraceDestination> destination,
                           bool success,
                           std::string error_message);

  // Stores memory trace file to permanent location.
  void StorePendingFile();

  // Trace status
  Status status_ = Status::kEmpty;

  // When trace was started.
  const base::Time timestamp_;

  bool user_logged_in_ = false;

  raw_ptr<AshTracingManager> tracing_manager_;

  // This object is deleted once tracing is stopped.
  std::unique_ptr<AshTracingHandler> tracing_handler_;

  // Non-empty if error has occurred.
  std::string error_message_;

  std::unique_ptr<AshTraceDestination> trace_destination_;

  SEQUENCE_CHECKER(my_sequence_checker_);

  base::WeakPtrFactory<AshTracingRequest> weak_factory_{this};
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_ASH_TRACING_REQUEST_H_
