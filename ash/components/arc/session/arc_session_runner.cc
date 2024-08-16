// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_session_runner.h"

#include <optional>
#include <utility>

#include "ash/components/arc/arc_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_runner.h"

namespace arc {

namespace {

constexpr base::TimeDelta kDefaultRestartDelay = base::Seconds(5);

void RecordInstanceCrashUma(ArcContainerLifetimeEvent sample) {
  UMA_HISTOGRAM_ENUMERATION("Arc.ContainerLifetimeEvent", sample,
                            ArcContainerLifetimeEvent::COUNT);
  // Log the metric to facilitate finding feedback reports in Xamine.
  VLOG(1) << "Arc.ContainerLifetimeEvent: "
          << static_cast<std::underlying_type_t<ArcContainerLifetimeEvent>>(
                 sample);
}

void RecordInstanceRestartAfterCrashUma(size_t restart_after_crash_count) {
  UMA_HISTOGRAM_COUNTS_100("Arc.ContainerRestartAfterCrashCount",
                           restart_after_crash_count);
}

// Gets an ArcContainerLifetimeEvent value to record. Returns nullopt when no
// UMA recording is needed.
std::optional<ArcContainerLifetimeEvent> GetArcContainerLifetimeEvent(
    size_t restart_after_crash_count,
    ArcStopReason stop_reason,
    bool was_running) {
  // Record UMA only when this is the first non-early crash. This has to be
  // done before checking other conditions. Otherwise, an early crash after
  // container restart might be recorded. Each CONTAINER_STARTED event can
  // be paired up to one non-START event.
  if (restart_after_crash_count)
    return std::nullopt;

  switch (stop_reason) {
    case ArcStopReason::SHUTDOWN:
    case ArcStopReason::LOW_DISK_SPACE:
      // We don't record these events.
      return std::nullopt;
    case ArcStopReason::GENERIC_BOOT_FAILURE:
      return ArcContainerLifetimeEvent::CONTAINER_FAILED_TO_START;
    case ArcStopReason::CRASH:
      return was_running ? ArcContainerLifetimeEvent::CONTAINER_CRASHED
                         : ArcContainerLifetimeEvent::CONTAINER_CRASHED_EARLY;
  }

  NOTREACHED();
}

// Returns true if restart is needed for given conditions.
bool IsRestartNeeded(std::optional<ArcInstanceMode> target_mode,
                     ArcStopReason stop_reason,
                     bool was_running) {
  if (!target_mode.has_value()) {
    // The request to run ARC is canceled by the caller. No need to restart.
    return false;
  }

  switch (stop_reason) {
    case ArcStopReason::SHUTDOWN:
      // This is a part of stop requested by ArcSessionRunner.
      // If ARC is re-requested to start, restart is necessary.
      // This case happens, e.g., RequestStart() -> RequestStop() ->
      // RequestStart(), case. If the second RequestStart() is called before
      // the instance previously running is stopped, then just |target_mode_|
      // is set. On completion, restart is needed.
      return true;
    case ArcStopReason::GENERIC_BOOT_FAILURE:
    case ArcStopReason::LOW_DISK_SPACE:
      // These two are errors on starting. To prevent failure loop, do not
      // restart.
      return false;
    case ArcStopReason::CRASH:
      // ARC instance is crashed unexpectedly, so automatically restart.
      // However, to avoid crash loop, do not restart if it is not successfully
      // started yet. So, check |was_running|.
      return was_running;
  }

  NOTREACHED();
}

// Returns true if the request to start/upgrade ARC instance is allowed
// operation.
bool IsRequestAllowed(const std::optional<ArcInstanceMode>& current_mode,
                      ArcInstanceMode request_mode) {
  if (!current_mode.has_value()) {
    // This is a request to start a new ARC instance (either mini instance
    // or full instance).
    return true;
  }

  if (current_mode == ArcInstanceMode::MINI_INSTANCE &&
      request_mode == ArcInstanceMode::FULL_INSTANCE) {
    // This is a request to upgrade the running mini instance to full instance.
    return true;
  }

  // Otherwise, not allowed.
  LOG(ERROR) << "Unexpected ARC instance mode transition request: "
             << current_mode << " -> " << request_mode;
  return false;
}

}  // namespace

ArcSessionRunner::ArcSessionRunner(const ArcSessionFactory& factory)
    : restart_delay_(kDefaultRestartDelay),
      restart_after_crash_count_(0),
      factory_(factory) {}

ArcSessionRunner::~ArcSessionRunner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (arc_session_)
    arc_session_->RemoveObserver(this);
}

