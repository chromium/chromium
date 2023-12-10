// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tast_support/stack_sampling_recorder.h"

#include <sys/file.h>

#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "components/metrics/call_stacks/call_stack_profile_metrics_provider.h"
#include "third_party/cros_system_api/proto/stack_sampled_metrics_status/stack_sampled_metrics_status.pb.h"

namespace chromeos::tast_support {

namespace {
// The path to write to. Deliberately not using base::GetTempDir() here;
// the tast test needs to known the complete, exact path.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
constexpr char kDefaultFilePath[] = "/tmp/stack-sampling-data-lacros";
#else
constexpr char kDefaultFilePath[] = "/tmp/stack-sampling-data";
#endif

// Time between writes. This is very short because we are only using this as
// part of an integration test and the test doesn't want to have a large
// timeout.
constexpr base::TimeDelta kTimeBetweenWrites = base::Seconds(1);
}  // namespace

StackSamplingRecorder::StackSamplingRecorder()
    : file_path_(base::FilePath(kDefaultFilePath)) {}

StackSamplingRecorder::StackSamplingRecorder(base::FilePath file_path)
    : file_path_(std::move(file_path)) {}

void StackSamplingRecorder::Start() {
  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&StackSamplingRecorder::WriteFile, this),
      kTimeBetweenWrites);
}

StackSamplingRecorder::~StackSamplingRecorder() = default;

metrics::CallStackProfileMetricsProvider::ProcessThreadCount
StackSamplingRecorder::GetSuccessfullyCollectedCounts() const {
  return metrics::CallStackProfileMetricsProvider::
      GetSuccessfullyCollectedCounts();
}

void StackSamplingRecorder::WriteFile() {
  // Do most work in a helper function so that we can return early if there's
  // an error, but we still always post a retry task.
  WriteFileHelper();

  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&StackSamplingRecorder::WriteFile, this),
      kTimeBetweenWrites);
}

void StackSamplingRecorder::WriteFileHelper() {
  // Build up the protobuf we want to write before locking the file, to
  // minimize the time spent holding the lock.
  auto process_thread_count_map = GetSuccessfullyCollectedCounts();
  stack_sampled_metrics_status::StackSampledMetricsStatus status;

  for (const auto& [process, thread_count_map] : process_thread_count_map) {
    auto& thread_count_map_proto =
        (*status.mutable_process_type_to_thread_count_map())[process];
    thread_count_map_proto.mutable_thread_type_to_success_count()->insert(
        thread_count_map.begin(), thread_count_map.end());
  }

  // Don't use the open flags that truncate the file; we can't truncate until
  // the lock succeeds.
  base::File file(file_path_,
                  base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);

  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to open " << file_path_ << ": "
               << base::File::ErrorToString(file.error_details());
    return;
  }

  // Use flock instead of File::Lock; File::Lock doesn't allow
  // block-until-locked.
  if (HANDLE_EINTR(flock(file.GetPlatformFile(), LOCK_EX)) != 0) {
    PLOG(ERROR) << "Unable to lock " << file_path_ << ": ";
    return;
  }

  // We don't need to unlock the file explicitly; it will always unlock when
  // we destruct |file|.
  if (!file.SetLength(0)) {
    LOG(ERROR) << "Unable to truncate " << file_path_;
    return;
  }

  if (!status.SerializeToFileDescriptor(file.GetPlatformFile())) {
    LOG(ERROR) << "Unable to write to " << file_path_;
    return;
  }
}

}  // namespace chromeos::tast_support
