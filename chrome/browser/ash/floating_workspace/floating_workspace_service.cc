// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_workspace/floating_workspace_service.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "base/check.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_metrics_util.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_service_factory.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sync/desk_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/grit/generated_resources.h"
#include "components/app_constants/constants.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/desks_storage/core/desk_sync_bridge.h"
#include "components/desks_storage/core/desk_sync_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

constexpr char kNotificationForNoNetworkConnection[] =
    "notification_no_network_connection";
constexpr char kNotificationForSyncErrorOrTimeOut[] =
    "notification_sync_error_or_timeout";
constexpr char kNotificationForRestoreAfterError[] =
    "notification_restore_after_error";
constexpr char kNotificationForProgressStatus[] =
    "notification_progress_status";
constexpr char kSafeMode[] = "notification_safe_mode";
// Default time without activity after which a floating workspace template is
// considered stale and becomes a candidate for garbage collection.
constexpr base::TimeDelta kStaleFWSThreshold = base::Days(30);
// Minimum time to wait before we decide to show the progress status if no
// floating workspace templates have been downloaded yet.
constexpr base::TimeDelta kMinTimeToWait = base::Seconds(2);
// Time interval between progress bar update.
constexpr base::TimeDelta kProgressTimeUpdateDelay = base::Seconds(1);

FloatingWorkspaceServiceNotificationType GetNotificationTypeById(
    const std::string& id) {
  if (id == kNotificationForNoNetworkConnection) {
    return FloatingWorkspaceServiceNotificationType::kNoNetworkConnection;
  }
  if (id == kNotificationForSyncErrorOrTimeOut) {
    return FloatingWorkspaceServiceNotificationType::kSyncErrorOrTimeOut;
  }
  if (id == kNotificationForRestoreAfterError) {
    return FloatingWorkspaceServiceNotificationType::kRestoreAfterError;
  }
  if (id == kNotificationForProgressStatus) {
    return FloatingWorkspaceServiceNotificationType::kProgressStatus;
  }
  if (id == kSafeMode) {
    return FloatingWorkspaceServiceNotificationType::kSafeMode;
  }
  return FloatingWorkspaceServiceNotificationType::kUnknown;
}

// Static
FloatingWorkspaceService* FloatingWorkspaceService::GetForProfile(
    Profile* profile) {
  return static_cast<FloatingWorkspaceService*>(
      FloatingWorkspaceServiceFactory::GetInstance()->GetForProfile(profile));
}

FloatingWorkspaceService::FloatingWorkspaceService(
    Profile* profile,
    floating_workspace_util::FloatingWorkspaceVersion version)
    : profile_(profile),
      version_(version),
      initialization_timeticks_(base::TimeTicks::Now()),
      initialization_time_(base::Time::Now()) {}

FloatingWorkspaceService::~FloatingWorkspaceService() {
  if (timer_.IsRunning()) {
    StopCaptureAndUploadActiveDesk();
  }
  ShutDownServicesAndObservers();
  if (ash::SessionController::Get()) {
    ash::SessionController::Get()->RemoveObserver(this);
  }
}

void FloatingWorkspaceService::OnSyncShutdown(syncer::SyncService* sync) {
  if (sync_service_ && sync_service_->HasObserver(this)) {
    sync_service_->RemoveObserver(this);
  }
  sync_service_ = nullptr;
}