void ArcSessionRunner::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_list_.AddObserver(observer);
}

void ArcSessionRunner::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_list_.RemoveObserver(observer);
}

void ArcSessionRunner::ResumeRunner() {
  VLOG(1) << "ArcSessionRunner is resumed";
  resumed_ = true;
  if (target_mode_) {
    ArcInstanceMode original_mode = *target_mode_;
    target_mode_ = std::nullopt;
    RequestStart(original_mode);
  }
}

void ArcSessionRunner::RequestStartMiniInstance() {
  RequestStart(ArcInstanceMode::MINI_INSTANCE);
}

void ArcSessionRunner::RequestUpgrade(UpgradeParams params) {
  upgrade_params_ = std::move(params);
  RequestStart(ArcInstanceMode::FULL_INSTANCE);
}

void ArcSessionRunner::RequestStart(ArcInstanceMode request_mode) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (target_mode_ == request_mode) {
    // Consecutive RequestStart() call for the same mode. Do nothing.
    return;
  }

  if (!IsRequestAllowed(target_mode_, request_mode))
    return;

  VLOG(1) << "Session start requested: " << request_mode;
  target_mode_ = request_mode;
  if (arc_session_ && arc_session_->IsStopRequested()) {
    // This is the case where RequestStop() was called, but before
    // |arc_session_| had finshed stopping, RequestStart() is called.
    // Do nothing in the that case, since when |arc_session_| does actually
    // stop, OnSessionStopped() will be called, where it should automatically
    // restart.
    return;
  }

  if (restart_timer_.IsRunning()) {
    // |restart_timer_| may be running if this is upgrade request in a
    // following scenario.
    // - RequestStart(MINI_INSTANCE)
    // - RequestStop()
    // - RequestStart(MINI_INSTANCE)
    // - OnSessionStopped()
    // - RequestStart(FULL_INSTANCE) before RestartArcSession() is called.
    // In such a case, defer the operation to RestartArcSession() called later.
    return;
  }

  if (!resumed_) {
    VLOG(1) << "Deferring to start ARC instance. "
            << "This runner hasn't been resumed yet.";
    return;
  }

  // No asynchronous event is expected later. Trigger the ArcSession now.
  StartArcSession();
}

void ArcSessionRunner::RequestStop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  VLOG(1) << "Session stop requested";
  target_mode_ = std::nullopt;

  if (arc_session_) {
    // If |arc_session_| is running, stop it.
    // Note that |arc_session_| may be already in the process of stopping or
    // be stopped.
    // E.g. RequestStart() -> RequestStop() -> RequestStart() -> RequestStop()
    // case. If the second RequestStop() is called before the first
    // RequestStop() is not yet completed for the instance, Stop() of the
    // instance is called again, but it is no-op as expected.
    arc_session_->Stop();
  }

  // In case restarting is in progress, cancel it.
  restart_timer_.Stop();
}

void ArcSessionRunner::OnShutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  VLOG(1) << "OnShutdown";
  target_mode_ = std::nullopt;
  restart_timer_.Stop();
  if (arc_session_)
    arc_session_->OnShutdown();
  // ArcSession::OnShutdown() invokes OnSessionStopped() synchronously.
  // In the observer method, |arc_session_| should be destroyed.
  DCHECK(!arc_session_);
}

void ArcSessionRunner::SetUserInfo(
    const cryptohome::Identification& cryptohome_id,
    const std::string& hash,
    const std::string& serial_number) {
  // |cryptohome_id.id()| and |hash| can be empty in unit tests. This function
  // can also be called multiple times in tests.
  // TODO(khmel): Fix tests and add DCHECKs to make sure they are not empty
  // and the function is called only once.
  DCHECK(!IsArcVmEnabled() || !serial_number.empty());
  cryptohome_id_ = cryptohome_id;
  user_id_hash_ = hash;
  serial_number_ = serial_number;
  if (arc_session_)
    arc_session_->SetUserInfo(cryptohome_id_, user_id_hash_, serial_number_);
}

void ArcSessionRunner::SetDemoModeDelegate(
    std::unique_ptr<ArcClientAdapter::DemoModeDelegate> delegate) {
  demo_mode_delegate_ = std::move(delegate);
}

