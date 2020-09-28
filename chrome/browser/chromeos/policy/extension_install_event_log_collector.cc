// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/extension_install_event_log_collector.h"

#include "base/command_line.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/enterprise/reporting/extension_info.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/forced_extensions/force_installed_tracker.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "extensions/browser/extension_system.h"

namespace em = enterprise_management;

namespace policy {

namespace {

std::unique_ptr<em::ExtensionInstallReportLogEvent> CreateSessionChangeEvent(
    em::ExtensionInstallReportLogEvent::SessionStateChangeType type) {
  auto event = std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->set_event_type(
      em::ExtensionInstallReportLogEvent::SESSION_STATE_CHANGE);
  event->set_session_state_change_type(type);
  return event;
}

bool GetOnlineState() {
  chromeos::NetworkStateHandler::NetworkStateList network_state_list;
  chromeos::NetworkHandler::Get()
      ->network_state_handler()
      ->GetNetworkListByType(
          chromeos::NetworkTypePattern::Default(), true /* configured_only */,
          false /* visible_only */, 0 /* limit */, &network_state_list);
  for (const chromeos::NetworkState* network_state : network_state_list) {
    if (network_state->connection_state() == shill::kStateOnline) {
      return true;
    }
  }
  return false;
}

// Helper method to convert InstallStageTracker::FailureReason to the failure
// reason proto.
em::ExtensionInstallReportLogEvent_FailureReason ConvertFailureReasonToProto(
    extensions::InstallStageTracker::FailureReason failure_reason) {
  switch (failure_reason) {
    case extensions::InstallStageTracker::FailureReason::UNKNOWN:
      return em::ExtensionInstallReportLogEvent::FAILURE_REASON_UNKNOWN;
    case extensions::InstallStageTracker::FailureReason::INVALID_ID:
      return em::ExtensionInstallReportLogEvent::INVALID_ID;
    case extensions::InstallStageTracker::FailureReason::
        MALFORMED_EXTENSION_SETTINGS:
      return em::ExtensionInstallReportLogEvent::MALFORMED_EXTENSION_SETTINGS;
    case extensions::InstallStageTracker::FailureReason::REPLACED_BY_ARC_APP:
      return em::ExtensionInstallReportLogEvent::REPLACED_BY_ARC_APP;
    case extensions::InstallStageTracker::FailureReason::
        MALFORMED_EXTENSION_DICT:
      return em::ExtensionInstallReportLogEvent::MALFORMED_EXTENSION_DICT;
    case extensions::InstallStageTracker::FailureReason::
        NOT_SUPPORTED_EXTENSION_DICT:
      return em::ExtensionInstallReportLogEvent::NOT_SUPPORTED_EXTENSION_DICT;
    case extensions::InstallStageTracker::FailureReason::
        MALFORMED_EXTENSION_DICT_FILE_PATH:
      return em::ExtensionInstallReportLogEvent::
          MALFORMED_EXTENSION_DICT_FILE_PATH;
    case extensions::InstallStageTracker::FailureReason::
        MALFORMED_EXTENSION_DICT_VERSION:
      return em::ExtensionInstallReportLogEvent::
          MALFORMED_EXTENSION_DICT_VERSION;
    case extensions::InstallStageTracker::FailureReason::
        MALFORMED_EXTENSION_DICT_UPDATE_URL:
      return em::ExtensionInstallReportLogEvent::
          MALFORMED_EXTENSION_DICT_UPDATE_URL;
    case extensions::InstallStageTracker::FailureReason::LOCALE_NOT_SUPPORTED:
      return em::ExtensionInstallReportLogEvent::LOCALE_NOT_SUPPORTED;
    case extensions::InstallStageTracker::FailureReason::
        NOT_PERFORMING_NEW_INSTALL:
      return em::ExtensionInstallReportLogEvent::NOT_PERFORMING_NEW_INSTALL;
    case extensions::InstallStageTracker::FailureReason::TOO_OLD_PROFILE:
      return em::ExtensionInstallReportLogEvent::TOO_OLD_PROFILE;
    case extensions::InstallStageTracker::FailureReason::
        DO_NOT_INSTALL_FOR_ENTERPRISE:
      return em::ExtensionInstallReportLogEvent::DO_NOT_INSTALL_FOR_ENTERPRISE;
    case extensions::InstallStageTracker::FailureReason::ALREADY_INSTALLED:
      return em::ExtensionInstallReportLogEvent::ALREADY_INSTALLED;
    case extensions::InstallStageTracker::FailureReason::CRX_FETCH_FAILED:
      return em::ExtensionInstallReportLogEvent::CRX_FETCH_FAILED;
    case extensions::InstallStageTracker::FailureReason::MANIFEST_FETCH_FAILED:
      return em::ExtensionInstallReportLogEvent::MANIFEST_FETCH_FAILED;
    case extensions::InstallStageTracker::FailureReason::MANIFEST_INVALID:
      return em::ExtensionInstallReportLogEvent::MANIFEST_INVALID;
    case extensions::InstallStageTracker::FailureReason::NO_UPDATE:
      return em::ExtensionInstallReportLogEvent::NO_UPDATE;
    case extensions::InstallStageTracker::FailureReason::
        CRX_INSTALL_ERROR_DECLINED:
      return em::ExtensionInstallReportLogEvent::CRX_INSTALL_ERROR_DECLINED;
    case extensions::InstallStageTracker::FailureReason::
        CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE:
      return em::ExtensionInstallReportLogEvent::
          CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE;
    case extensions::InstallStageTracker::FailureReason::
        CRX_INSTALL_ERROR_OTHER:
      return em::ExtensionInstallReportLogEvent::CRX_INSTALL_ERROR_OTHER;
    case extensions::InstallStageTracker::FailureReason::NO_UPDATE_URL:
      return em::ExtensionInstallReportLogEvent::NO_UPDATE_URL;
    case extensions::InstallStageTracker::FailureReason::PENDING_ADD_FAILED:
      return em::ExtensionInstallReportLogEvent::PENDING_ADD_FAILED;
    case extensions::InstallStageTracker::FailureReason::DOWNLOADER_ADD_FAILED:
      return em::ExtensionInstallReportLogEvent::DOWNLOADER_ADD_FAILED;
    case extensions::InstallStageTracker::FailureReason::IN_PROGRESS:
      return em::ExtensionInstallReportLogEvent::IN_PROGRESS;
    case extensions::InstallStageTracker::FailureReason::CRX_FETCH_URL_EMPTY:
      return em::ExtensionInstallReportLogEvent::CRX_FETCH_URL_EMPTY;
    case extensions::InstallStageTracker::FailureReason::CRX_FETCH_URL_INVALID:
      return em::ExtensionInstallReportLogEvent::CRX_FETCH_URL_INVALID;
    case extensions::InstallStageTracker::FailureReason::OVERRIDDEN_BY_SETTINGS:
      return em::ExtensionInstallReportLogEvent::OVERRIDDEN_BY_SETTINGS;
    default:
      NOTREACHED();
  }
}

// Helper method to convert InstallStageTracker::Stage to the Stage proto.
em::ExtensionInstallReportLogEvent_InstallationStage
ConvertInstallationStageToProto(extensions::InstallStageTracker::Stage stage) {
  using Stage = extensions::InstallStageTracker::Stage;
  switch (stage) {
    case Stage::CREATED:
      return em::ExtensionInstallReportLogEvent::CREATED;
    case Stage::PENDING:
      return em::ExtensionInstallReportLogEvent::PENDING;
    case Stage::DOWNLOADING:
      return em::ExtensionInstallReportLogEvent::DOWNLOADING;
    case Stage::INSTALLING:
      return em::ExtensionInstallReportLogEvent::INSTALLING;
    case Stage::COMPLETE:
      return em::ExtensionInstallReportLogEvent::COMPLETE;
    default:
      NOTREACHED();
      return em::ExtensionInstallReportLogEvent::INSTALLATION_STAGE_UNKNOWN;
  }
}

// Helper method to convert InstallStageTracker::UserType to the user
// type proto.
em::ExtensionInstallReportLogEvent_UserType ConvertUserTypeToProto(
    user_manager::UserType user_type) {
  switch (user_type) {
    case user_manager::USER_TYPE_REGULAR:
      return em::ExtensionInstallReportLogEvent::USER_TYPE_REGULAR;
    case user_manager::USER_TYPE_GUEST:
      return em::ExtensionInstallReportLogEvent::USER_TYPE_GUEST;
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
      return em::ExtensionInstallReportLogEvent::USER_TYPE_PUBLIC_ACCOUNT;
    case user_manager::USER_TYPE_SUPERVISED:
      return em::ExtensionInstallReportLogEvent::USER_TYPE_SUPERVISED;
    case user_manager::USER_TYPE_KIOSK_APP:
      return em::ExtensionInstallReportLogEvent::USER_TYPE_KIOSK_APP;
    case user_manager::USER_TYPE_CHILD:
      return em::ExtensionInstallReportLogEvent::USER_TYPE_CHILD;
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
      return em::ExtensionInstallReportLogEvent::USER_TYPE_ARC_KIOSK_APP;
    case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
      return em::ExtensionInstallReportLogEvent::USER_TYPE_ACTIVE_DIRECTORY;
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
      return em::ExtensionInstallReportLogEvent::USER_TYPE_WEB_KIOSK_APP;
    case user_manager::NUM_USER_TYPES:
      NOTREACHED();
      return em::ExtensionInstallReportLogEvent::USER_TYPE_UNKNOWN;
  }
}

// Helper method to convert ExtensionDownloaderDelegate::Stage to the
// DownloadingStage proto.
em::ExtensionInstallReportLogEvent_DownloadingStage
ConvertDownloadingStageToProto(
    extensions::ExtensionDownloaderDelegate::Stage stage) {
  using DownloadingStage = extensions::ExtensionDownloaderDelegate::Stage;
  switch (stage) {
    case DownloadingStage::PENDING:
      return em::ExtensionInstallReportLogEvent::DOWNLOAD_PENDING;
    case DownloadingStage::QUEUED_FOR_MANIFEST:
      return em::ExtensionInstallReportLogEvent::QUEUED_FOR_MANIFEST;
    case DownloadingStage::DOWNLOADING_MANIFEST:
      return em::ExtensionInstallReportLogEvent::DOWNLOADING_MANIFEST;
    case DownloadingStage::DOWNLOADING_MANIFEST_RETRY:
      return em::ExtensionInstallReportLogEvent::DOWNLOADING_MANIFEST_RETRY;
    case DownloadingStage::PARSING_MANIFEST:
      return em::ExtensionInstallReportLogEvent::PARSING_MANIFEST;
    case DownloadingStage::MANIFEST_LOADED:
      return em::ExtensionInstallReportLogEvent::MANIFEST_LOADED;
    case DownloadingStage::QUEUED_FOR_CRX:
      return em::ExtensionInstallReportLogEvent::QUEUED_FOR_CRX;
    case DownloadingStage::DOWNLOADING_CRX:
      return em::ExtensionInstallReportLogEvent::DOWNLOADING_CRX;
    case DownloadingStage::DOWNLOADING_CRX_RETRY:
      return em::ExtensionInstallReportLogEvent::DOWNLOADING_CRX_RETRY;
    case DownloadingStage::FINISHED:
      return em::ExtensionInstallReportLogEvent::FINISHED;
    default:
      NOTREACHED();
      return em::ExtensionInstallReportLogEvent::DOWNLOADING_STAGE_UNKNOWN;
  }
}

em::ExtensionInstallReportLogEvent_InstallCreationStage
ConvertInstallCreationStageToProto(
    extensions::InstallStageTracker::InstallCreationStage stage) {
  using Stage = extensions::InstallStageTracker::InstallCreationStage;
  switch (stage) {
    case Stage::CREATION_INITIATED:
      return em::ExtensionInstallReportLogEvent::CREATION_INITIATED;
    case Stage::NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_FORCED:
      return em::ExtensionInstallReportLogEvent::
          NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_FORCED;
    case Stage::NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_NOT_FORCED:
      return em::ExtensionInstallReportLogEvent::
          NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_NOT_FORCED;
    case Stage::NOTIFIED_FROM_MANAGEMENT:
      return em::ExtensionInstallReportLogEvent::NOTIFIED_FROM_MANAGEMENT;
    case Stage::NOTIFIED_FROM_MANAGEMENT_NOT_FORCED:
      return em::ExtensionInstallReportLogEvent::
          NOTIFIED_FROM_MANAGEMENT_NOT_FORCED;
    case Stage::SEEN_BY_POLICY_LOADER:
      return em::ExtensionInstallReportLogEvent::SEEN_BY_POLICY_LOADER;
    case Stage::SEEN_BY_EXTERNAL_PROVIDER:
      return em::ExtensionInstallReportLogEvent::SEEN_BY_EXTERNAL_PROVIDER;
    default:
      NOTREACHED();
      return em::ExtensionInstallReportLogEvent::INSTALL_CREATION_STAGE_UNKNOWN;
  }
}

}  // namespace

ExtensionInstallEventLogCollector::ExtensionInstallEventLogCollector(
    extensions::ExtensionRegistry* registry,
    Delegate* delegate,
    Profile* profile)
    : registry_(registry),
      delegate_(delegate),
      profile_(profile),
      online_(GetOnlineState()) {
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  registry_observer_.Add(registry_);
  stage_tracker_observer_.Add(extensions::InstallStageTracker::Get(profile_));
}

ExtensionInstallEventLogCollector::~ExtensionInstallEventLogCollector() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
}