// TODO(b/309137462): Clean up params to not need to be passed in.
void FloatingWorkspaceService::Init(
    syncer::SyncService* sync_service,
    desks_storage::DeskSyncService* desk_sync_service) {
  if (ash::SessionController::Get()) {
    ash::SessionController::Get()->AddObserver(this);
  }

  if (version_ == floating_workspace_util::FloatingWorkspaceVersion::
                      kFloatingWorkspaceV1Enabled) {
    floating_workspace_metrics_util::
        RecordFloatingWorkspaceV1InitializedHistogram();
    InitForV1();
    return;
  }

  if (version_ == floating_workspace_util::FloatingWorkspaceVersion::
                      kFloatingWorkspaceV2Enabled &&
      floating_workspace_util::IsFloatingWorkspaceV2Enabled()) {
    InitForV2(sync_service, desk_sync_service);
  }
  LOG(WARNING) << "Floating workspace V2 init (not a warning)";
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
      initialization_timeticks_ +
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

void FloatingWorkspaceService::OnStateChanged(syncer::SyncService* sync) {
  switch (sync->GetDownloadStatusFor(syncer::ModelType::WORKSPACE_DESK)) {
    case syncer::SyncService::ModelTypeDownloadStatus::kWaitingForUpdates: {
      // Floating Workspace Service needs to Wait until workspace desks are up
      // to date.
      break;
    }
    case syncer::SyncService::ModelTypeDownloadStatus::kUpToDate: {
      if (!should_run_restore_) {
        MaybeSignOutOfCurrentSession();
        return;
      }
      if (!first_uptodate_download_timeticks_.has_value()) {
        first_uptodate_download_timeticks_ = base::TimeTicks::Now();
      }
      if (!is_cache_ready_) {
        should_launch_on_ready_ = true;
        VLOG(1)
            << "App cache is not ready. Don't restore floating workspace yet.";
        return;
      }
      StopProgressBarAndRestoreFloatingWorkspace();
      break;
    }
    case syncer::SyncService::ModelTypeDownloadStatus::kError: {
      // Sync is not expected to deliver the data, let user decide.
      // TODO: send notification to user asking if restore local.
      if (!should_run_restore_) {
        return;
      }
      StopProgressBarNotification();
      HandleSyncEror();
      break;
    }
  }
}

void FloatingWorkspaceService::Click(
    const absl::optional<int>& button_index,
    const absl::optional<std::u16string>& reply) {
  DCHECK(notification_);

  switch (GetNotificationTypeById(notification_->id())) {
    case FloatingWorkspaceServiceNotificationType::kUnknown:
      // For unknown type of notification id, do nothing and run close logic.
    case FloatingWorkspaceServiceNotificationType::kSyncErrorOrTimeOut:
    case FloatingWorkspaceServiceNotificationType::kProgressStatus:
    case FloatingWorkspaceServiceNotificationType::kSafeMode:
      break;
    case FloatingWorkspaceServiceNotificationType::kNoNetworkConnection:
      if (button_index.has_value()) {
        // Show network settings if the user clicks the show network settings
        // button.
        chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
            profile_, chromeos::settings::mojom::kNetworkSectionPath);
      }
      break;
    case FloatingWorkspaceServiceNotificationType::kRestoreAfterError:
      if (!button_index.has_value() ||
          button_index.value() ==
              static_cast<int>(
                  RestoreFromErrorNotificationButtonIndex::kRestore)) {
        VLOG(1) << "Restore button clicked for floating workspace after error";
        if (floating_workspace_template_to_restore_ != nullptr) {
          LaunchFloatingWorkspaceTemplate(
              floating_workspace_template_to_restore_.get());
        }
      }
      break;
  }
  MaybeCloseNotification();
}

void FloatingWorkspaceService::MaybeCloseNotification() {
  if (notification_ == nullptr) {
    return;
  }
  // If it's a progress bar notification and we're still waiting for chrome sync
  // to finish downloading, don't need to close notification.
  if (notification_->type() == message_center::NOTIFICATION_TYPE_PROGRESS &&
      !progress_notification_id_.empty() &&
      progress_notification_id_ == notification_->id()) {
    return;
  }
  auto* notification_display_service =
      NotificationDisplayService::GetForProfile(profile_);
  notification_display_service->Close(NotificationHandler::Type::TRANSIENT,
                                      notification_->id());
  notification_ = nullptr;
}

void FloatingWorkspaceService::InitForV1() {
  session_sync_service_ =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile_);
}

