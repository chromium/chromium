// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/files/important_file_writer.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/critical_closure.h"
#include "base/debug/alias.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer_cleaner.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace base {

namespace {

constexpr auto kDefaultCommitInterval = Seconds(10);
#if BUILDFLAG(IS_WIN)
// This is how many times we will retry ReplaceFile on Windows.
constexpr int kReplaceRetries = 5;
// This is the result code recorded if ReplaceFile still fails.
// It should stay constant even if we change kReplaceRetries.
constexpr int kReplaceRetryFailure = 10;
static_assert(kReplaceRetryFailure > kReplaceRetries, "No overlap allowed");
constexpr auto kReplacePauseInterval = Milliseconds(100);
#endif

void UmaHistogramTimesWithSuffix(const char* histogram_name,
                                 std::string_view histogram_suffix,
                                 base::TimeDelta sample) {
  DCHECK(histogram_name);
  std::string histogram_full_name(histogram_name);
  if (!histogram_suffix.empty()) {
    histogram_full_name.append(".");
    histogram_full_name.append(histogram_suffix);
  }
  UmaHistogramTimes(histogram_full_name, sample);
}

// Deletes the file named |tmp_file_path| (which may be open as |tmp_file|),
// retrying on the same sequence after some delay in case of error. It is sadly
// common that third-party software on Windows may open the temp file and map it
// into its own address space, which prevents others from marking it for
// deletion (even if opening it for deletion was possible). |attempt| is the
// number of failed previous attempts to the delete the file (defaults to 0).
void DeleteTmpFileWithRetry(File tmp_file,
                            const FilePath& tmp_file_path,
                            int attempt = 0) {
#if BUILDFLAG(IS_WIN)
  // Mark the file for deletion when it is closed and then close it implicitly.
  if (tmp_file.IsValid()) {
    if (tmp_file.DeleteOnClose(true))
      return;
    // The file was opened with exclusive r/w access, so failures are primarily
    // due to I/O errors or other phenomena out of the process's control. Go
    // ahead and close the file. The call to DeleteFile below will basically
    // repeat the above, but maybe it will somehow succeed.
    tmp_file.Close();
  }
#endif

  // Retry every 250ms for up to two seconds. Metrics indicate that this is a
  // reasonable number of retries -- the failures after all attempts generally
  // point to access denied. The ImportantFileWriterCleaner should clean these
  // up in the next process.
  constexpr int kMaxDeleteAttempts = 8;
  constexpr TimeDelta kDeleteFileRetryDelay = Milliseconds(250);

  if (!DeleteFile(tmp_file_path) && ++attempt < kMaxDeleteAttempts &&
      SequencedTaskRunner::HasCurrentDefault()) {
    SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        BindOnce(&DeleteTmpFileWithRetry, base::File(), tmp_file_path, attempt),
        kDeleteFileRetryDelay);
  }
}

}  // namespace

// static
bool ImportantFileWriter::WriteFileAtomically(
    const FilePath& path,
    std::string_view data,
    std::string_view histogram_suffix) {
  // Calling the impl by way of the public WriteFileAtomically, so
  // |from_instance| is false.
  return WriteFileAtomicallyImpl(path, data, histogram_suffix,
                                 /*from_instance=*/false);
}

// static
void ImportantFileWriter::ProduceAndWriteStringToFileAtomically(
    const FilePath& path,
    BackgroundDataProducerCallback data_producer_for_background_sequence,
    OnceClosure before_write_callback,
    OnceCallback<void(bool success)> after_write_callback,
    const std::string& histogram_suffix) {
  // Produce the actual data string on the background sequence.
  std::optional<std::string> data =
      std::move(data_producer_for_background_sequence).Run();
  if (!data) {
    DLOG(WARNING) << "Failed to serialize data to be saved in " << path.value();
    return;
  }

  if (!before_write_callback.is_null())
    std::move(before_write_callback).Run();

  // Calling the impl by way of the private
  // ProduceAndWriteStringToFileAtomically, which originated from an
  // ImportantFileWriter instance, so |from_instance| is true.
  const bool result = WriteFileAtomicallyImpl(path, *data, histogram_suffix,
                                              /*from_instance=*/true);

  if (!after_write_callback.is_null())
    std::move(after_write_callback).Run(result);
}