void ExtensionInstallEventLogCollector::AddLoginEvent() {
  // Don't log in case session is restarted or recovered from crash.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kLoginUser) ||
      profile_->GetLastSessionExitType() == Profile::EXIT_CRASHED) {
    return;
  }
  online_ = GetOnlineState();
  std::unique_ptr<em::ExtensionInstallReportLogEvent> event =
      CreateSessionChangeEvent(em::ExtensionInstallReportLogEvent::LOGIN);
  if (chromeos::ProfileHelper::Get()->GetUserByProfile(profile_)) {
    extensions::InstallStageTracker::UserInfo user_info =
        extensions::InstallStageTracker::GetUserInfo(profile_);
    event->set_user_type(ConvertUserTypeToProto(user_info.user_type));
    event->set_is_new_user(user_info.is_new_user);
  }
  event->set_online(online_);
  delegate_->AddForAllExtensions(std::move(event));
}

void ExtensionInstallEventLogCollector::AddLogoutEvent() {
  // Don't log in case session is restared.
  if (g_browser_process->local_state()->GetBoolean(prefs::kWasRestarted))
    return;

  delegate_->AddForAllExtensions(
      CreateSessionChangeEvent(em::ExtensionInstallReportLogEvent::LOGOUT));
}

void ExtensionInstallEventLogCollector::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  delegate_->AddForAllExtensions(
      CreateSessionChangeEvent(em::ExtensionInstallReportLogEvent::SUSPEND));
}