void FloatingWorkspaceService::InitForV2(
    syncer::SyncService* sync_service,
    desks_storage::DeskSyncService* desk_sync_service) {
  // Disable floating workspace action in safe mode.
  if (floating_workspace_util::IsSafeMode()) {
    LOG(WARNING) << "Floating workspace disabled in safe mode.";
    SendNotification(kSafeMode);
    return;
  }
  floating_workspace_metrics_util::
      RecordFloatingWorkspaceV2InitializedHistogram();
  SetUpServiceAndObservers(sync_service, desk_sync_service);
  if (!floating_workspace_util::IsInternetConnected()) {
    SendNotification(kNotificationForNoNetworkConnection);
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &FloatingWorkspaceService::MaybeStartProgressBarNotification,
            weak_pointer_factory_.GetWeakPtr()),
        kMinTimeToWait);
  }
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
  if (!open_tabs) {
    return;
  }
  std::vector<const sessions::SessionWindow*> session_windows =
      open_tabs->GetForeignSession(session->GetSessionTag());
  if (session_windows.empty()) {
    return;
  }
  SessionRestore::RestoreForeignSessionWindows(
      profile_, session_windows.begin(), session_windows.end());
  floating_workspace_metrics_util::RecordFloatingWorkspaceV1RestoredSessionType(
      floating_workspace_metrics_util::RestoredBrowserSessionType::kRemote);
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
  CaptureAndUploadActiveDesk();
  if (!timer_.IsRunning()) {
    timer_.Start(
        FROM_HERE,
        ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get(),
        this, &FloatingWorkspaceService::CaptureAndUploadActiveDesk);
  }
}

void FloatingWorkspaceService::StopCaptureAndUploadActiveDesk() {
  timer_.Stop();
}

void FloatingWorkspaceService::MaybeStartProgressBarNotification() {
  if (!should_run_restore_) {
    return;
  }
  progress_timer_.Start(FROM_HERE, kProgressTimeUpdateDelay, this,
                        &FloatingWorkspaceService::HandleProgressBarStatus);
}

void FloatingWorkspaceService::StopProgressBarNotification() {
  progress_notification_id_ = std::string();
  if (progress_timer_.IsRunning()) {
    progress_timer_.Stop();
  }
  MaybeCloseNotification();
}

void FloatingWorkspaceService::HandleProgressBarStatus() {
  const base::TimeDelta time_difference =
      base::TimeTicks::Now() - initialization_timeticks_;
  if (!should_run_restore_ ||
      time_difference >=
          ash::features::
              kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin.Get()) {
    StopProgressBarNotification();
    MaybeHandleDownloadTimeOut();
    return;
  }

  SendNotification(kNotificationForProgressStatus);
}

const DeskTemplate*
FloatingWorkspaceService::GetLatestFloatingWorkspaceTemplate() {
  const DeskTemplate* floating_workspace_template = nullptr;
  std::vector<const ash::DeskTemplate*> fws_entries =
      GetFloatingWorkspaceTemplateEntries();
  VLOG(1) << "Found " << fws_entries.size() << " floating workspace entries";
  for (const DeskTemplate* entry : fws_entries) {
    if (!entry) {
      continue;
    }
    if (!floating_workspace_template ||
        floating_workspace_template->GetLastUpdatedTime() <
            entry->GetLastUpdatedTime()) {
      floating_workspace_template = entry;
    }
  }
  DoGarbageCollection(/*exclude template=*/floating_workspace_template);
  return floating_workspace_template;
}

std::vector<const ash::DeskTemplate*>
FloatingWorkspaceService::GetFloatingWorkspaceTemplateEntries() {
  std::vector<const ash::DeskTemplate*> entries;
  if (!desk_sync_service_ || !desk_sync_service_->GetDeskModel()) {
    return entries;
  }
  desks_storage::DeskModel::GetAllEntriesResult result =
      desk_sync_service_->GetDeskModel()->GetAllEntries();
  if (result.status != desks_storage::DeskModel::GetAllEntriesStatus::kOk) {
    return entries;
  }
  for (const DeskTemplate* desk_template : result.entries) {
    if (desk_template &&
        desk_template->type() == DeskTemplateType::kFloatingWorkspace) {
      entries.push_back(desk_template);
    }
  }
  return entries;
}

void FloatingWorkspaceService::CaptureAndUploadActiveDesk() {
  GetDesksClient()->CaptureActiveDesk(
      base::BindOnce(&FloatingWorkspaceService::OnTemplateCaptured,
                     weak_pointer_factory_.GetWeakPtr()),
      DeskTemplateType::kFloatingWorkspace);
}

void FloatingWorkspaceService::CaptureAndUploadActiveDeskForTest(
    std::unique_ptr<DeskTemplate> desk_template) {
  OnTemplateCaptured(absl::nullopt, std::move(desk_template));
}