// static
bool ImportantFileWriter::WriteFileAtomicallyImpl(
    const FilePath& path,
    std::string_view data,
    std::string_view histogram_suffix,
    bool from_instance) {
  const TimeTicks write_start = TimeTicks::Now();
  if (!from_instance)
    ImportantFileWriterCleaner::AddDirectory(path.DirName());

#if BUILDFLAG(IS_WIN) && DCHECK_IS_ON()
  // In https://crbug.com/920174, we have cases where CreateTemporaryFileInDir
  // hits a DCHECK because creation fails with no indication why. Pull the path
  // onto the stack so that we can see if it is malformed in some odd way.
  wchar_t path_copy[MAX_PATH];
  base::wcslcpy(path_copy, path.value().c_str(), std::size(path_copy));
  base::debug::Alias(path_copy);
#endif  // BUILDFLAG(IS_WIN) && DCHECK_IS_ON()

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS, chrome gets killed when it cannot finish shutdown quickly,
  // and this function seems to be one of the slowest shutdown steps.
  // Include some info to the report for investigation. crbug.com/418627
  // TODO(hashimoto): Remove this.
  struct {
    size_t data_size;
    char path[128];
  } file_info;
  file_info.data_size = data.size();
  strlcpy(file_info.path, path.value().c_str(), std::size(file_info.path));
  debug::Alias(&file_info);
#endif

  // Write the data to a temp file then rename to avoid data loss if we crash
  // while writing the file. Ensure that the temp file is on the same volume
  // as target file, so it can be moved in one step, and that the temp file
  // is securely created.
  FilePath tmp_file_path;
  File tmp_file =
      CreateAndOpenTemporaryFileInDir(path.DirName(), &tmp_file_path);
  if (!tmp_file.IsValid()) {
    DPLOG(WARNING) << "Failed to create temporary file to update " << path;
    return false;
  }

  // Don't write all of the data at once because this can lead to kernel
  // address-space exhaustion on 32-bit Windows (see https://crbug.com/1001022
  // for details).
  constexpr ptrdiff_t kMaxWriteAmount = 8 * 1024 * 1024;
  int bytes_written = 0;
  for (const char *scan = data.data(), *const end = scan + data.length();
       scan < end; scan += bytes_written) {
    const int write_amount =
        static_cast<int>(std::min(kMaxWriteAmount, end - scan));
    bytes_written = tmp_file.WriteAtCurrentPos(scan, write_amount);
    if (bytes_written != write_amount) {
      DPLOG(WARNING) << "Failed to write " << write_amount << " bytes to temp "
                     << "file to update " << path
                     << " (bytes_written=" << bytes_written << ")";
      DeleteTmpFileWithRetry(std::move(tmp_file), tmp_file_path);
      return false;
    }
  }

  if (!tmp_file.Flush()) {
    DPLOG(WARNING) << "Failed to flush temp file to update " << path;
    DeleteTmpFileWithRetry(std::move(tmp_file), tmp_file_path);
    return false;
  }

  File::Error replace_file_error = File::FILE_OK;
  bool result;

  // The file must be closed for ReplaceFile to do its job, which opens up a
  // race with other software that may open the temp file (e.g., an A/V scanner
  // doing its job without oplocks). Boost a background thread's priority on
  // Windows and close as late as possible to improve the chances that the other
  // software will lose the race.
#if BUILDFLAG(IS_WIN)
  DWORD last_error;
  int retry_count = 0;
  {
    ScopedBoostPriority scoped_boost_priority(ThreadType::kDisplayCritical);
    tmp_file.Close();
    result = ReplaceFile(tmp_file_path, path, &replace_file_error);
    // Save and restore the last error code so that it's not polluted by the
    // thread priority change.
    last_error = ::GetLastError();
    for (/**/; !result && retry_count < kReplaceRetries; ++retry_count) {
      // The race condition between closing the temporary file and moving it
      // gets hit on a regular basis on some systems
      // (https://crbug.com/1099284), so we retry a few times before giving up.
      PlatformThread::Sleep(kReplacePauseInterval);
      result = ReplaceFile(tmp_file_path, path, &replace_file_error);
      last_error = ::GetLastError();
    }
  }

  // Log how many times we had to retry the ReplaceFile operation before it
  // succeeded. If we never succeeded then return a special value.
  if (!result)
    retry_count = kReplaceRetryFailure;
  UmaHistogramExactLinear("ImportantFile.FileReplaceRetryCount", retry_count,
                          kReplaceRetryFailure);
#else
  tmp_file.Close();
  result = ReplaceFile(tmp_file_path, path, &replace_file_error);
#endif  // BUILDFLAG(IS_WIN)

  if (!result) {
#if BUILDFLAG(IS_WIN)
    // Restore the error code from ReplaceFile so that it will be available for
    // the log message, otherwise failures in SetCurrentThreadType may be
    // reported instead.
    ::SetLastError(last_error);
#endif
    DPLOG(WARNING) << "Failed to replace " << path << " with " << tmp_file_path;
    DeleteTmpFileWithRetry(File(), tmp_file_path);
  }

  const TimeDelta write_duration = TimeTicks::Now() - write_start;
  UmaHistogramTimesWithSuffix("ImportantFile.WriteDuration", histogram_suffix,
                              write_duration);

  return result;
}

