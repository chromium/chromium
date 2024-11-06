// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/user_session_activity/user_session_activity_reporter.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"
#include "chrome/browser/ash/policy/reporting/user_session_activity/user_session_activity_reporter_delegate.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/user_session_activity.pb.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/public/mojom/compositing/video_detector_observer.mojom.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"

namespace reporting {

namespace {
bool IsRegularUserOrManagedGuestUser(const user_manager::User* user) {
  return user->GetType() == user_manager::UserType::kPublicAccount ||
         user->GetType() == user_manager::UserType::kRegular;
}
}  // namespace

// static
std::unique_ptr<UserSessionActivityReporter>
UserSessionActivityReporter::Create(
    policy::ManagedSessionService* managed_session_service,
    user_manager::UserManager* user_manager) {
  auto reporter_helper = std::make_unique<UserEventReporterHelper>(
      Destination::USER_SESSION_ACTIVITY);

  mojo::PendingRemote<viz::mojom::VideoDetectorObserver>
      video_observer_idle_notifier;

  auto idle_event_notifier =
      std::make_unique<ash::power::ml::IdleEventNotifier>(
          chromeos::PowerManagerClient::Get(), ui::UserActivityDetector::Get(),
          video_observer_idle_notifier.InitWithNewPipeAndPassReceiver());

  // Start observing video activity in `idle_event_notifier`.
  aura::Env::GetInstance()
      ->context_factory()
      ->GetHostFrameSinkManager()
      ->AddVideoDetectorObserver(std::move(video_observer_idle_notifier));

  auto delegate = base::WrapUnique(new UserSessionActivityReporterDelegate(
      std::move(reporter_helper), std::move(idle_event_notifier)));

  return base::WrapUnique(new UserSessionActivityReporter(
      managed_session_service, user_manager, std::move(delegate)));
}

UserSessionActivityReporter::UserSessionActivityReporter(
    policy::ManagedSessionService* managed_session_service,
    user_manager::UserManager* user_manager,
    std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)), session_user_(nullptr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  CHECK(managed_session_service);
  managed_session_service_observation_.Observe(managed_session_service);

  CHECK(user_manager);
  user_session_state_observation_.Observe(user_manager);
}

UserSessionActivityReporter::~UserSessionActivityReporter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  StopTimers();
}

void UserSessionActivityReporter::OnSessionTerminationStarted(
    const user_manager::User* user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(crbug.com/377539371): Remove this line after ManagedSessionObserver is
  // updated to return the active user instead of the primary user.
  user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();

  OnSessionEnd(SessionEndEvent_Reason_LOGOUT, active_user);
}

void UserSessionActivityReporter::OnLocked() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();

  OnSessionEnd(SessionEndEvent_Reason_LOCK, active_user);
}

void UserSessionActivityReporter::ActiveUserChanged(
    user_manager::User* active_user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // This user change could be a login, or an unlock.
  if (!IsSessionActive()) {
    if (is_device_locked_) {
      OnSessionStart(SessionStartEvent_Reason_UNLOCK, active_user);
    } else {
      OnSessionStart(SessionStartEvent_Reason_LOGIN, active_user);
    }
    return;
  }

  // We are switching users in multi-user mode.
  // End session for the current user and start the session for the
  // next user.
  OnSessionEnd(SessionEndEvent_Reason_MULTI_USER_SWITCH, session_user_);
  OnSessionStart(SessionStartEvent_Reason_MULTI_USER_SWITCH, active_user);
}

void UserSessionActivityReporter::OnSessionStart(
    SessionStartEvent::Reason reason,
    const user_manager::User* user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!user) {
    LOG(ERROR) << "Cannot report session start: user is null";
    return;
  }

  if (!IsRegularUserOrManagedGuestUser(user)) {
    return;
  }

  if (IsSessionActive()) {
    LOG(ERROR) << "Cannot report session start: session is already active";
    return;
  }

  is_device_locked_ = false;
  session_user_ = user;

  StartTimers();

  delegate_->SetSessionStartEvent(reason, session_user_);
}

void UserSessionActivityReporter::OnSessionEnd(SessionEndEvent::Reason reason,
                                               const user_manager::User* user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!user) {
    LOG(ERROR) << "Cannot report session end: user is null";
    return;
  }

  if (!IsRegularUserOrManagedGuestUser(user)) {
    return;
  }

  // The user can logout after locking the device. This is ok, but the session
  // has already ended so we don't want to send a second session end event. If
  // the device is NOT locked and we're trying to end an inactive session,
  // that's an error.
  if (!IsSessionActive()) {
    if (!is_device_locked_) {
      LOG(ERROR) << "Cannot report session end: session is not active";
    }
    return;
  }

  if (session_user_ != user) {
    LOG(ERROR)
        << "Ending session for different user than the current session user.";
  }

  if (reason == SessionEndEvent_Reason_LOCK) {
    is_device_locked_ = true;
  }

  StopTimers();

  delegate_->SetSessionEndEvent(reason, user);

  // Session activity should be reported at the end of each session.
  delegate_->ReportSessionActivity();

  session_user_ = nullptr;
}

void UserSessionActivityReporter::OnReportingTimerExpired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  UpdateActiveIdleState();

  // We report session activity periodically so that we can update active/idle
  // states on the server side during long sessions. This also serves as a
  // heartbeat so that the server knows it hasn't missed a session end event.
  delegate_->ReportSessionActivity();
}

void UserSessionActivityReporter::OnCollectionTimerExpired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  UpdateActiveIdleState();
}

void UserSessionActivityReporter::UpdateActiveIdleState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const ash::power::ml::IdleEventNotifier::ActivityData& activity_data =
      delegate_->QueryIdleStatus();

  bool is_user_active = delegate_->IsUserActive(activity_data);
  CHECK(session_user_);
  delegate_->AddActiveIdleState(is_user_active, session_user_);
}

void UserSessionActivityReporter::StartTimers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  reporting_timer_.Start(
      FROM_HERE, kReportingFrequency,
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &UserSessionActivityReporter::OnReportingTimerExpired,
          weak_ptr_factory_.GetWeakPtr())));

  collect_idle_state_timer_.Start(
      FROM_HERE, kActiveIdleStateCollectionFrequency,
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &UserSessionActivityReporter::OnCollectionTimerExpired,
          weak_ptr_factory_.GetWeakPtr())));
}

void UserSessionActivityReporter::StopTimers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  reporting_timer_.Stop();
  collect_idle_state_timer_.Stop();
}

bool UserSessionActivityReporter::IsSessionActive() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return session_user_ != nullptr;
}

}  // namespace reporting
