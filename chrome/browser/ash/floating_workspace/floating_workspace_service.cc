// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_workspace/floating_workspace_service.h"

#include <cstddef>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_service_factory.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sync/desk_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "components/desks_storage/core/desk_sync_bridge.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"

namespace ash {

// Max time floating workspace service can wait after user login.
// After that even a more recent foreign session change is detected
// restore will not take place.
constexpr base::TimeDelta kMaxTimeAvaliableForRestoreAfterLogin =
    base::Seconds(3);

// Static
FloatingWorkspaceService* FloatingWorkspaceService::GetForProfile(
    Profile* profile) {
  return static_cast<FloatingWorkspaceService*>(
      FloatingWorkspaceServiceFactory::GetInstance()->GetForProfile(profile));
}

FloatingWorkspaceService::FloatingWorkspaceService(Profile* profile)
    : profile_(profile), initialization_timestamp_(base::TimeTicks::Now()) {}

FloatingWorkspaceService::~FloatingWorkspaceService() {
  if (is_testing_)
    return;
  if (floating_workspace_util::IsFloatingWorkspaceV2Enabled()) {
    StopCaptureAndUploadActiveDesk();
    OnDeskModelDestroying();
  }
}

void FloatingWorkspaceService::Init() {
  is_testing_ = false;
  if (floating_workspace_util::IsFloatingWorkspaceV1Enabled()) {
    InitForV1();
    return;
  }

  if (saved_desk_util::AreDesksTemplatesEnabled() &&
      features::IsDeskTemplateSyncEnabled() &&
      floating_workspace_util::IsFloatingWorkspaceV2Enabled()) {
    InitForV2();
  }
}

void FloatingWorkspaceService::InitForTest(
    TestFloatingWorkspaceVersion version) {
  CHECK_IS_TEST();
  is_testing_ = true;
  switch (version) {
    case TestFloatingWorkspaceVersion::kNoVersionEnabled:
      break;
    case TestFloatingWorkspaceVersion::kFloatingWorkspaceV1Enabled:
      InitForV1();
      break;
    case TestFloatingWorkspaceVersion::kFloatingWorkspaceV2Enabled:
      InitForV2();
      break;
  }
}

void FloatingWorkspaceService::SubscribeToForeignSessionUpdates() {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  // If sync is disabled no need to observe anything.
  if (!sync_service || !sync_service->IsSyncFeatureEnabled()) {
    return;
  }
  foreign_session_updated_subscription_ =
      session_sync_service_->SubscribeToForeignSessionsChanged(
          base::BindRepeating(
              &FloatingWorkspaceService::
                  RestoreBrowserWindowsFromMostRecentlyUsedDevice,
              weak_pointer_factory_.GetWeakPtr()));
}

void FloatingWorkspaceService::
    RestoreBrowserWindowsFromMostRecentlyUsedDevice() {
  if (!should_run_restore_)
    return;
  if (base::TimeTicks::Now() >
      initialization_timestamp_ + kMaxTimeAvaliableForRestoreAfterLogin) {
    // No need to restore any remote session 3 seconds (TBD) after login.
    should_run_restore_ = false;
    return;
  }
  const sync_sessions::SyncedSession* most_recently_used_remote_session =
      GetMostRecentlyUsedRemoteSession();
  const sync_sessions::SyncedSession* local_session = GetLocalSession();
  if (!most_recently_used_remote_session ||
      (local_session && local_session->modified_time >
                            most_recently_used_remote_session->modified_time)) {
    // If local session is the most recently modified or no remote session,
    // dispatch a delayed task to check whether any foreign session got updated.
    // If remote session is not updated after the delay, launch local session.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &FloatingWorkspaceService::TryRestoreMostRecentlyUsedSession,
            weak_pointer_factory_.GetWeakPtr()),
        kMaxTimeAvaliableForRestoreAfterLogin);
    should_run_restore_ = false;
    return;
  }

  // Restore most recently used remote session.
  RestoreForeignSessionWindows(most_recently_used_remote_session);
  should_run_restore_ = false;
}

void FloatingWorkspaceService::TryRestoreMostRecentlyUsedSession() {
  // A task generated by RestoreBrowserWindowsFromMostRecentlyUsedDevice
  // will call this method with a delay, at this time if local session is
  // still more recent, restore the local session.
  const sync_sessions::SyncedSession* local_session = GetLocalSession();
  const sync_sessions::SyncedSession* most_recently_used_remote_session =
      GetMostRecentlyUsedRemoteSession();
  if (local_session) {
    if (!most_recently_used_remote_session ||
        local_session->modified_time >
            most_recently_used_remote_session->modified_time) {
      // This is a delayed task, if at this time local session is still
      // most recent, restore local session.
      RestoreLocalSessionWindows();
    } else {
      RestoreForeignSessionWindows(most_recently_used_remote_session);
    }
  } else if (most_recently_used_remote_session) {
    RestoreForeignSessionWindows(most_recently_used_remote_session);
  }
}

void FloatingWorkspaceService::OnDeskModelDestroying() {
  desk_sync_service_->GetDeskModel()->RemoveObserver(this);
}

void FloatingWorkspaceService::InitForV1() {
  session_sync_service_ =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile_);
}

