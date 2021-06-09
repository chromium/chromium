// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/util/memory_pressure/system_memory_pressure_evaluator_win.h"

#include <windows.h>
#include <memory>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/single_thread_task_runner.h"
#include "base/system/sys_info.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "base/util/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "base/win/object_watcher.h"

namespace util {
namespace win {

namespace {

static const DWORDLONG kMBBytes = 1024 * 1024;

// Implements ObjectWatcher::Delegate by forwarding to a provided callback.
class MemoryPressureWatcherDelegate
    : public base::win::ObjectWatcher::Delegate {
 public:
  MemoryPressureWatcherDelegate(base::win::ScopedHandle handle,
                                base::OnceClosure callback);
  ~MemoryPressureWatcherDelegate() override;
  MemoryPressureWatcherDelegate(const MemoryPressureWatcherDelegate& other) =
      delete;
  MemoryPressureWatcherDelegate& operator=(
      const MemoryPressureWatcherDelegate&) = delete;

  void ReplaceWatchedHandleForTesting(base::win::ScopedHandle handle);
  void SetCallbackForTesting(base::OnceClosure callback) {
    callback_ = std::move(callback);
  }

 private:
  void OnObjectSignaled(HANDLE handle) override;

  base::win::ScopedHandle handle_;
  base::win::ObjectWatcher watcher_;
  base::OnceClosure callback_;
};

MemoryPressureWatcherDelegate::MemoryPressureWatcherDelegate(
    base::win::ScopedHandle handle,
    base::OnceClosure callback)
    : handle_(std::move(handle)), callback_(std::move(callback)) {
  DCHECK(handle_.IsValid());
  CHECK(watcher_.StartWatchingOnce(handle_.Get(), this));
}

MemoryPressureWatcherDelegate::~MemoryPressureWatcherDelegate() = default;

void MemoryPressureWatcherDelegate::ReplaceWatchedHandleForTesting(
    base::win::ScopedHandle handle) {
  if (watcher_.IsWatching())
    watcher_.StopWatching();
  handle_ = std::move(handle);
  CHECK(watcher_.StartWatchingOnce(handle_.Get(), this));
}

void MemoryPressureWatcherDelegate::OnObjectSignaled(HANDLE handle) {
  DCHECK_EQ(handle, handle_.Get());
  std::move(callback_).Run();
}

}  // namespace

// Check the amount of RAM left every 5 seconds.
const base::TimeDelta SystemMemoryPressureEvaluator::kMemorySamplingPeriod =
    base::TimeDelta::FromSeconds(5);

// The following constants have been lifted from similar values in the ChromeOS
// memory pressure monitor. The values were determined experimentally to ensure
// sufficient responsiveness of the memory pressure subsystem, and minimal
// overhead.
const base::TimeDelta SystemMemoryPressureEvaluator::kModeratePressureCooldown =
    base::TimeDelta::FromSeconds(10);

// TODO(chrisha): Explore the following constants further with an experiment.

// A system is considered 'high memory' if it has more than 1.5GB of system
// memory available for use by the memory manager (not reserved for hardware
// and drivers). This is a fuzzy version of the ~2GB discussed below.
const int SystemMemoryPressureEvaluator::kLargeMemoryThresholdMb = 1536;

// These are the default thresholds used for systems with < ~2GB of physical
// memory. Such systems have been observed to always maintain ~100MB of
// available memory, paging until that is the case. To try to avoid paging a
// threshold slightly above this is chosen. The moderate threshold is slightly
// less grounded in reality and chosen as 2.5x critical.
const int
    SystemMemoryPressureEvaluator::kSmallMemoryDefaultModerateThresholdMb = 500;
const int
    SystemMemoryPressureEvaluator::kSmallMemoryDefaultCriticalThresholdMb = 200;

// These are the default thresholds used for systems with >= ~2GB of physical
// memory. Such systems have been observed to always maintain ~300MB of
// available memory, paging until that is the case.
const int
    SystemMemoryPressureEvaluator::kLargeMemoryDefaultModerateThresholdMb =
        1000;
const int
    SystemMemoryPressureEvaluator::kLargeMemoryDefaultCriticalThresholdMb = 400;

// A memory pressure evaluator that receives memory pressure notifications from
// the OS and forwards them to the memory pressure monitor.
class SystemMemoryPressureEvaluator::OSSignalsMemoryPressureEvaluator {
 public:
  using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;

  explicit OSSignalsMemoryPressureEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter);
  ~OSSignalsMemoryPressureEvaluator();
  OSSignalsMemoryPressureEvaluator(
      const OSSignalsMemoryPressureEvaluator& other) = delete;
  OSSignalsMemoryPressureEvaluator& operator=(
      const OSSignalsMemoryPressureEvaluator&) = delete;

  // Creates the watcher used to receive the low and high memory notifications.
  void Start();

  MemoryPressureWatcherDelegate* GetWatcherForTesting() const {
    return memory_notification_watcher_.get();
  }
  void WaitForHighMemoryNotificationForTesting(base::OnceClosure closure);

 private:
  // Called when receiving a low/high memory notification.
  void OnLowMemoryNotification();
  void OnHighMemoryNotification();

  void StartLowMemoryNotificationWatcher();
  void StartHighMemoryNotificationWatcher();

  // The period of the critical pressure notification timer.
  static constexpr base::TimeDelta kHighPressureNotificationInterval =
      base::TimeDelta::FromSeconds(2);

  // The voter used to cast the votes.
  std::unique_ptr<MemoryPressureVoter> voter_;

  // The memory notification watcher.
  std::unique_ptr<MemoryPressureWatcherDelegate> memory_notification_watcher_;

  // Timer that will re-emit the critical memory pressure signal until the
  // memory gets high again.
  base::RepeatingTimer critical_pressure_notification_timer_;

  // Beginning of the critical memory pressure session.
  base::TimeTicks critical_pressure_session_begin_;

  // Ensures that this object is used from a single sequence.
  SEQUENCE_CHECKER(sequence_checker_);
};

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator(
    std::unique_ptr<MemoryPressureVoter> voter)
    : util::SystemMemoryPressureEvaluator(std::move(voter)),
      moderate_threshold_mb_(0),
      critical_threshold_mb_(0),
      moderate_pressure_repeat_count_(0) {
  InferThresholds();
  StartObserving();
}

SystemMemoryPressureEvaluator::SystemMemoryPressureEvaluator(
    int moderate_threshold_mb,
    int critical_threshold_mb,
    std::unique_ptr<MemoryPressureVoter> voter)
    : util::SystemMemoryPressureEvaluator(std::move(voter)),
      moderate_threshold_mb_(moderate_threshold_mb),
      critical_threshold_mb_(critical_threshold_mb),
      moderate_pressure_repeat_count_(0) {
  DCHECK_GE(moderate_threshold_mb_, critical_threshold_mb_);
  DCHECK_LE(0, critical_threshold_mb_);
  StartObserving();
}

SystemMemoryPressureEvaluator::~SystemMemoryPressureEvaluator() {
  StopObserving();
}

void SystemMemoryPressureEvaluator::CheckMemoryPressureSoon() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&SystemMemoryPressureEvaluator::CheckMemoryPressure,
                          weak_ptr_factory_.GetWeakPtr()));
}

void SystemMemoryPressureEvaluator::CreateOSSignalPressureEvaluator(
    std::unique_ptr<MemoryPressureVoter> voter) {
  os_signals_evaluator_ =
      std::make_unique<OSSignalsMemoryPressureEvaluator>(std::move(voter));
  os_signals_evaluator_->Start();
}

void SystemMemoryPressureEvaluator::ReplaceWatchedHandleForTesting(
    base::win::ScopedHandle handle) {
  os_signals_evaluator_->GetWatcherForTesting()->ReplaceWatchedHandleForTesting(
      std::move(handle));
}

void SystemMemoryPressureEvaluator::WaitForHighMemoryNotificationForTesting(
    base::OnceClosure closure) {
  os_signals_evaluator_->WaitForHighMemoryNotificationForTesting(
      std::move(closure));
}