void FloatingWorkspaceService::StopProgressBarAndRestoreFloatingWorkspace() {
  StopProgressBarNotification();
  RestoreFloatingWorkspaceTemplate(GetLatestFloatingWorkspaceTemplate());
  StartCaptureAndUploadActiveDesk();
}

void FloatingWorkspaceService::RestoreFloatingWorkspaceTemplate(
    const DeskTemplate* desk_template) {
  if (desk_template == nullptr) {
    LOG(WARNING) << "No floating workspace entry found. Won't "
                    "restore. This is only possible if this is the first time "
                    "a user is using Floating Workspace.";
    should_run_restore_ = false;
    floating_workspace_metrics_util::
        RecordFloatingWorkspaceV2TemplateNotFound();
    return;
  }
  // Record metrics for window and tab count and also the time it took to
  // download the floating workspace template.
  floating_workspace_metrics_util::RecordFloatingWorkspaceV2TemplateLoadTime(
      base::TimeTicks::Now() - initialization_timeticks_);
  RecordWindowAndTabCountHistogram(*desk_template);
  // Check if template has been downloaded after
  // kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin.
  if (base::TimeTicks::Now() >
      initialization_timeticks_ +
          ash::features::
              kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin.Get()) {
    // Template arrives late, asking user to restore or not.
    StopProgressBarNotification();
    SendNotification(kNotificationForRestoreAfterError);
    // Save the workspace template in memory so we can restore the correct one.
    floating_workspace_template_to_restore_ = desk_template->Clone();
    // Set this flag to false after sending restore notification to user
    // since the user will control the restoration behavior from here on.
    should_run_restore_ = false;
    return;
  }
  LaunchFloatingWorkspaceTemplate(desk_template);
}

void FloatingWorkspaceService::LaunchFloatingWorkspaceTemplate(
    const DeskTemplate* desk_template) {
  should_run_restore_ = false;
  if (desk_template == nullptr) {
    return;
  }
  base::Uuid active_desk_uuid = GetDesksClient()->GetActiveDesk();
  VLOG(1) << "Launching Floating Workspace template with timestamp of "
          << desk_template->GetLastUpdatedTime();
  RemoveAllPreviousDesksExceptActiveDesk(
      /*exclude_desk_uuid=*/active_desk_uuid);

  VLOG(1) << "Combining Floating Workspace apps to current desk.";
  std::unique_ptr<DeskTemplate> template_copy = desk_template->Clone();
  // Open the apps from the floating workspace on top of existing windows.
  saved_desk_util::UpdateTemplateActivationIndicesRelativeOrder(*template_copy);
  GetDesksClient()->LaunchAppsFromTemplate(std::move(template_copy));
  RecordLaunchSavedDeskHistogram(DeskTemplateType::kFloatingWorkspace);
}

DesksClient* FloatingWorkspaceService::GetDesksClient() {
  return DesksClient::Get();
}