void FloatingWorkspaceService::InitForV2() {
  desk_sync_service_ = DeskSyncServiceFactory::GetForProfile(profile_);
  StartCaptureAndUploadActiveDesk();
  desk_sync_service_->GetDeskModel()->AddObserver(this);
  syncer::SyncService* sync_service(
      SyncServiceFactory::GetForProfile(profile_));
  if (sync_service)
    sync_service->TriggerRefresh({syncer::WORKSPACE_DESK});
}

const sync_sessions::SyncedSession*
FloatingWorkspaceService::GetMostRecentlyUsedRemoteSession() {
  sync_sessions::OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate();
  std::vector<const sync_sessions::SyncedSession*> remote_sessions;
  if (!open_tabs || !open_tabs->GetAllForeignSessions(&remote_sessions)) {
    return nullptr;
  }
  // GetAllForeignSessions returns remote sessions in sorted way
  // with most recent at first.
  return remote_sessions.front();
}

const sync_sessions::SyncedSession*
FloatingWorkspaceService::GetLocalSession() {
  sync_sessions::OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate();
  const sync_sessions::SyncedSession* local_session = nullptr;
  if (!open_tabs || !open_tabs->GetLocalSession(&local_session))
    return nullptr;
  return local_session;
}

void FloatingWorkspaceService::RestoreForeignSessionWindows(
    const sync_sessions::SyncedSession* session) {
  sync_sessions::OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate();
  std::vector<const sessions::SessionWindow*> session_windows;
  if (open_tabs &&
      open_tabs->GetForeignSession(session->session_tag, &session_windows)) {
    SessionRestore::RestoreForeignSessionWindows(
        profile_, session_windows.begin(), session_windows.end());
  }
}

void FloatingWorkspaceService::RestoreLocalSessionWindows() {
  // Restore local session based on user settings in
  // chrome://settings/onStartup.
  UserSessionManager::GetInstance()->LaunchBrowser(profile_);
}

sync_sessions::OpenTabsUIDelegate*
FloatingWorkspaceService::GetOpenTabsUIDelegate() {
  DCHECK(session_sync_service_);
  return session_sync_service_->GetOpenTabsUIDelegate();
}

void FloatingWorkspaceService::StartCaptureAndUploadActiveDesk() {
  timer_.Start(FROM_HERE, kPeriodicJobIntervalInSeconds, this,
               &FloatingWorkspaceService::CaptureAndUploadActiveDesk);
}

void FloatingWorkspaceService::StopCaptureAndUploadActiveDesk() {
  timer_.Stop();
}

// TODO(b/258692868): Add a method in DesksClient to capture but not save
// current desk for floating workspace; we can attach our own callback with the
// prev/current comparison method to see if a upload/save is necessary.
void FloatingWorkspaceService::CaptureAndUploadActiveDesk() {
  DesksClient::Get()->CaptureActiveDeskAndSaveTemplate(
      base::BindOnce(&FloatingWorkspaceService::OnTemplateCaptured,
                     weak_pointer_factory_.GetWeakPtr()),
      DeskTemplateType::kFloatingWorkspace);
}

void FloatingWorkspaceService::OnTemplateCaptured(
    absl::optional<DesksClient::DeskActionError> error,
    std::unique_ptr<DeskTemplate> desk_template) {
  // Desk capture was not successful, nothing to upload.
  if (!desk_template)
    return;

  // If successfully captured desk, remove old entry and record new uuid.
  if (!FloatingWorkspaceService::IsCurrentDeskSameAsPrevious(
          desk_template.get())) {
    // Upload and save the template.
    desk_sync_service_->GetDeskModel()->AddOrUpdateEntry(
        std::move(desk_template), base::DoNothing());
  }
}

void FloatingWorkspaceService::EntriesAddedOrUpdatedRemotely(
    const std::vector<const DeskTemplate*>& new_entries) {
  for (const DeskTemplate* desk_template : new_entries) {
    if (desk_template &&
        desk_template->type() == DeskTemplateType::kFloatingWorkspace) {
      RestoreFloatingWorkspaceTemplate(desk_template);
    }
  }
}

void FloatingWorkspaceService::RestoreFloatingWorkspaceTemplate(
    const DeskTemplate* desk_template) {
  // Desk templates have been downloaded.
  if (!should_run_restore_) {
    return;
  }

  // Check if template has been downloaded after 3 seconds.
  if (base::TimeTicks::Now() >
      initialization_timestamp_ + kMaxTimeAvaliableForRestoreAfterLogin) {
    // No need to restore any remote session 3 seconds (TBD) after login.
    should_run_restore_ = false;
    return;
  }

  DesksClient::Get()->LaunchDeskTemplate(
      desk_template->uuid(),
      base::BindOnce(&FloatingWorkspaceService::OnTemplateLaunched,
                     weak_pointer_factory_.GetWeakPtr()),
      desk_template->template_name());
}

void FloatingWorkspaceService::OnTemplateLaunched(
    absl::optional<DesksClient::DeskActionError> error,
    const base::GUID& desk_uuid) {
  // Disable future floating workspace restore.
  should_run_restore_ = false;
}

// TODO(b/256874545): Implement comparison where all apps/ browsers are checked.
// As of right now, a return of false indicates that both templates
// are different, thus periodic checks will happen every 30 seconds
// regardless of if no changes exist.
bool FloatingWorkspaceService::IsCurrentDeskSameAsPrevious(
    DeskTemplate* current) const {
  return false;
}

}  // namespace ash
