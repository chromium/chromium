// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/important_file_writer.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/critical_closure.h"
#include "base/cxx17_backports.h"
#include "base/debug/alias.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer_cleaner.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace base {

namespace {

constexpr auto kDefaultCommitInterval = TimeDelta::FromSeconds(10);
#if defined(OS_WIN)
// This is how many times we will retry ReplaceFile on Windows.
constexpr int kReplaceRetries = 5;
// This is the result code recorded if ReplaceFile still fails.
// It should stay constant even if we change kReplaceRetries.
constexpr int kReplaceRetryFailure = 10;
static_assert(kReplaceRetryFailure > kReplaceRetries, "No overlap allowed");
constexpr auto kReplacePauseInterval = TimeDelta::FromMilliseconds(100);
#endif

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum TempFileFailure {
  FAILED_CREATING = 0,
  // FAILED_OPENING = 1,
  // FAILED_CLOSING = 2,
  FAILED_WRITING = 3,
  FAILED_RENAMING = 4,
  FAILED_FLUSHING = 5,
  TEMP_FILE_FAILURE_MAX
};

// Helper function to write samples to a histogram with a dynamically assigned
// histogram name.  Works with different error code types convertible to int
// which is the actual argument type of UmaHistogramExactLinear.
template <typename SampleType>
void UmaHistogramExactLinearWithSuffix(const char* histogram_name,
                                       StringPiece histogram_suffix,
                                       SampleType add_sample,
                                       SampleType max_sample) {
  static_assert(std::is_convertible<SampleType, int>::value,
                "SampleType should be convertible to int");
  DCHECK(histogram_name);
  std::string histogram_full_name(histogram_name);
  if (!histogram_suffix.empty()) {
    histogram_full_name.append(".");
    histogram_full_name.append(histogram_suffix.data(),
                               histogram_suffix.length());
  }
  UmaHistogramExactLinear(histogram_full_name, static_cast<int>(add_sample),
                          static_cast<int>(max_sample));
}

void UmaHistogramTimesWithSuffix(const char* histogram_name,
                                 StringPiece histogram_suffix,
                                 base::TimeDelta sample) {
  DCHECK(histogram_name);
  std::string histogram_full_name(histogram_name);
  if (!histogram_suffix.empty()) {
    histogram_full_name.append(".");
    histogram_full_name.append(histogram_suffix.data(),
                               histogram_suffix.length());
  }
  UmaHistogramTimes(histogram_full_name, sample);
}

void LogFailure(const FilePath& path,
                StringPiece histogram_suffix,
                TempFileFailure failure_code,
                StringPiece message) {
  UmaHistogramExactLinearWithSuffix("ImportantFile.TempFileFailures",
                                    histogram_suffix, failure_code,
                                    TEMP_FILE_FAILURE_MAX);
  DPLOG(WARNING) << "temp file failure: " << path.value() << " : " << message;
}

// Deletes the file named |tmp_file_path| (which may be open as |tmp_file|),
// retrying on the same sequence after some delay in case of error. It is sadly
// common that third-party software on Windows may open the temp file and map it
// into its own address space, which prevents others from marking it for
// deletion (even if opening it for deletion was possible). |histogram_suffix|
// is a (possibly empty) suffix for metrics. |attempt| is the number of failed
// previous attempts to the delete the file (defaults to 0).
void DeleteTmpFileWithRetry(File tmp_file,
                            const FilePath& tmp_file_path,
                            StringPiece histogram_suffix,
                            int attempt = 0) {
#if defined(OS_WIN)
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

  // Retry every 250ms for up to two seconds. These values were pulled out of
  // thin air, and may be adjusted in the future based on the metrics collected.
  static constexpr int kMaxDeleteAttempts = 8;
  static constexpr TimeDelta kDeleteFileRetryDelay =
      TimeDelta::FromMilliseconds(250);

  if (!DeleteFile(tmp_file_path)) {
    const auto last_file_error = File::GetLastFileError();
    if (++attempt >= kMaxDeleteAttempts) {
      // All retries have been exhausted; record the final error.
      UmaHistogramExactLinearWithSuffix(
          "ImportantFile.FileDeleteRetryExceededError", histogram_suffix,
          -last_file_error, -File::FILE_ERROR_MAX);
    } else if (!SequencedTaskRunnerHandle::IsSet() ||
               !SequencedTaskRunnerHandle::Get()->PostDelayedTask(
                   FROM_HERE,
                   BindOnce(&DeleteTmpFileWithRetry, base::File(),
                            tmp_file_path,
                            // Pass the suffix as a std::string rather than a
                            // StringPiece since the latter references memory
                            // owned by this function's caller.
                            std::string(histogram_suffix), attempt),
                   kDeleteFileRetryDelay)) {
      // Retries are not possible, so record the simple delete error.
      UmaHistogramExactLinearWithSuffix("ImportantFile.FileDeleteNoRetryError",
                                        histogram_suffix, -last_file_error,
                                        -File::FILE_ERROR_MAX);
    }
  } else if (attempt) {
    // Record the number of attempts to reach success only if more than one was
    // needed.
    UmaHistogramExactLinearWithSuffix(
        "ImportantFile.FileDeleteRetrySuccessCount", histogram_suffix, attempt,
        kMaxDeleteAttempts);
  }
}

}  // namespace