void ArcSessionRunner::TrimVmMemory(TrimVmMemoryCallback callback,
                                    int page_limit) {
  if (arc_session_) {
    arc_session_->TrimVmMemory(std::move(callback), page_limit);
    return;
  }
  LOG(WARNING) << "TrimVmMemory is called when no ARC session is running";
  std::move(callback).Run(/*success=*/false,
                          /*failure_reason=*/"No ARC session is running");
}

void ArcSessionRunner::SetRestartDelayForTesting(
    const base::TimeDelta& restart_delay) {
  DCHECK(!arc_session_);
  DCHECK(!restart_timer_.IsRunning());
  restart_delay_ = restart_delay;
}

void ArcSessionRunner::StartArcSession() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!restart_timer_.IsRunning());
  DCHECK(target_mode_.has_value());

  VLOG(1) << "Starting ARC instance";
  if (!arc_session_) {
    arc_session_ = factory_.Run();
    if (!cryptohome_id_.id().empty() && !user_id_hash_.empty() &&
        !serial_number_.empty()) {
      arc_session_->SetUserInfo(cryptohome_id_, user_id_hash_, serial_number_);
    }
    arc_session_->SetDefaultDeviceScaleFactor(default_device_scale_factor_);
    arc_session_->SetDemoModeDelegate(demo_mode_delegate_.get());
    arc_session_->SetUseVirtioBlkData(use_virtio_blk_data_);
    arc_session_->SetArcSignedIn(arc_signed_in_);
    arc_session_->AddObserver(this);
    arc_session_->StartMiniInstance();
    // Record the UMA only when |restart_after_crash_count_| is zero to avoid
    // recording an auto-restart-then-crash loop. Such a crash loop is recorded
    // separately with RecordInstanceRestartAfterCrashUma().
    if (!restart_after_crash_count_)
      RecordInstanceCrashUma(ArcContainerLifetimeEvent::CONTAINER_STARTING);
  }
  if (target_mode_ == ArcInstanceMode::FULL_INSTANCE) {
    // Do not std::move the params intentionally. RestartArcSession() can
    // reuse the params without preceded by resetting them.
    arc_session_->RequestUpgrade(upgrade_params_);
  }
}

void ArcSessionRunner::RestartArcSession() {
  VLOG(0) << "Restarting ARC instance";
  // The order is important here. Call StartArcSession(), then notify observers.
  StartArcSession();
  for (auto& observer : observer_list_)
    observer.OnSessionRestarting();
}

void ArcSessionRunner::OnSessionStopped(ArcStopReason stop_reason,
                                        bool was_running,
                                        bool full_requested) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(arc_session_);
  DCHECK(!restart_timer_.IsRunning());

  VLOG(0) << "ARC stopped: " << stop_reason;

  arc_session_->RemoveObserver(this);
  arc_session_.reset();

  const std::optional<ArcContainerLifetimeEvent> uma_to_record =
      GetArcContainerLifetimeEvent(restart_after_crash_count_, stop_reason,
                                   was_running);
  if (uma_to_record.has_value())
    RecordInstanceCrashUma(uma_to_record.value());

  const bool restarting =
      IsRestartNeeded(target_mode_, stop_reason, was_running);

  if (restarting && stop_reason == ArcStopReason::CRASH) {
    ++restart_after_crash_count_;
  } else {
    // The session ended. Record the restart count.
    RecordInstanceRestartAfterCrashUma(restart_after_crash_count_);
    restart_after_crash_count_ = 0;
  }

  if (restarting) {
    // There was a previous invocation and it crashed for some reason. Try
    // starting ARC instance later again.
    // Note that even |restart_delay_| is 0 (for testing), it needs to
    // PostTask, because observer callback may call RequestStart()/Stop().
    VLOG(0) << "ARC restarting";
    restart_timer_.Start(FROM_HERE, restart_delay_,
                         base::BindOnce(&ArcSessionRunner::RestartArcSession,
                                        weak_ptr_factory_.GetWeakPtr()));
  }

  // The observers should be agnostic to the existence of the limited-purpose
  // instance.
  if (full_requested) {
    for (auto& observer : observer_list_)
      observer.OnSessionStopped(stop_reason, restarting);
  }
}

}  // namespace arc
