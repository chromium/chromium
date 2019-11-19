// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/util/memory_pressure/system_memory_pressure_evaluator_chromeos.h"

#include <fcntl.h>
#include <sys/poll.h>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/process_metrics.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"

namespace util {
namespace chromeos {

namespace {
// Pointer to the SystemMemoryPressureEvaluator used by TabManagerDelegate for
// chromeos to need to call into ScheduleEarlyCheck.
SystemMemoryPressureEvaluator* g_system_evaluator = nullptr;

// We try not to re-notify on moderate too frequently, this time
// controls how frequently we will notify after our first notification.
constexpr base::TimeDelta kModerateMemoryPressureCooldownTime =
    base::TimeDelta::FromSeconds(10);

// The margin mem file contains the two memory levels, the first is the
// critical level and the second is the moderate level. Note, this
// file may contain more values but only the first two are used for
// memory pressure notifications in chromeos.
constexpr char kMarginMemFile[] = "/sys/kernel/mm/chromeos-low_mem/margin";

// The available memory file contains the available memory as determined
// by the kernel.
constexpr char kAvailableMemFile[] =
    "/sys/kernel/mm/chromeos-low_mem/available";

// Converts an available memory value in MB to a memory pressure level.
base::MemoryPressureListener::MemoryPressureLevel
GetMemoryPressureLevelFromAvailable(int available_mb,
                                    int moderate_avail_mb,
                                    int critical_avail_mb) {
  if (available_mb < critical_avail_mb)
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
  if (available_mb < moderate_avail_mb)
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;

  return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
}

int64_t ReadAvailableMemoryMB(int available_fd) {
  // Read the available memory.
  char buf[32] = {};

  // kernfs/file.c:
  // "Once poll/select indicates that the value has changed, you
  // need to close and re-open the file, or seek to 0 and read again.
  ssize_t bytes_read = HANDLE_EINTR(pread(available_fd, buf, sizeof(buf), 0));
  PCHECK(bytes_read != -1);

  std::string mem_str(buf, bytes_read);
  int64_t available = -1;
  CHECK(base::StringToInt64(
      base::TrimWhitespaceASCII(mem_str, base::TrimPositions::TRIM_ALL),
      &available));

  return available;
}

// This function will wait until the /sys/kernel/mm/chromeos-low_mem/available
// file becomes readable and then read the latest value. This file will only
// become readable once the available memory cross through one of the margin
// values specified in /sys/kernel/mm/chromeos-low_mem/margin, for more
// details see https://crrev.com/c/536336.
bool WaitForMemoryPressureChanges(int available_fd) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  pollfd pfd = {available_fd, POLLPRI | POLLERR, 0};
  int res = HANDLE_EINTR(poll(&pfd, 1, -1));  // Wait indefinitely.
  PCHECK(res != -1);

  if (pfd.revents != (POLLPRI | POLLERR)) {
    // If we didn't receive POLLPRI | POLLERR it means we likely received
    // POLLNVAL because the fd has been closed we will only log an error in
    // other situations.
    LOG_IF(ERROR, pfd.revents != POLLNVAL)
        << "WaitForMemoryPressureChanges received unexpected revents: "
        << pfd.revents;

    // We no longer want to wait for a kernel notification if the fd has been
    // closed.
    return false;
  }