bool FloatingWorkspaceService::IsCurrentDeskSameAsPrevious(
    DeskTemplate* current_desk_template) const {
  if (!previously_captured_desk_template_) {
    return false;
  }

  // If the last user activity was before the last uploaded template, then it is
  // very likely that the current captured desk is done due to changing urls for
  // the same window (caused by things like auth protection on gmail app when
  // certs aren't installed).
  if (ui::UserActivityDetector::Get()->last_activity_time() <=
      last_uploaded_timeticks_) {
    return true;
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

void FloatingWorkspaceService::HandleTemplateCaptureErrors(
    DesksClient::DeskActionError error) {
  switch (error) {
    case DesksClient::DeskActionError::kUnknownError:
      LOG(WARNING) << "Failed to capture template: unknown error.";
      return;
    case DesksClient::DeskActionError::kStorageError:
      LOG(WARNING) << "Failed to capture template: storage error.";
      return;
    case DesksClient::DeskActionError::kDesksCountCheckFailedError:
      LOG(WARNING) << "Failed to capture template: max number of desks open.";
      return;
    case DesksClient::DeskActionError::kNoCurrentUserError:
      LOG(WARNING) << "Failed to capture template: no active user.";
      return;
    case DesksClient::DeskActionError::kBadProfileError:
      LOG(WARNING) << "Failed to capture template: bad profile.";
      return;
    case DesksClient::DeskActionError::kResourceNotFoundError:
      LOG(WARNING) << "Failed to capture template: resource not found.";
      return;
    case DesksClient::DeskActionError::kInvalidIdError:
      LOG(WARNING) << "Failed to capture template: desk id is invalid.";
      return;
    case DesksClient::DeskActionError::kDesksBeingModifiedError:
      LOG(WARNING)
          << "Failed to capture template: desk is currently being modified.";
      return;
  }
}

void FloatingWorkspaceService::OnTemplateCaptured(
    absl::optional<DesksClient::DeskActionError> error,
    std::unique_ptr<DeskTemplate> desk_template) {
  // Desk capture was not successful, nothing to upload.
  if (error) {
    HandleTemplateCaptureErrors(error.value());
  }
  if (!desk_template) {
    LOG(WARNING) << "Desk capture failed. Nothing to upload.";
    return;
  }
  // Check if there's an associated floating workspace uuid from the desk
  // sync bridge. If there is, use that one. The `floating_workspace_uuid_
  // is populated once during the first capture of the session if there is
  // known information from the sync bridge and the info may be outdated for
  // the sync bridge. However, the sync bridge does not need to know the new
  // uuid since the current service will handle it. Ignore for testing.
  if (!floating_workspace_uuid_.has_value()) {
    absl::optional<base::Uuid> floating_workspace_uuid_from_desk_model =
        GetFloatingWorkspaceUuidForCurrentDevice();
    if (floating_workspace_uuid_from_desk_model.has_value()) {
      floating_workspace_uuid_ =
          floating_workspace_uuid_from_desk_model.value();
    }
  }
  if (floating_workspace_uuid_.has_value() &&
      floating_workspace_uuid_.value().is_valid()) {
    desk_template->set_uuid(floating_workspace_uuid_.value());
  } else {
    floating_workspace_uuid_ = desk_template->uuid();
  }
  // If it successfully captured desk, remove old entry and record new uuid only
  // if the user was active from when the sync cycle is finished to now.
  if (!IsCurrentDeskSameAsPrevious(desk_template.get()) &&
      (first_uptodate_download_timeticks_.has_value() &&
       first_uptodate_download_timeticks_.value() <=
           ui::UserActivityDetector::Get()->last_activity_time())) {
    UploadFloatingWorkspaceTemplateToDeskModel(std::move(desk_template));
  }
}

void FloatingWorkspaceService::UploadFloatingWorkspaceTemplateToDeskModel(
    std::unique_ptr<DeskTemplate> desk_template) {
  // Upload and save the template.
  auto* active_user = user_manager::UserManager::Get()->GetActiveUser();
  auto* user_profile = ProfileHelper::Get()->GetProfileByUser(active_user);
  // Do not upload if the active user profile doesn't match the logged in user
  // profile.
  if (user_profile != profile_) {
    return;
  }
  desk_sync_service_->GetDeskModel()->AddOrUpdateEntry(
      std::move(desk_template),
      base::BindOnce(&FloatingWorkspaceService::OnTemplateUploaded,
                     weak_pointer_factory_.GetWeakPtr()));
}

void FloatingWorkspaceService::OnTemplateUploaded(
    desks_storage::DeskModel::AddOrUpdateEntryStatus status,
    std::unique_ptr<DeskTemplate> new_entry) {
  previously_captured_desk_template_ = std::move(new_entry);
  last_uploaded_timeticks_ = base::TimeTicks::Now();
  floating_workspace_metrics_util::
      RecordFloatingWorkspaceV2TemplateUploadStatusHistogram(status);
  VLOG(1) << "Desk template uploaded successfully.";
}

absl::optional<base::Uuid>
FloatingWorkspaceService::GetFloatingWorkspaceUuidForCurrentDevice() {
  std::string cache_guid = desk_sync_service_->GetDeskModel()->GetCacheGuid();
  std::vector<const ash::DeskTemplate*> fws_entries =
      GetFloatingWorkspaceTemplateEntries();
  for (const DeskTemplate* entry : fws_entries) {
    if (entry && entry->client_cache_guid() == cache_guid) {
      return entry->uuid();
    }
  }
  return absl::nullopt;
}

void FloatingWorkspaceService::HandleSyncEror() {
  SendNotification(kNotificationForSyncErrorOrTimeOut);
}

void FloatingWorkspaceService::MaybeHandleDownloadTimeOut() {
  if (!should_run_restore_) {
    return;
  }
  // Record timeout metrics.
  floating_workspace_metrics_util::
      RecordFloatingWorkspaceV2TemplateLaunchTimeout(
          floating_workspace_metrics_util::LaunchTemplateTimeoutType::
              kPassedWaitPeriod);
  SendNotification(kNotificationForSyncErrorOrTimeOut);
}

void FloatingWorkspaceService::SendNotification(const std::string& id) {
  // If there is a previous notification for floating workspace, close it.
  MaybeCloseNotification();

  message_center::RichNotificationData notification_data;
  std::u16string title, message;
  message_center::SystemNotificationWarningLevel warning_level;
  const base::TimeDelta time_difference =
      base::TimeTicks::Now() - initialization_timeticks_;
  bool is_progress_bar = false;
  switch (GetNotificationTypeById(id)) {
    case FloatingWorkspaceServiceNotificationType::kNoNetworkConnection:
      title =
          l10n_util::GetStringUTF16(IDS_FLOATING_WORKSPACE_NO_NETWORK_TITLE);
      message =
          l10n_util::GetStringUTF16(IDS_FLOATING_WORKSPACE_NO_NETWORK_MESSAGE);
      warning_level =
          message_center::SystemNotificationWarningLevel::CRITICAL_WARNING;
      notification_data.buttons.emplace_back(
          l10n_util::GetStringUTF16(IDS_FLOATING_WORKSPACE_NO_NETWORK_BUTTON));
      break;
    case FloatingWorkspaceServiceNotificationType::kSyncErrorOrTimeOut:
      title = l10n_util::GetStringUTF16(IDS_FLOATING_WORKSPACE_ERROR_TITLE);
      message = l10n_util::GetStringUTF16(IDS_FLOATING_WORKSPACE_ERROR_MESSAGE);
      warning_level =
          message_center::SystemNotificationWarningLevel::CRITICAL_WARNING;
      break;
    case FloatingWorkspaceServiceNotificationType::kRestoreAfterError:
      title = l10n_util::GetStringUTF16(
          IDS_FLOATING_WORKSPACE_RESTORE_FROM_ERROR_TITLE);
      message = l10n_util::GetStringUTF16(
          IDS_FLOATING_WORKSPACE_RESTORE_FROM_ERROR_MESSAGE);
      warning_level = message_center::SystemNotificationWarningLevel::NORMAL;
      notification_data.buttons.emplace_back(l10n_util::GetStringUTF16(
          IDS_FLOATING_WORKSPACE_RESTORE_FROM_ERROR_RESTORATION_BUTTON));
      break;
    case FloatingWorkspaceServiceNotificationType::kProgressStatus:
      title =
          l10n_util::GetStringUTF16(IDS_FLOATING_WORKSPACE_PROGRESS_BAR_TITLE);
      notification_data.progress_status = l10n_util::GetStringUTF16(
          IDS_FLOATING_WORKSPACE_PROGRESS_BAR_MESSAGE);
      warning_level = message_center::SystemNotificationWarningLevel::NORMAL;
      notification_data.progress = std::min(
          100.0,
          (time_difference * 100.0) /
              ash::features::
                  kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin
                      .Get());
      is_progress_bar = true;
      break;
    case FloatingWorkspaceServiceNotificationType::kSafeMode:
      title = l10n_util::GetStringUTF16(IDS_FLOATING_WORKSPACE_SAFE_MODE_TITLE);
      message =
          l10n_util::GetStringUTF16(IDS_FLOATING_WORKSPACE_SAFE_MODE_MESSAGE);
      warning_level = message_center::SystemNotificationWarningLevel::WARNING;
      break;
    case FloatingWorkspaceServiceNotificationType::kUnknown:
      VLOG(2) << "Unknown notification type for floating workspace, skip "
                 "sending notification";
      return;
  }
  // Update the current notification with progress status if we are still
  // showing progress status. Otherwise, make a new notification.
  if (is_progress_bar && notification_ != nullptr &&
      !progress_notification_id_.empty() &&
      notification_->id() == progress_notification_id_) {
    notification_->set_progress(notification_data.progress);
  } else {
    notification_ = CreateSystemNotificationPtr(
        is_progress_bar ? message_center::NOTIFICATION_TYPE_PROGRESS
                        : message_center::NOTIFICATION_TYPE_SIMPLE,
        id, title, message,
        l10n_util::GetStringUTF16(IDS_FLOATING_WORKSPACE_DISPLAY_SOURCE),
        GURL(),
        message_center::NotifierId(
            message_center::NotifierType::SYSTEM_COMPONENT, id,
            NotificationCatalogName::kFloatingWorkspace),
        notification_data,
        base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
            weak_pointer_factory_.GetWeakPtr()),
        kFloatingWorkspaceNotificationIcon, warning_level);
    notification_->set_priority(message_center::SYSTEM_PRIORITY);
    if (is_progress_bar) {
      progress_notification_id_ = notification_->id();
    }
  }
  auto* notification_display_service =
      NotificationDisplayService::GetForProfile(profile_);
  notification_display_service->Display(NotificationHandler::Type::TRANSIENT,
                                        *notification_,
                                        /*metadata=*/nullptr);
}

void FloatingWorkspaceService::DoGarbageCollection(
    const DeskTemplate* exclude_template) {
  // Do not delete any floating workspace templates if we have less than 2
  // templates. We want to keep the latest template. If there's only one
  // floating workspace template then this is the latest one.
  std::vector<const DeskTemplate*> fws_entries =
      GetFloatingWorkspaceTemplateEntries();
  if (fws_entries.size() < 2) {
    return;
  }
  for (const DeskTemplate* entry : fws_entries) {
    const base::TimeDelta template_age =
        base::Time::Now() - entry->GetLastUpdatedTime();
    if (template_age < kStaleFWSThreshold ||
        (exclude_template != nullptr &&
         exclude_template->uuid() == entry->uuid())) {
      continue;
    }
    base::Uuid uuid = entry->uuid();
    desk_sync_service_->GetDeskModel()->DeleteEntry(uuid, base::DoNothing());
  }
}

// TODO(b/294456894): Migrate to desk controller logic.
void FloatingWorkspaceService::RemoveAllPreviousDesksExceptActiveDesk(
    const base::Uuid& exclude_desk_uuid) {
  auto all_desks = GetDesksClient()->GetAllDesks();
  if (all_desks.has_value() && all_desks.value().size() > 1) {
    for (const Desk* entry : all_desks.value()) {
      if (entry && entry->uuid() != exclude_desk_uuid) {
        base::Uuid uuid_to_remove = entry->uuid();
        GetDesksClient()->RemoveDesk(uuid_to_remove,
                                     ash::DeskCloseType::kCloseAllWindows);
      }
    }
  }
}

void FloatingWorkspaceService::MaybeSignOutOfCurrentSession() {
  auto* latest_floating_workspace = GetLatestFloatingWorkspaceTemplate();
  if (latest_floating_workspace == nullptr) {
    return;
  }
  // Checks if the latest uploaded floating workspace template is a captured
  // template from this device and sign out of this session if it is not. Note:
  // we are comparing the last activity time for the user here with the template
  // that we just got. Since `last_activity_time` is in timeticks and the
  // template time is in time, we need to do some manually conversion with
  // Time. Note: this time_delta is strictly > 0 but can be smaller than wall
  // clock time difference. Some additional time buffer (using the 30s from the
  // periodic capture job) is added to account for clock drifts from device to
  // device.
  base::TimeDelta time_delta =
      ui::UserActivityDetector::Get()->last_activity_time() -
      initialization_timeticks_;

  if (latest_floating_workspace->client_cache_guid() !=
          desk_sync_service_->GetDeskModel()->GetCacheGuid() &&
      latest_floating_workspace->GetLastUpdatedTime() >
          initialization_time_ +
              (time_delta.is_positive() ? time_delta : base::Seconds(0)) +
              ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds
                  .Get()) {
    VLOG(1) << "Another device uploaded a template, logging out.";
    chrome::AttemptUserExit();
  }
}

void FloatingWorkspaceService::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  // Set the cache readiness to false. If this is happening, then it's very
  // likely the service will be destroyed soon.
  is_cache_ready_ = false;
  app_cache_obs_.Reset();
}