// static
bool ImportantFileWriter::WriteFileAtomically(const FilePath& path,
                                              StringPiece data,
                                              StringPiece histogram_suffix) {
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
  std::string data;
  if (!std::move(data_producer_for_background_sequence).Run(&data)) {
    DLOG(WARNING) << "Failed to serialize data to be saved in " << path.value();
    return;
  }

  if (!before_write_callback.is_null())
    std::move(before_write_callback).Run();

  // Calling the impl by way of the private
  // ProduceAndWriteStringToFileAtomically, which originated from an
  // ImportantFileWriter instance, so |from_instance| is true.
  const bool result = WriteFileAtomicallyImpl(path, data, histogram_suffix,
                                              /*from_instance=*/true);

  if (!after_write_callback.is_null())
    std::move(after_write_callback).Run(result);
}

// static
bool ImportantFileWriter::WriteFileAtomicallyImpl(const FilePath& path,
                                                  StringPiece data,
                                                  StringPiece histogram_suffix,
                                                  bool from_instance) {
  if (!from_instance)
    ImportantFileWriterCleaner::AddDirectory(path.DirName());

#if defined(OS_WIN) && DCHECK_IS_ON()
  // In https://crbug.com/920174, we have cases where CreateTemporaryFileInDir
  // hits a DCHECK because creation fails with no indication why. Pull the path
  // onto the stack so that we can see if it is malformed in some odd way.
  wchar_t path_copy[MAX_PATH];
  base::wcslcpy(path_copy, path.value().c_str(), base::size(path_copy));
  base::debug::Alias(path_copy);
#endif  // defined(OS_WIN) && DCHECK_IS_ON()

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
  strlcpy(file_info.path, path.value().c_str(), base::size(file_info.path));
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
    UmaHistogramExactLinearWithSuffix(
        "ImportantFile.FileCreateError", histogram_suffix,
        -tmp_file.error_details(), -File::FILE_ERROR_MAX);
    LogFailure(path, histogram_suffix, FAILED_CREATING,
               "could not create temporary file");
    return false;
  }

  // Don't write all of the data at once because this can lead to kernel
  // address-space exhaustion on 32-bit Windows (see https://crbug.com/1001022
  // for details).
  constexpr ptrdiff_t kMaxWriteAmount = 8 * 1024 * 1024;
  int bytes_written = 0;
  for (const char *scan = data.data(), *const end = scan + data.length();
       scan < end; scan += bytes_written) {
    const int write_amount = std::min(kMaxWriteAmount, end - scan);
    bytes_written = tmp_file.WriteAtCurrentPos(scan, write_amount);
    if (bytes_written != write_amount) {
      UmaHistogramExactLinearWithSuffix(
          "ImportantFile.FileWriteError", histogram_suffix,
          -File::GetLastFileError(), -File::FILE_ERROR_MAX);
      LogFailure(
          path, histogram_suffix, FAILED_WRITING,
          "error writing, bytes_written=" + NumberToString(bytes_written));
      DeleteTmpFileWithRetry(std::move(tmp_file), tmp_file_path,
                             histogram_suffix);
      return false;
    }
  }

  if (!tmp_file.Flush()) {
    LogFailure(path, histogram_suffix, FAILED_FLUSHING, "error flushing");
    DeleteTmpFileWithRetry(std::move(tmp_file), tmp_file_path,
                           histogram_suffix);
    return false;
  }

  File::Error replace_file_error = File::FILE_OK;

  // The file must be closed for ReplaceFile to do its job, which opens up a
  // race with other software that may open the temp file (e.g., an A/V scanner
  // doing its job without oplocks). Boost a background thread's priority on
  // Windows and close as late as possible to improve the chances that the other
  // software will lose the race.
