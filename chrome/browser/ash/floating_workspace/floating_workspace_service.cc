// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_workspace/floating_workspace_service.h"

#include <cstddef>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_metrics_util.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_service_factory.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sync/desk_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom-forward.h"
#include "chrome/grit/generated_resources.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/desks_storage/core/desk_sync_bridge.h"
#include "components/desks_storage/core/desk_sync_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

const char kNotificationForNoNetworkConnection[] =
    "notification_no_network_connection";
const char kNotificationForSyncErrorOrTimeOut[] =
    "notification_sync_error_or_timeout";
const char kNotificationForRestoreAfterError[] =
    "notification_restore_after_error";

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
      initialization_timestamp_(base::TimeTicks::Now()) {}

FloatingWorkspaceService::~FloatingWorkspaceService() {
  if (is_testing_)
    return;
  if (floating_workspace_util::IsFloatingWorkspaceV2Enabled()) {
    StopCaptureAndUploadActiveDesk();
  }
}

void FloatingWorkspaceService::Init(
    syncer::SyncService* sync_service,
    desks_storage::DeskSyncService* desk_sync_service) {
  if (is_testing_) {
    CHECK_IS_TEST();
    if (version_ == floating_workspace_util::FloatingWorkspaceVersion::
                        kFloatingWorkspaceV1Enabled) {
      InitForV1();
    } else {
      InitForV2(sync_service, desk_sync_service);
    }
    return;
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
      saved_desk_util::AreDesksTemplatesEnabled() &&
      features::IsDeskTemplateSyncEnabled() &&
      floating_workspace_util::IsFloatingWorkspaceV2Enabled()) {
    InitForV2(sync_service, desk_sync_service);
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

void FloatingWorkspaceService::OnStateChanged(syncer::SyncService* sync) {
  if (!should_run_restore_) {
    return;
  }
  switch (sync->GetDownloadStatusFor(syncer::ModelType::WORKSPACE_DESK)) {
    case syncer::SyncService::ModelTypeDownloadStatus::kWaitingForUpdates: {
      // Floating Workspace Service needs to Wait until workspace desks are up
      // to date.
      break;
    }
    case syncer::SyncService::ModelTypeDownloadStatus::kUpToDate: {
      RestoreFloatingWorkspaceTemplate(GetLatestFloatingWorkspaceTemplate());
      break;
    }
    case syncer::SyncService::ModelTypeDownloadStatus::kError: {
      // Sync is not expected to deliver the data, let user decide.
      // TODO: send notification to user asking if restore local.
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
      break;
    case FloatingWorkspaceServiceNotificationType::kSyncErrorOrTimeOut:
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
        LaunchFloatingWorkspaceTemplate(GetLatestFloatingWorkspaceTemplate());
      }
      break;
  }
  MaybeCloseNotification();
}

void FloatingWorkspaceService::MaybeCloseNotification() {
  if (notification_ == nullptr) {
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
  sync_service_ = sync_service;
  desk_sync_service_ = desk_sync_service;
  sync_service_->AddObserver(this);
  StartCaptureAndUploadActiveDesk();
  // Post a task to check if anything is restored after FWS timeout.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FloatingWorkspaceService::MaybeHandleDownloadTimeOut,
                     weak_pointer_factory_.GetWeakPtr()),
      ash::features::kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin
          .Get());
  if (!floating_workspace_util::IsInternetConnected()) {
    SendNotification(kNotificationForNoNetworkConnection);
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

const DeskTemplate*
FloatingWorkspaceService::GetLatestFloatingWorkspaceTemplate() {
  desks_storage::DeskModel::GetAllEntriesResult result =
      desk_sync_service_->GetDeskModel()->GetAllEntries();
  if (result.status != desks_storage::DeskModel::GetAllEntriesStatus::kOk) {
    return nullptr;
  }
  const DeskTemplate* floating_workspace_template = nullptr;
  for (const DeskTemplate* desk_template : result.entries) {
    if (desk_template &&
        desk_template->type() == DeskTemplateType::kFloatingWorkspace) {
      // Set the to be floating workspace template to the latest floating
      // workspace template found.
      if (!floating_workspace_template ||
          floating_workspace_template->GetLastUpdatedTime() <
              desk_template->GetLastUpdatedTime()) {
        floating_workspace_template = desk_template;
      }
    }
  }
  return floating_workspace_template;
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

// TODO(b/274502821): create garbage collection method for stale floating
// workspace templates.
void FloatingWorkspaceService::RestoreFloatingWorkspaceTemplate(
    const DeskTemplate* desk_template) {
  if (desk_template == nullptr) {
    should_run_restore_ = false;
    return;
  }
  // Record metrics for window and tab count and also the time it took to
  // download the floating workspace template.
  floating_workspace_metrics_util::RecordFloatingWorkspaceV2TemplateLoadTime(
      base::TimeTicks::Now() - initialization_timestamp_);
  RecordWindowAndTabCountHistogram(*desk_template);
  // Check if template has been downloaded after
  // kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin.
  if (base::TimeTicks::Now() >
      initialization_timestamp_ +
          ash::features::
              kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin.Get()) {
    // Template arrives late, asking user to restore or not.
    SendNotification(kNotificationForRestoreAfterError);
    // Set this flag false after sending restore notification to user
    // since user will control the restoration behavior from then on.
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
  GetDesksClient()->LaunchDeskTemplate(
      desk_template->uuid(),
      base::BindOnce(&FloatingWorkspaceService::OnTemplateLaunched,
                     weak_pointer_factory_.GetWeakPtr()),
      desk_template->template_name());
}

DesksClient* FloatingWorkspaceService::GetDesksClient() {
  return DesksClient::Get();
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
  // Check if there's an associated floating workspace uuid from the desk sync
  // bridge. If there is, use that one. The `floating_workspace_uuid_ is
  // populated once during the first capture of the session if there is known
  // information from the sync bridge and the info may be outdated for the sync
  // bridge. However, the sync bridge does not need to know the new uuid since
  // the current service will handle it. Ignore for testing.
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

  // If successfully captured desk, remove old entry and record new uuid.
  if (!IsCurrentDeskSameAsPrevious(desk_template.get())) {
    UploadFloatingWorkspaceTemplateToDeskModel(std::move(desk_template));
  }
}

void FloatingWorkspaceService::UploadFloatingWorkspaceTemplateToDeskModel(
    std::unique_ptr<DeskTemplate> desk_template) {
  // Upload and save the template.
  desk_sync_service_->GetDeskModel()->AddOrUpdateEntry(
      std::move(desk_template),
      base::BindOnce(&FloatingWorkspaceService::OnTemplateUploaded,
                     weak_pointer_factory_.GetWeakPtr()));
}

void FloatingWorkspaceService::OnTemplateUploaded(
    desks_storage::DeskModel::AddOrUpdateEntryStatus status,
    std::unique_ptr<DeskTemplate> new_entry) {
  previously_captured_desk_template_ = std::move(new_entry);
  floating_workspace_metrics_util::
      RecordFloatingWorkspaceV2TemplateUploadStatusHistogram(status);
}

absl::optional<base::Uuid>
FloatingWorkspaceService::GetFloatingWorkspaceUuidForCurrentDevice() {
  std::string cache_guid = desk_sync_service_->GetDeskModel()->GetCacheGuid();
  std::vector<const DeskTemplate*> entries =
      desk_sync_service_->GetDeskModel()->GetAllEntries().entries;
  auto iter = base::ranges::find_if(entries, [cache_guid](const auto& entry) {
    return entry->client_cache_guid() == cache_guid;
  });
  if (iter == entries.end()) {
    return absl::nullopt;
  }
  return (*iter)->uuid();
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
    case FloatingWorkspaceServiceNotificationType::kUnknown:
      VLOG(2) << "Unknown notification type for floating workspace, skip "
                 "sending notification";
      return;
  }

  notification_ = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, id, title, message,
      l10n_util::GetStringUTF16(IDS_FLOATING_WORKSPACE_DISPLAY_SOURCE), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 id,
                                 NotificationCatalogName::kFloatingWorkspace),
      notification_data,
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          weak_pointer_factory_.GetWeakPtr()),
      kFloatingWorkspaceNotificationIcon, warning_level);
  notification_->set_priority(message_center::SYSTEM_PRIORITY);
  auto* notification_display_service =
      NotificationDisplayService::GetForProfile(profile_);
  notification_display_service->Display(NotificationHandler::Type::TRANSIENT,
                                        *notification_,
                                        /*metadata=*/nullptr);
}

}  // namespace ash