bool FloatingWorkspaceService::AreRequiredAppTypesInitialized() {
  if (!app_cache_obs_.IsObserving()) {
    return false;
  }
  apps::AppRegistryCache* cache =
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(
          multi_user_util::GetAccountIdFromProfile(profile_));
  DCHECK(cache);
  const std::set<apps::AppType>& initialized_types =
      cache->InitializedAppTypes();
  if (!initialized_types.contains(apps::AppType::kWeb)) {
    return false;
  }
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return (
      initialized_types.contains(apps::AppType::kStandaloneBrowser) &&
      initialized_types.contains(apps::AppType::kStandaloneBrowserChromeApp));
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return initialized_types.contains(apps::AppType::kChromeApp);
#endif
}

void FloatingWorkspaceService::OnAppTypeInitialized(apps::AppType app_type) {
  // If the cache is already ready we don't need to check for additional app
  // type initialization.
  if (is_cache_ready_) {
    return;
  }
  is_cache_ready_ = AreRequiredAppTypesInitialized();
  // If we're here it means that we have floating workspace template to be
  // launched, but until this point the AppRegistryCache wasn't ready.
  if (is_cache_ready_ && should_launch_on_ready_ && should_run_restore_) {
    StopProgressBarAndRestoreFloatingWorkspace();
  }
}