#if defined(OS_WIN)
  const auto previous_priority = PlatformThread::GetCurrentThreadPriority();
  const bool reset_priority = previous_priority <= ThreadPriority::NORMAL;
  if (reset_priority)
    PlatformThread::SetCurrentThreadPriority(ThreadPriority::DISPLAY);
#endif  // defined(OS_WIN)
  tmp_file.Close();
  bool result = ReplaceFile(tmp_file_path, path, &replace_file_error);
#if defined(OS_WIN)
  // Save and restore the last error code so that it's not polluted by the
  // thread priority change.
  auto last_error = ::GetLastError();
  int retry_count = 0;
  for (/**/; !result && retry_count < kReplaceRetries; ++retry_count) {
    // The race condition between closing the temporary file and moving it gets
    // hit on a regular basis on some systems (https://crbug.com/1099284), so
    // we retry a few times before giving up.
    PlatformThread::Sleep(kReplacePauseInterval);
    result = ReplaceFile(tmp_file_path, path, &replace_file_error);
    last_error = ::GetLastError();
  }
  if (reset_priority)
    PlatformThread::SetCurrentThreadPriority(previous_priority);

  // Log how many times we had to retry the ReplaceFile operation before it
  // succeeded. If we never succeeded then return a special value.
  if (!result)
    retry_count = kReplaceRetryFailure;
  UmaHistogramExactLinear("ImportantFile.FileReplaceRetryCount", retry_count,
                          kReplaceRetryFailure);
#endif  // defined(OS_WIN)

  if (!result) {
    UmaHistogramExactLinearWithSuffix("ImportantFile.FileRenameError",
                                      histogram_suffix, -replace_file_error,
                                      -File::FILE_ERROR_MAX);
#if defined(OS_WIN)
    // Restore the error code from ReplaceFile so that it will be available for
    // LogFailure, otherwise failures in SetCurrrentThreadPriority may be
    // reported instead.
    ::SetLastError(last_error);
#endif
    LogFailure(path, histogram_suffix, FAILED_RENAMING,
               "could not rename temporary file");
    DeleteTmpFileWithRetry(File(), tmp_file_path, histogram_suffix);
  }

  return result;
}

ImportantFileWriter::ImportantFileWriter(
    const FilePath& path,
    scoped_refptr<SequencedTaskRunner> task_runner,
    StringPiece histogram_suffix)
    : ImportantFileWriter(path,
                          std::move(task_runner),
                          kDefaultCommitInterval,
                          histogram_suffix) {}

ImportantFileWriter::ImportantFileWriter(
    const FilePath& path,
    scoped_refptr<SequencedTaskRunner> task_runner,
    TimeDelta interval,
    StringPiece histogram_suffix)
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

void ImportantFileWriter::WriteNow(std::unique_ptr<std::string> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsValueInRangeForNumericType<int32_t>(data->length())) {
    NOTREACHED();
    return;
  }

  WriteNowWithBackgroundDataProducer(base::BindOnce(
      [](std::string data, std::string* output) {
        *output = std::move(data);
        return true;
      },
      std::move(*data)));
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
                                         std::move(split_task.first)))) {
    // Posting the task to background message loop is not expected
    // to fail, but if it does, avoid losing data and just hit the disk
    // on the current thread.
    NOTREACHED();

    std::move(split_task.second).Run();
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
    std::string data;

    // Pre-allocate previously needed memory plus 1kB for potential growth of
    // data. Reduces the number of memory allocations to grow |data| step by
    // step from tiny to very large.
    data.reserve(previous_data_size_ + 1024);

    if (!absl::get<DataSerializer*>(serializer_)->SerializeData(&data)) {
      DLOG(WARNING) << "Failed to serialize data to be saved in "
                    << path_.value();
      ClearPendingWrite();
      return;
    }

    previous_data_size_ = data.size();
    data_producer_for_background_sequence = base::BindOnce(
        [](std::string data, std::string* result) {
          *result = std::move(data);
          return true;
        },
        std::move(data));
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