void SystemMemoryPressureEvaluator::InferThresholds() {
  // Default to a 'high' memory situation, which uses more conservative
  // thresholds.
  bool high_memory = true;
  MEMORYSTATUSEX mem_status = {};
  if (GetSystemMemoryStatus(&mem_status)) {
    static const DWORDLONG kLargeMemoryThresholdBytes =
        static_cast<DWORDLONG>(kLargeMemoryThresholdMb) * kMBBytes;
    high_memory = mem_status.ullTotalPhys >= kLargeMemoryThresholdBytes;
  }

  if (high_memory) {
    moderate_threshold_mb_ = kLargeMemoryDefaultModerateThresholdMb;
    critical_threshold_mb_ = kLargeMemoryDefaultCriticalThresholdMb;
  } else {
    moderate_threshold_mb_ = kSmallMemoryDefaultModerateThresholdMb;
    critical_threshold_mb_ = kSmallMemoryDefaultCriticalThresholdMb;
  }
}

void SystemMemoryPressureEvaluator::StartObserving() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  timer_.Start(
      FROM_HERE, kMemorySamplingPeriod,
      BindRepeating(&SystemMemoryPressureEvaluator::CheckMemoryPressure,
                    weak_ptr_factory_.GetWeakPtr()));
}

void SystemMemoryPressureEvaluator::StopObserving() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If StartObserving failed, StopObserving will still get called.
  timer_.Stop();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void SystemMemoryPressureEvaluator::CheckMemoryPressure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Get the previous pressure level and update the current one.
  MemoryPressureLevel old_vote = current_vote();
  SetCurrentVote(CalculateCurrentPressureLevel());

  // |notify| will be set to true if MemoryPressureListeners need to be
  // notified of a memory pressure level state change.
  bool notify = false;
  switch (current_vote()) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      break;

    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      if (old_vote != current_vote()) {
        // This is a new transition to moderate pressure so notify.
        moderate_pressure_repeat_count_ = 0;
        notify = true;
      } else {
        // Already in moderate pressure, only notify if sustained over the
        // cooldown period.
        const int kModeratePressureCooldownCycles =
            kModeratePressureCooldown / kMemorySamplingPeriod;
        if (++moderate_pressure_repeat_count_ ==
            kModeratePressureCooldownCycles) {
          moderate_pressure_repeat_count_ = 0;
          notify = true;
        }
      }
      break;

    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      // Always notify of critical pressure levels.
      notify = true;
      break;
  }

  SendCurrentVote(notify);
}

base::MemoryPressureListener::MemoryPressureLevel
SystemMemoryPressureEvaluator::CalculateCurrentPressureLevel() {
  MEMORYSTATUSEX mem_status = {};
  if (!GetSystemMemoryStatus(&mem_status))
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;

  // How much system memory is actively available for use right now, in MBs.
  int phys_free = static_cast<int>(mem_status.ullAvailPhys / kMBBytes);

  // TODO(chrisha): This should eventually care about address space pressure,
  // but the browser process (where this is running) effectively never runs out
  // of address space. Renderers occasionally do, but it does them no good to
  // have the browser process monitor address space pressure. Long term,
  // renderers should run their own address space pressure monitors and act
  // accordingly, with the browser making cross-process decisions based on
  // system memory pressure.

  // Determine if the physical memory is under critical memory pressure.
  if (phys_free <= critical_threshold_mb_)
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;

  // Determine if the physical memory is under moderate memory pressure.
  if (phys_free <= moderate_threshold_mb_)
    return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;

  // No memory pressure was detected.
  return base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
}

bool SystemMemoryPressureEvaluator::GetSystemMemoryStatus(
    MEMORYSTATUSEX* mem_status) {
  DCHECK(mem_status);
  mem_status->dwLength = sizeof(*mem_status);
  if (!::GlobalMemoryStatusEx(mem_status))
    return false;
  return true;
}

SystemMemoryPressureEvaluator::OSSignalsMemoryPressureEvaluator::
    OSSignalsMemoryPressureEvaluator(std::unique_ptr<MemoryPressureVoter> voter)
    : voter_(std::move(voter)) {}