void ExtensionInstallEventLogCollector::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  delegate_->AddForAllExtensions(
      CreateSessionChangeEvent(em::ExtensionInstallReportLogEvent::RESUME));
}

void ExtensionInstallEventLogCollector::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  const bool currently_online = GetOnlineState();
  if (currently_online == online_)
    return;
  online_ = currently_online;

  std::unique_ptr<em::ExtensionInstallReportLogEvent> event =
      std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->set_event_type(
      em::ExtensionInstallReportLogEvent::CONNECTIVITY_CHANGE);
  event->set_online(online_);
  delegate_->AddForAllExtensions(std::move(event));
}

void ExtensionInstallEventLogCollector::OnExtensionInstallationFailed(
    const extensions::ExtensionId& extension_id,
    extensions::InstallStageTracker::FailureReason reason) {
  if (!delegate_->IsExtensionPending(extension_id))
    return;
  auto event = std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->set_event_type(
      em::ExtensionInstallReportLogEvent::INSTALLATION_FAILED);
  event->set_failure_reason(ConvertFailureReasonToProto(reason));
  extensions::InstallStageTracker* install_stage_tracker =
      extensions::InstallStageTracker::Get(profile_);
  extensions::InstallStageTracker::InstallationData data =
      install_stage_tracker->Get(extension_id);
  if (data.extension_type) {
    event->set_extension_type(enterprise_reporting::ConvertExtensionTypeToProto(
        data.extension_type.value()));
  }
  extensions::ForceInstalledTracker* force_installed_tracker =
      extensions::ExtensionSystem::Get(profile_)
          ->extension_service()
          ->force_installed_tracker();
  event->set_is_misconfiguration_failure(
      force_installed_tracker->IsMisconfiguration(data, extension_id));
  delegate_->Add(extension_id, true /* gather_disk_space_info */,
                 std::move(event));
  delegate_->OnExtensionInstallationFinished(extension_id);
}

