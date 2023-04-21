// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_workspace/floating_workspace_service.h"

#include <cstddef>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_metrics_util.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_service_factory.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sync/desk_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "components/app_restore/restore_data.h"
#include "components/desks_storage/core/desk_sync_bridge.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"

namespace ash {

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
    floating_workspace_metrics_util::
        RecordFloatingWorkspaceV1InitializedHistogram();
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
      // For testings we don't need to add itself to observer list of
      // DeskSyncBridge, tests can be done by calling
      // EntriesAddedOrUpdatedRemotely directly so InitForV2 can be skipped.
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
      initialization_timestamp_ +
          ash::features::kFloatingWorkspaceMaxTimeAvailableForRestoreAfterLogin
              .Get()) {
    // No need to restore any remote session 3 seconds (TBD) after login.
    should_run_restore_ = false;
    return;
  }
  const sync_sessions::SyncedSession* most_recently_used_remote_session =
      GetMostRecentlyUsedRemoteSession();
  const sync_sessions::SyncedSession* local_session = GetLocalSession();
  if (!most_recently_used_remote_session ||
      (local_session &&
       local_session->GetModifiedTime() >
           most_recently_used_remote_session->GetModifiedTime())) {
    // If local session is the most recently modified or no remote session,
    // dispatch a delayed task to check whether any foreign session got updated.
    // If remote session is not updated after the delay, launch local session.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &FloatingWorkspaceService::TryRestoreMostRecentlyUsedSession,
            weak_pointer_factory_.GetWeakPtr()),
        ash::features::kFloatingWorkspaceMaxTimeAvailableForRestoreAfterLogin
            .Get());
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
        local_session->GetModifiedTime() >
            most_recently_used_remote_session->GetModifiedTime()) {
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

void FloatingWorkspaceService::EntriesAddedOrUpdatedRemotely(
    const std::vector<const DeskTemplate*>& new_entries) {
  bool found_floating_workspace_template = false;
  for (const DeskTemplate* desk_template : new_entries) {
    if (desk_template &&
        desk_template->type() == DeskTemplateType::kFloatingWorkspace) {
      floating_workspace_metrics_util::
          RecordFloatingWorkspaceV2TemplateLoadTime(base::TimeTicks::Now() -
                                                    initialization_timestamp_);
      RestoreFloatingWorkspaceTemplate(desk_template);
      found_floating_workspace_template = true;
    }
  }
  // Completed waiting for desk templates to download. Unable to find a floating
  // workspace template. Emit a metric indictating we timeout because there is
  // no floating workspace template.
  if (!found_floating_workspace_template) {
    floating_workspace_metrics_util::
        RecordFloatingWorkspaceV2TemplateLaunchTimeout(
            floating_workspace_metrics_util::LaunchTemplateTimeoutType::
                kNoFloatingWorkspaceTemplate);
  }
}

void FloatingWorkspaceService::InitForV1() {
  session_sync_service_ =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile_);
}

void FloatingWorkspaceService::InitForV2() {
  desk_sync_service_ = DeskSyncServiceFactory::GetForProfile(profile_);
  StartCaptureAndUploadActiveDesk();
  desk_sync_service_->GetDeskModel()->AddObserver(this);
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
  if (open_tabs && open_tabs->GetForeignSession(session->GetSessionTag(),
                                                &session_windows)) {
    SessionRestore::RestoreForeignSessionWindows(
        profile_, session_windows.begin(), session_windows.end());
    floating_workspace_metrics_util::
        RecordFloatingWorkspaceV1RestoredSessionType(
            floating_workspace_metrics_util::RestoredBrowserSessionType::
                kRemote);
  }
}

void FloatingWorkspaceService::RestoreLocalSessionWindows() {
  // Restore local session based on user settings in
  // chrome://settings/onStartup.
  UserSessionManager::GetInstance()->LaunchBrowser(profile_);
  floating_workspace_metrics_util::RecordFloatingWorkspaceV1RestoredSessionType(
      floating_workspace_metrics_util::RestoredBrowserSessionType::kLocal);
}

sync_sessions::OpenTabsUIDelegate*
FloatingWorkspaceService::GetOpenTabsUIDelegate() {
  DCHECK(session_sync_service_);
  return session_sync_service_->GetOpenTabsUIDelegate();
}

void FloatingWorkspaceService::StartCaptureAndUploadActiveDesk() {
  timer_.Start(
      FROM_HERE,
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get(),
      this, &FloatingWorkspaceService::CaptureAndUploadActiveDesk);
}

void FloatingWorkspaceService::StopCaptureAndUploadActiveDesk() {
  timer_.Stop();
}

void FloatingWorkspaceService::CaptureAndUploadActiveDesk() {
  DesksClient::Get()->CaptureActiveDesk(
      base::BindOnce(&FloatingWorkspaceService::OnTemplateCaptured,
                     weak_pointer_factory_.GetWeakPtr()),
      DeskTemplateType::kFloatingWorkspace);
}