void FloatingWorkspaceService::OnAppRegistryCacheAdded(
    const AccountId& account_id) {
  if (account_id != multi_user_util::GetAccountIdFromProfile(profile_) ||
      app_cache_obs_.IsObserving()) {
    return;
  }
  auto* apps_cache =
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id);
  app_cache_obs_.Observe(apps_cache);
  is_cache_ready_ = AreRequiredAppTypesInitialized();
}

void FloatingWorkspaceService::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  VLOG(1) << "Active User session changed for fws";
  Profile* active_profile =
      ash::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  // Stop the capture if the switched user is not the profile we logged in with.
  // Set up the observers again if we switched back to the profile we logged in
  // with.
  if (active_profile != profile_) {
    ShutDownServicesAndObservers();
  } else {
    SetUpServiceAndObservers(SyncServiceFactory::GetForProfile(profile_),
                             DeskSyncServiceFactory::GetForProfile(profile_));
  }
}

void FloatingWorkspaceService::ShutDownServicesAndObservers() {
  // Remove `this` service as an observer so we do not run into an issue where
  // chrome sync data is downloaded and the capture is kicked started after we
  // stopped the capture timer below.
  if (sync_service_ && sync_service_->HasObserver(this)) {
    sync_service_->RemoveObserver(this);
  }
  // If we don't have an apps cache then we observe the wrapper to
  // wait for it to be ready.
  if (app_cache_obs_.IsObserving()) {
    app_cache_obs_.Reset();
  }
  if (app_cache_wrapper_obs_.IsObserving()) {
    app_cache_wrapper_obs_.Reset();
  }
  if (timer_.IsRunning()) {
    StopCaptureAndUploadActiveDesk();
  }
}