void ExtensionInstallEventLogCollector::OnExtensionInstallationStageChanged(
    const extensions::ExtensionId& id,
    extensions::InstallStageTracker::Stage stage) {
  if (!delegate_->IsExtensionPending(id))
    return;
  auto event = std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->set_installation_stage(ConvertInstallationStageToProto(stage));
  delegate_->Add(id, true /* gather_disk_space_info */, std::move(event));
}

void ExtensionInstallEventLogCollector::OnExtensionDownloadingStageChanged(
    const extensions::ExtensionId& id,
    extensions::ExtensionDownloaderDelegate::Stage stage) {
  if (!delegate_->IsExtensionPending(id))
    return;
  auto event = std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->set_downloading_stage(ConvertDownloadingStageToProto(stage));
  delegate_->Add(id, true /* gather_disk_space_info */, std::move(event));
}

void ExtensionInstallEventLogCollector::OnExtensionInstallCreationStageChanged(
    const extensions::ExtensionId& id,
    extensions::InstallStageTracker::InstallCreationStage stage) {
  if (!delegate_->IsExtensionPending(id))
    return;
  auto event = std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->set_install_creation_stage(ConvertInstallCreationStageToProto(stage));
  delegate_->Add(id, false /* gather_disk_space_info */, std::move(event));
}

void ExtensionInstallEventLogCollector::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (!delegate_->IsExtensionPending(extension->id()))
    return;
  AddSuccessEvent(extension);
}

void ExtensionInstallEventLogCollector::OnExtensionsRequested(
    const extensions::ExtensionIdSet& extension_ids) {
  for (const auto& extension_id : extension_ids) {
    const extensions::Extension* extension = registry_->GetExtensionById(
        extension_id, extensions::ExtensionRegistry::ENABLED);
    if (extension)
      AddSuccessEvent(extension);
  }
}

void ExtensionInstallEventLogCollector::AddSuccessEvent(
    const extensions::Extension* extension) {
  auto event = std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  event->set_extension_type(
      enterprise_reporting::ConvertExtensionTypeToProto(extension->GetType()));
  delegate_->Add(extension->id(), true /* gather_disk_space_info */,
                 std::move(event));
  delegate_->OnExtensionInstallationFinished(extension->id());
}

}  // namespace policy