void FloatingWorkspaceService::RestoreFloatingWorkspaceTemplate(
    const DeskTemplate* desk_template) {
  // Desk templates have been downloaded.
  if (!should_run_restore_) {
    return;
  }
  RecordWindowAndTabCountHistogram(*desk_template);
  // Check if template has been downloaded after 15 seconds (TBD).
  if (base::TimeTicks::Now() >
      initialization_timestamp_ +
          ash::features::
              kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin.Get()) {
    // No need to restore any remote session 15 seconds (TBD) after login.
    should_run_restore_ = false;
    floating_workspace_metrics_util::
        RecordFloatingWorkspaceV2TemplateLaunchTimeout(
            floating_workspace_metrics_util::LaunchTemplateTimeoutType::
                kPassedWaitPeriod);
    return;
  }

  LaunchFloatingWorkspaceTemplate(desk_template);
}

void FloatingWorkspaceService::LaunchFloatingWorkspaceTemplate(
    const DeskTemplate* desk_template) {
  DesksClient::Get()->LaunchDeskTemplate(
      desk_template->uuid(),
      base::BindOnce(&FloatingWorkspaceService::OnTemplateLaunched,
                     weak_pointer_factory_.GetWeakPtr()),
      desk_template->template_name());
}

bool FloatingWorkspaceService::IsCurrentDeskSameAsPrevious(
    DeskTemplate* current_desk_template) const {
  if (!previously_captured_desk_template_) {
    return false;
  }
  const auto& previous_app_id_to_app_launch_list =
      previously_captured_desk_template_->desk_restore_data()
          ->app_id_to_launch_list();
  const auto& current_app_id_to_app_launch_list =
      current_desk_template->desk_restore_data()->app_id_to_launch_list();

  // If previous and current template have different number of apps they are
  // different.
  if (previous_app_id_to_app_launch_list.size() !=
      current_app_id_to_app_launch_list.size()) {
    return false;
  }

  for (const auto& it : previous_app_id_to_app_launch_list) {
    const std::string app_id = it.first;
    // Cannot find app id in currently captured desk.
    if (current_app_id_to_app_launch_list.find(app_id) ==
        current_app_id_to_app_launch_list.end()) {
      return false;
    }
    for (const auto& [restore_window_id, previous_app_restore_data] :
         it.second) {
      auto& current_app_restore_data_launch_list =
          current_app_id_to_app_launch_list.at(app_id);
      // Cannot find window id in currently captured template.
      if (current_app_restore_data_launch_list.find(restore_window_id) ==
          current_app_restore_data_launch_list.end()) {
        return false;
      }
      // For the same window the data inside are different.
      if (*current_app_restore_data_launch_list.at(restore_window_id) !=
          *previous_app_restore_data) {
        return false;
      }
    }
  }
  return true;
}

void FloatingWorkspaceService::HandleTemplateUploadErrors(
    DesksClient::DeskActionError error) {
  switch (error) {
    case DesksClient::DeskActionError::kUnknownError:
      floating_workspace_metrics_util::
          RecordFloatingWorkspaceV2TemplateLaunchFailureType(
              floating_workspace_metrics_util::LaunchTemplateFailureType::
                  kUnknownError);
      return;
    case DesksClient::DeskActionError::kStorageError:
      floating_workspace_metrics_util::
          RecordFloatingWorkspaceV2TemplateLaunchFailureType(
              floating_workspace_metrics_util::LaunchTemplateFailureType::
                  kStorageError);
      return;
    case DesksClient::DeskActionError::kDesksCountCheckFailedError:
      floating_workspace_metrics_util::
          RecordFloatingWorkspaceV2TemplateLaunchFailureType(
              floating_workspace_metrics_util::LaunchTemplateFailureType::
                  kDesksCountCheckFailedError);
      return;
    // No need to record metrics for the below desk action errors since they do
    // not relate to template launch.
    case DesksClient::DeskActionError::kNoCurrentUserError:
    case DesksClient::DeskActionError::kBadProfileError:
    case DesksClient::DeskActionError::kResourceNotFoundError:
    case DesksClient::DeskActionError::kInvalidIdError:
    case DesksClient::DeskActionError::kDesksBeingModifiedError:
      return;
  }
}

void FloatingWorkspaceService::OnTemplateLaunched(
    absl::optional<DesksClient::DeskActionError> error,
    const base::Uuid& desk_uuid) {
  // Disable future floating workspace restore.
  should_run_restore_ = false;
  if (error) {
    HandleTemplateUploadErrors(error.value());
    return;
  }
  RecordLaunchSavedDeskHistogram(DeskTemplateType::kFloatingWorkspace);
}

void FloatingWorkspaceService::OnTemplateCaptured(
    absl::optional<DesksClient::DeskActionError> error,
    std::unique_ptr<DeskTemplate> desk_template) {
  // Desk capture was not successful, nothing to upload.
  if (!desk_template) {
    return;
  }

  // If successfully captured desk, remove old entry and record new uuid.
  if (!FloatingWorkspaceService::IsCurrentDeskSameAsPrevious(
          desk_template.get())) {
    // Upload and save the template.
    desk_sync_service_->GetDeskModel()->AddOrUpdateEntry(
        std::move(desk_template),
        base::BindOnce(&FloatingWorkspaceService::OnTemplateUploaded,
                       weak_pointer_factory_.GetWeakPtr()));
  }
}

void FloatingWorkspaceService::OnTemplateUploaded(
    desks_storage::DeskModel::AddOrUpdateEntryStatus status,
    std::unique_ptr<DeskTemplate> new_entry) {
  previously_captured_desk_template_ = std::move(new_entry);
  floating_workspace_metrics_util::
      RecordFloatingWorkspaceV2TemplateUploadStatusHistogram(status);
}

}  // namespace ash