ImportantFileWriter::ImportantFileWriter(
    const FilePath& path,
    scoped_refptr<SequencedTaskRunner> task_runner,
    std::string_view histogram_suffix)
    : ImportantFileWriter(path,
                          std::move(task_runner),
                          kDefaultCommitInterval,
                          histogram_suffix) {}

ImportantFileWriter::ImportantFileWriter(
    const FilePath& path,
    scoped_refptr<SequencedTaskRunner> task_runner,
    TimeDelta interval,
    std::string_view histogram_suffix)
    : path_(path),
      task_runner_(std::move(task_runner)),
      commit_interval_(interval),
      histogram_suffix_(histogram_suffix) {
  DCHECK(task_runner_);
  ImportantFileWriterCleaner::AddDirectory(path.DirName());
}

ImportantFileWriter::~ImportantFileWriter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We're usually a member variable of some other object, which also tends
  // to be our serializer. It may not be safe to call back to the parent object
  // being destructed.
  DCHECK(!HasPendingWrite());
}

bool ImportantFileWriter::HasPendingWrite() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return timer().IsRunning();
}

void ImportantFileWriter::WriteNow(std::string data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsValueInRangeForNumericType<int32_t>(data.length()));

  WriteNowWithBackgroundDataProducer(base::BindOnce(
      [](std::string data) { return std::make_optional(std::move(data)); },
      std::move(data)));
}

void ImportantFileWriter::WriteNowWithBackgroundDataProducer(
    BackgroundDataProducerCallback background_data_producer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto split_task = SplitOnceCallback(
      BindOnce(&ProduceAndWriteStringToFileAtomically, path_,
               std::move(background_data_producer),
               std::move(before_next_write_callback_),
               std::move(after_next_write_callback_), histogram_suffix_));

  if (!task_runner_->PostTask(
          FROM_HERE, MakeCriticalClosure("ImportantFileWriter::WriteNow",
                                         std::move(split_task.first),
                                         /*is_immediate=*/true))) {
    // Posting the task to background message loop is not expected
    // to fail.
    NOTREACHED();
  }
  ClearPendingWrite();
}

void ImportantFileWriter::ScheduleWrite(DataSerializer* serializer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(serializer);
  serializer_.emplace<DataSerializer*>(serializer);

  if (!timer().IsRunning()) {
    timer().Start(
        FROM_HERE, commit_interval_,
        BindOnce(&ImportantFileWriter::DoScheduledWrite, Unretained(this)));
  }
}

void ImportantFileWriter::ScheduleWriteWithBackgroundDataSerializer(
    BackgroundDataSerializer* serializer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(serializer);
  serializer_.emplace<BackgroundDataSerializer*>(serializer);

  if (!timer().IsRunning()) {
    timer().Start(
        FROM_HERE, commit_interval_,
        BindOnce(&ImportantFileWriter::DoScheduledWrite, Unretained(this)));
  }
}

void ImportantFileWriter::DoScheduledWrite() {
  // One of the serializers should be set.
  DCHECK(!absl::holds_alternative<absl::monostate>(serializer_));

  const TimeTicks serialization_start = TimeTicks::Now();
  BackgroundDataProducerCallback data_producer_for_background_sequence;

  if (absl::holds_alternative<DataSerializer*>(serializer_)) {
    std::optional<std::string> data;
    data = absl::get<DataSerializer*>(serializer_)->SerializeData();
    if (!data) {
      DLOG(WARNING) << "Failed to serialize data to be saved in "
                    << path_.value();
      ClearPendingWrite();
      return;
    }

    previous_data_size_ = data->size();
    data_producer_for_background_sequence = base::BindOnce(
        [](std::string data) { return std::make_optional(std::move(data)); },
        std::move(data).value());
  } else {
    data_producer_for_background_sequence =
        absl::get<BackgroundDataSerializer*>(serializer_)
            ->GetSerializedDataProducerForBackgroundSequence();

    DCHECK(data_producer_for_background_sequence);
  }

  const TimeDelta serialization_duration =
      TimeTicks::Now() - serialization_start;

  UmaHistogramTimesWithSuffix("ImportantFile.SerializationDuration",
                              histogram_suffix_, serialization_duration);

  WriteNowWithBackgroundDataProducer(
      std::move(data_producer_for_background_sequence));
  DCHECK(!HasPendingWrite());
}

void ImportantFileWriter::RegisterOnNextWriteCallbacks(
    OnceClosure before_next_write_callback,
    OnceCallback<void(bool success)> after_next_write_callback) {
  before_next_write_callback_ = std::move(before_next_write_callback);
  after_next_write_callback_ = std::move(after_next_write_callback);
}

void ImportantFileWriter::ClearPendingWrite() {
  timer().Stop();
  serializer_.emplace<absl::monostate>();
}

void ImportantFileWriter::SetTimerForTesting(OneShotTimer* timer_override) {
  timer_override_ = timer_override;
}

}  // namespace base