SystemMemoryPressureEvaluator::OSSignalsMemoryPressureEvaluator::
    ~OSSignalsMemoryPressureEvaluator() = default;

void SystemMemoryPressureEvaluator::OSSignalsMemoryPressureEvaluator::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Start by observing the low memory notifications. If the system is already
  // under pressure this will run the |OnLowMemoryNotification| callback and
  // automatically switch to waiting for the high memory notification/
  StartLowMemoryNotificationWatcher();
}

void SystemMemoryPressureEvaluator::OSSignalsMemoryPressureEvaluator::
    OnLowMemoryNotification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  critical_pressure_session_begin_ = base::TimeTicks::Now();

  base::UmaHistogramEnumeration(
      "Discarding.WinOSPressureSignals.PressureLevelOnLowMemoryNotification",
      base::MemoryPressureMonitor::Get()->GetCurrentPressureLevel());

  base::UmaHistogramMemoryMB(
      "Discarding.WinOSPressureSignals."
      "AvailableMemoryMbOnLowMemoryNotification",
      base::SysInfo::AmountOfAvailablePhysicalMemory() / 1024 / 1024);

  voter_->SetVote(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
                  /* notify = */ true);

  // Start a timer to repeat the notification at regular interval until
  // OnHighMemoryNotification gets called.
  critical_pressure_notification_timer_.Start(
      FROM_HERE, kHighPressureNotificationInterval,
      base::BindRepeating(
          &MemoryPressureVoter::SetVote, base::Unretained(voter_.get()),
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
          /* notify = */ true));

  // Start the high memory notification watcher to be notified when the system
  // exits memory pressure.
  StartHighMemoryNotificationWatcher();
}

void SystemMemoryPressureEvaluator::OSSignalsMemoryPressureEvaluator::
    OnHighMemoryNotification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramMediumTimes(
      "Discarding.WinOSPressureSignals.LowMemorySessionLength",
      base::TimeTicks::Now() - critical_pressure_session_begin_);
  critical_pressure_session_begin_ = base::TimeTicks();

  critical_pressure_notification_timer_.Stop();
  voter_->SetVote(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
                  /* notify = */ false);

  // Start the low memory notification watcher to be notified the next time the
  // system hits memory pressure.
  StartLowMemoryNotificationWatcher();
}

void SystemMemoryPressureEvaluator::OSSignalsMemoryPressureEvaluator::
    StartLowMemoryNotificationWatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  memory_notification_watcher_ =
      std::make_unique<MemoryPressureWatcherDelegate>(
          base::win::ScopedHandle(::CreateMemoryResourceNotification(
              ::LowMemoryResourceNotification)),
          base::BindOnce(
              &SystemMemoryPressureEvaluator::OSSignalsMemoryPressureEvaluator::
                  OnLowMemoryNotification,
              base::Unretained(this)));
}

void SystemMemoryPressureEvaluator::OSSignalsMemoryPressureEvaluator::
    StartHighMemoryNotificationWatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  memory_notification_watcher_ =
      std::make_unique<MemoryPressureWatcherDelegate>(
          base::win::ScopedHandle(::CreateMemoryResourceNotification(
              ::HighMemoryResourceNotification)),
          base::BindOnce(
              &SystemMemoryPressureEvaluator::OSSignalsMemoryPressureEvaluator::
                  OnHighMemoryNotification,
              base::Unretained(this)));
}

void SystemMemoryPressureEvaluator::OSSignalsMemoryPressureEvaluator::
    WaitForHighMemoryNotificationForTesting(base::OnceClosure closure) {
  // If the timer isn't running then it means that the high memory notification
  // has already been received.
  if (!critical_pressure_notification_timer_.IsRunning()) {
    std::move(closure).Run();
    return;
  }

  memory_notification_watcher_->SetCallbackForTesting(base::BindOnce(
      [](SystemMemoryPressureEvaluator::OSSignalsMemoryPressureEvaluator*
             evaluator,
         base::OnceClosure closure) {
        evaluator->OnHighMemoryNotification();
        std::move(closure).Run();
      },
      base::Unretained(this), std::move(closure)));
}

}  // namespace win
}  // namespace util