void FloatingWorkspaceService::SetUpServiceAndObservers(
    syncer::SyncService* sync_service,
    desks_storage::DeskSyncService* desk_sync_service) {
  sync_service_ = sync_service;
  desk_sync_service_ = desk_sync_service;
  if (sync_service_ && !sync_service_->HasObserver(this)) {
    sync_service_->AddObserver(this);
  }
  // If we don't have an apps cache then we observe the wrapper to
  // wait for it to be ready.
  auto& apps_cache_wrapper = apps::AppRegistryCacheWrapper::Get();
  DCHECK(&apps_cache_wrapper);
  auto* apps_cache = apps_cache_wrapper.GetAppRegistryCache(
      multi_user_util::GetAccountIdFromProfile(profile_));
  if (apps_cache) {
    app_cache_obs_.Observe(apps_cache);
  } else {
    app_cache_wrapper_obs_.Observe(&apps_cache_wrapper);
  }
  is_cache_ready_ = AreRequiredAppTypesInitialized();
  // Explicitly start the capture if we do not need to run restore. This means
  // we had already gone through the restore logic before a profile switch and
  // won't go through the restore procedure to start the capture. So instead,
  // just start capturing.
  if (!should_run_restore_) {
    StartCaptureAndUploadActiveDesk();
  }
}
}  // namespace ash