  return true;
}

}  // namespace

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator(
    std::unique_ptr<MemoryPressureVoter> voter)
    : SystemMemoryPressureEvaluator(
          kMarginMemFile,
          kAvailableMemFile,
          base::BindRepeating(&WaitForMemoryPressureChanges),
          /*enable_metrics=*/true,
          std::move(voter)) {}

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator(
    const std::string& margin_file,
    const std::string& available_file,
    base::RepeatingCallback<bool(int)> kernel_waiting_callback,
    bool enable_metrics,
    std::unique_ptr<MemoryPressureVoter> voter)
    : util::SystemMemoryPressureEvaluator(std::move(voter)),
      available_mem_file_(HANDLE_EINTR(open(available_file.c_str(), O_RDONLY))),
      kernel_waiting_callback_(
          base::BindRepeating(std::move(kernel_waiting_callback),
                              available_mem_file_.get())),
      weak_ptr_factory_(this) {
  DCHECK(g_system_evaluator == nullptr);
  g_system_evaluator = this;

  CHECK(available_mem_file_.is_valid());
  std::vector<int> margin_parts =
      SystemMemoryPressureEvaluator::GetMarginFileParts(margin_file);

  // This class SHOULD have verified kernel support by calling
  // SupportsKernelNotifications() before creating a new instance of this.
  // Therefore we will check fail if we don't have multiple margin values.
  CHECK_LE(2u, margin_parts.size());
  critical_pressure_threshold_mb_ = margin_parts[0];
  moderate_pressure_threshold_mb_ = margin_parts[1];

  if (enable_metrics) {
    // We will report the current memory pressure at some periodic interval,
    // the metric ChromeOS.MemoryPRessureLevel is currently reported every 1s.
    reporting_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(1),
        base::BindRepeating(&SystemMemoryPressureEvaluator::
                                CheckMemoryPressureAndRecordStatistics,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  ScheduleWaitForKernelNotification();
}
SystemMemoryPressureEvaluator::~SystemMemoryPressureEvaluator() {
  DCHECK(g_system_evaluator);
  g_system_evaluator = nullptr;
}

// static
SystemMemoryPressureEvaluator* SystemMemoryPressureEvaluator::Get() {
  return g_system_evaluator;
}

std::vector<int> SystemMemoryPressureEvaluator::GetMarginFileParts() {
  static const base::NoDestructor<std::vector<int>> margin_file_parts(
      GetMarginFileParts(kMarginMemFile));
  return *margin_file_parts;
}

std::vector<int> SystemMemoryPressureEvaluator::GetMarginFileParts(
    const std::string& file) {
  std::vector<int> margin_values;
  std::string margin_contents;
  if (base::ReadFileToString(base::FilePath(file), &margin_contents)) {
    std::vector<std::string> margins =
        base::SplitString(margin_contents, base::kWhitespaceASCII,
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const auto& v : margins) {
      int value = -1;
      if (!base::StringToInt(v, &value)) {
        // If any of the values weren't parseable as an int we return
        // nothing as the file format is unexpected.
        LOG(ERROR) << "Unable to parse margin file contents as integer: " << v;
        return std::vector<int>();
      }
      margin_values.push_back(value);
    }
  } else {
    LOG(ERROR) << "Unable to read margin file: " << kMarginMemFile;
  }
  return margin_values;
}

bool SystemMemoryPressureEvaluator::SupportsKernelNotifications() {
  // Unfortunately at the moment the only way to determine if the chromeos
  // kernel supports polling on the available file is to observe two values
  // in the margin file, if the critical and moderate levels are specified
  // there then we know the kernel must support polling on available.
  return SystemMemoryPressureEvaluator::GetMarginFileParts().size() >= 2;
}

// CheckMemoryPressure will get the current memory pressure level by reading
// the available file.
void SystemMemoryPressureEvaluator::CheckMemoryPressure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto old_vote = current_vote();
  int64_t mem_avail = ReadAvailableMemoryMB(available_mem_file_.get());
  SetCurrentVote(GetMemoryPressureLevelFromAvailable(
      mem_avail, moderate_pressure_threshold_mb_,
      critical_pressure_threshold_mb_));
  bool notify = true;

  if (current_vote() ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    last_moderate_notification_ = base::TimeTicks();
    notify = false;
  }

  // In the case of MODERATE memory pressure we may be in this state for quite
  // some time so we limit the rate at which we dispatch notifications.
  else if (current_vote() ==
           base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE) {
    if (old_vote == current_vote()) {
      if (base::TimeTicks::Now() - last_moderate_notification_ <
          kModerateMemoryPressureCooldownTime) {
        notify = false;
      } else if (old_vote ==
                 base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
        // Reset the moderate notification time if we just crossed back.
        last_moderate_notification_ = base::TimeTicks::Now();
        notify = false;
      }
    }

    if (notify)
      last_moderate_notification_ = base::TimeTicks::Now();
  }

  VLOG(1) << "SystemMemoryPressureEvaluator::CheckMemoryPressure dispatching "
             "at level: "
          << current_vote();
  SendCurrentVote(notify);
}

void SystemMemoryPressureEvaluator::HandleKernelNotification(bool result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If WaitForKernelNotification returned false then the FD has been closed and
  // we just exit without waiting again.
  if (!result) {
    return;
  }

  CheckMemoryPressure();

  // Now we need to schedule back our blocking task to wait for more
  // kernel notifications.
  ScheduleWaitForKernelNotification();
}

void SystemMemoryPressureEvaluator::CheckMemoryPressureAndRecordStatistics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note: If we support notifications of memory pressure changes in both
  // directions we will not have to update the cached value as it will always
  // be correct.
  CheckMemoryPressure();

  // Record UMA histogram statistics for the current memory pressure level, it
  // would seem that only Memory.PressureLevel would be necessary.
  constexpr int kNumberPressureLevels = 3;
  UMA_HISTOGRAM_ENUMERATION("ChromeOS.MemoryPressureLevel", current_vote(),
                            kNumberPressureLevels);
}

void SystemMemoryPressureEvaluator::ScheduleEarlyCheck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&SystemMemoryPressureEvaluator::CheckMemoryPressure,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SystemMemoryPressureEvaluator::ScheduleWaitForKernelNotification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      kernel_waiting_callback_,
      base::BindRepeating(
          &SystemMemoryPressureEvaluator::HandleKernelNotification,
          weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace chromeos
}  // namespace util
