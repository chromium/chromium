// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/apn_migrator.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/containers/contains.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

namespace {

using chromeos::network_config::mojom::ApnPropertiesPtr;
using chromeos::network_config::mojom::ApnState;
using chromeos::network_config::mojom::ApnType;
using chromeos::network_config::mojom::ManagedApnPropertiesPtr;

absl::optional<ApnPropertiesPtr> GetPreRevampApnFromDict(
    const base::Value::Dict* cellular_dict,
    const char* key) {
  const base::Value::Dict* apn_dict =
      chromeos::network_config::GetDictionary(cellular_dict, key);
  if (!apn_dict) {
    return absl::nullopt;
  }

  // Pre-revamp APNs with empty kAccessPointName will be ignored as they
  // indicate shill tried to send a NULL APN to modemmanager. If shill
  // uses a custom APN or modem DB APN, the kAccessPointName will be
  // non-empty.
  const std::string* access_point_name =
      apn_dict->FindString(::onc::cellular_apn::kAccessPointName);
  if (!access_point_name || access_point_name->empty()) {
    return absl::nullopt;
  }

  return chromeos::network_config::GetApnProperties(
      *apn_dict,
      /*is_apn_revamp_enabled=*/false);
}

bool ContainsMatchingApn(const base::Value::Dict* cellular_dict,
                         const std::string& access_point_name) {
  chromeos::network_config::mojom::ManagedApnListPtr apn_list =
      chromeos::network_config::GetManagedApnList(
          cellular_dict->Find(::onc::cellular::kAPNList),
          ash::features::IsApnRevampEnabled());
  for (const auto& apn : apn_list->active_value) {
    if (apn->access_point_name == access_point_name) {
      return true;
    }
  }
  return false;
}

std::vector<ApnType> GetMigratedApnTypes(
    const ApnPropertiesPtr& pre_revamp_apn) {
  if (pre_revamp_apn->attach.has_value() &&
      !(*pre_revamp_apn->attach).empty()) {
    return {ApnType::kDefault, ApnType::kAttach};
  }
  return {ApnType::kDefault};
}

// Clicking on the notification will bring the user to the APN subpage.
void ShowApnConfigurationDisabledNotification(
    const std::string& access_point_name,
    const std::string& guid) {
  const std::string notification_id =
      ApnMigrator::kShowApnConfigurationDisabledNotificationIdPrefix + guid;
  auto on_click = base::BindRepeating(
      [](const std::string& guid, const std::string& notification_id) {
        message_center::MessageCenter::Get()->RemoveNotification(
            notification_id, /*by_user=*/false);
        const std::string apn_subpage =
            ::chromeos::settings::mojom::kApnSubpagePath +
            std::string("?guid=") +
            base::EscapeUrlEncodedData(guid, /*use_plus=*/true);
        chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
            ProfileManager::GetActiveUserProfile(), apn_subpage);
      },
      guid, notification_id);

  // TODO(b/162365553): Get final strings after string meeting.
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotificationPtr(
          message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
          notification_id,
          u"Title for " + base::ASCIIToUTF16(access_point_name),
          u"Message for " + base::ASCIIToUTF16(access_point_name),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT, notification_id,
              ash::NotificationCatalogName::kMobileData),
          message_center::RichNotificationData(),
          new message_center::HandleNotificationClickDelegate(on_click),
          kNotificationCellularAlertIcon,
          message_center::SystemNotificationWarningLevel::WARNING);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

}  // namespace

// static
const char ApnMigrator::kShowApnConfigurationDisabledNotificationIdPrefix[] =
    "show_apn_configuration_disabled_notification_";

ApnMigrator::ApnMigrator(
    ManagedCellularPrefHandler* managed_cellular_pref_handler,
    ManagedNetworkConfigurationHandler* network_configuration_handler,
    NetworkStateHandler* network_state_handler)
    : managed_cellular_pref_handler_(managed_cellular_pref_handler),
      network_configuration_handler_(network_configuration_handler),
      network_state_handler_(network_state_handler) {
  if (!NetworkHandler::IsInitialized()) {
    return;
  }
  // TODO(b/162365553): Only bind this lazily when CrosNetworkConfig is actually
  // used.
  ash::GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  network_state_handler_observer_.Observe(network_state_handler_.get());
}

ApnMigrator::~ApnMigrator() = default;

void ApnMigrator::NetworkListChanged() {
  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::Cellular(), &network_list);
  for (const NetworkState* network : network_list) {
    // Only attempt to migrate networks known by Shill.
    if (network->IsNonShillCellularNetwork()) {
      continue;
    }

    // The network has already been updated in Shill with the correct logic
    // depending on if the flag is enabled or disabled. Finish early so we don't
    // redundantly update Shill.
    if (base::Contains(shill_updated_iccids_, network->iccid())) {
      continue;
    }

    bool has_network_been_migrated =
        managed_cellular_pref_handler_->ContainsApnMigratedIccid(
            network->iccid());
    if (!ash::features::IsApnRevampEnabled()) {
      // If the network has been marked as migrated, but the ApnRevamp flag is
      // disabled, the flag was disabled after being enabled. Clear
      // CustomApnList so that Shill knows to use legacy APN selection logic.
      if (has_network_been_migrated) {
        NET_LOG(EVENT) << "Network has been migrated but the revamp flag is "
                       << "disabled. Clearing CustomAPNList: "
                       << network->iccid();
        network_configuration_handler_->ClearShillProperties(
            network->path(), {shill::kCellularCustomApnListProperty},
            base::BindOnce(&ApnMigrator::OnClearPropertiesSuccess,
                           weak_factory_.GetWeakPtr(), network->iccid()),
            base::BindOnce(&ApnMigrator::OnClearPropertiesFailure,
                           weak_factory_.GetWeakPtr(), network->iccid(),
                           network->guid()));
      }
      continue;
    }

    if (!has_network_been_migrated) {
      NET_LOG(EVENT) << "Network has not been migrated, attempting to migrate: "
                     << network->iccid();
      MigrateNetwork(*network);
      continue;
    }

    // The network has already been migrated, either the last time the flag was
    // on, or this time. Send Shill the revamp APN list.
    if (const base::Value::List* custom_apn_list =
            GetNetworkMetadataStore()->GetCustomApnList(network->guid())) {
      NET_LOG(EVENT) << "Network has already been migrated, setting with the "
                     << "populated custom APN list: " << network->iccid();
      SetShillCustomApnListForNetwork(*network, custom_apn_list);
      continue;
    }

    NET_LOG(EVENT) << "Network has already been migrated, setting with the "
                   << "empty custom APN list: " << network->iccid();
    base::Value::List empty_custom_apn_list;
    SetShillCustomApnListForNetwork(*network, &empty_custom_apn_list);
  }
}

void ApnMigrator::OnClearPropertiesSuccess(const std::string iccid) {
  NET_LOG(EVENT) << "Successfully cleared CustomAPNList for: " << iccid;
  shill_updated_iccids_.emplace(iccid);
}

void ApnMigrator::OnClearPropertiesFailure(const std::string iccid,
                                           const std::string guid,
                                           const std::string& error_name) {
  NET_LOG(ERROR) << "Failed to clear CustomAPNList for: " << iccid;
}

void ApnMigrator::SetShillCustomApnListForNetwork(
    const NetworkState& network,
    const base::Value::List* apn_list) {
  network_configuration_handler_->SetProperties(
      network.path(),
      chromeos::network_config::CustomApnListToOnc(network.guid(), apn_list),
      base::BindOnce(&ApnMigrator::OnSetShillCustomApnListSuccess,
                     weak_factory_.GetWeakPtr(), network.iccid()),
      base::BindOnce(&ApnMigrator::OnSetShillCustomApnListFailure,
                     weak_factory_.GetWeakPtr(), network.iccid(),
                     network.guid()));
}

void ApnMigrator::OnSetShillCustomApnListSuccess(const std::string iccid) {
  // Shill has successfully updated the network with the revamp APN list.
  shill_updated_iccids_.emplace(iccid);
  NET_LOG(EVENT) << "ApnMigrator: Update the custom APN "
                 << "list in Shill for network with ICCID: " << iccid;

  // The network has just been migrated.
  if (!managed_cellular_pref_handler_->ContainsApnMigratedIccid(iccid)) {
    NET_LOG(EVENT) << "ApnMigrator: Mark network with ICCID: " << iccid
                   << " as migrated";
    managed_cellular_pref_handler_->AddApnMigratedIccid(iccid);
    iccids_in_migration_.erase(iccid);
  }
}

void ApnMigrator::OnSetShillCustomApnListFailure(
    const std::string iccid,
    const std::string guid,
    const std::string& error_name) {
  NET_LOG(ERROR) << "ApnMigrator: Failed to update the custom APN "
                 << "list in Shill for network: " << guid << ": [" << error_name
                 << ']';

  iccids_in_migration_.erase(iccid);
}

void ApnMigrator::MigrateNetwork(const NetworkState& network) {
  DCHECK(ash::features::IsApnRevampEnabled());

  // Return early if the network is already in the process of being migrated.
  if (base::Contains(iccids_in_migration_, network.iccid())) {
    NET_LOG(DEBUG) << "Attempting to migrate network that already has a "
                   << "migration in progress, returning early: "
                   << network.iccid();
    return;
  }

  DCHECK(!managed_cellular_pref_handler_->ContainsApnMigratedIccid(
      network.iccid()));

  // Get the pre-revamp APN list.
  const base::Value::List* custom_apn_list =
      GetNetworkMetadataStore()->GetPreRevampCustomApnList(network.guid());

  // If the pre-revamp APN list is empty, set the revamp list as empty and
  // finish the migration.
  if (!custom_apn_list || custom_apn_list->empty()) {
    NET_LOG(EVENT) << "Pre-revamp APN list is empty, sending empty list to "
                   << "Shill: " << network.iccid();
    base::Value::List empty_apn_list;
    SetShillCustomApnListForNetwork(network, &empty_apn_list);
    return;
  }

  // If the pre-revamp APN list is non-empty, get the network's managed
  // properties, to be used for the migration heuristic. This call is
  // asynchronous; mark the ICCID as migrating so that the network won't
  // be attempted to be migrated again while these properties are being fetched.
  iccids_in_migration_.emplace(network.iccid());

  NET_LOG(EVENT) << "Fetching managed properties for network: "
                 << network.iccid();
  network_configuration_handler_->GetManagedProperties(
      LoginState::Get()->primary_user_hash(), network.path(),
      base::BindOnce(&ApnMigrator::OnGetManagedProperties,
                     weak_factory_.GetWeakPtr(), network.iccid(),
                     network.guid()));
}

void ApnMigrator::OnGetManagedProperties(
    std::string iccid,
    std::string guid,
    const std::string& service_path,
    absl::optional<base::Value::Dict> properties,
    absl::optional<std::string> error) {
  if (error.has_value()) {
    NET_LOG(ERROR) << "Error fetching managed properties for " << iccid
                   << ", error: " << error.value();
    iccids_in_migration_.erase(iccid);
    return;
  }

  if (!properties) {
    NET_LOG(ERROR) << "Error fetching managed properties for " << iccid;
    iccids_in_migration_.erase(iccid);
    return;
  }

  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromGuid(guid);
  if (!network) {
    NET_LOG(ERROR) << "Network no longer exists: " << guid;
    iccids_in_migration_.erase(iccid);
    return;
  }

  // Get the pre-revamp APN list.
  const base::Value::List* custom_apn_list =
      GetNetworkMetadataStore()->GetPreRevampCustomApnList(guid);

  // At this point, the pre-revamp APN list should not be empty. However, there
  // could be the case where the custom APN list was cleared during the
  // GetManagedProperties() call. If so, set the revamp list as empty and finish
  // the migration.
  if (!custom_apn_list || custom_apn_list->empty()) {
    NET_LOG(EVENT) << "Custom APN list cleared during GetManagedProperties() "
                   << "call, setting Shill with empty list for network: "
                   << guid;
    base::Value::List empty_apn_list;
    SetShillCustomApnListForNetwork(*network, &empty_apn_list);
    return;
  }

  ApnPropertiesPtr pre_revamp_custom_apn =
      chromeos::network_config::GetApnProperties(
          custom_apn_list->front().GetDict(),
          /*is_apn_revamp_enabled=*/false);
  const base::Value::Dict* cellular_dict =
      chromeos::network_config::GetDictionary(&properties.value(),
                                              ::onc::network_config::kCellular);
  absl::optional<ApnPropertiesPtr> last_connected_attach_apn =
      GetPreRevampApnFromDict(cellular_dict,
                              ::onc::cellular::kLastConnectedAttachApnProperty);
  NET_LOG(EVENT) << "last_connected_attach_apn: "
                 << (last_connected_attach_apn.has_value()
                         ? (*last_connected_attach_apn)->access_point_name
                         : "none");

  absl::optional<ApnPropertiesPtr> last_connected_default_apn =
      GetPreRevampApnFromDict(
          cellular_dict, ::onc::cellular::kLastConnectedDefaultApnProperty);
  NET_LOG(EVENT) << "last_connected_default_apn: "
                 << (last_connected_default_apn.has_value()
                         ? (*last_connected_default_apn)->access_point_name
                         : "none");

  const bool is_network_managed = network->IsManagedByPolicy();
  if (is_network_managed && !last_connected_default_apn) {
    ManagedApnPropertiesPtr selected_apn =
        chromeos::network_config::GetManagedApnProperties(
            cellular_dict, ::onc::cellular::kAPN);
    if (selected_apn && pre_revamp_custom_apn->access_point_name ==
                            selected_apn->access_point_name->active_value) {
      NET_LOG(EVENT) << "Managed network's selected APN matches the saved "
                     << "custom APN, migrating APN: " << guid;
      // Ensure the APN is enabled when it's migrated so that it's attempted
      // to be used by the new UI.
      pre_revamp_custom_apn->state = ApnState::kEnabled;
      pre_revamp_custom_apn->apn_types =
          GetMigratedApnTypes(pre_revamp_custom_apn);
      CellularNetworkMetricsLogger::LogManagedCustomApnMigrationType(
          CellularNetworkMetricsLogger::ManagedApnMigrationType::
              kMatchesSelectedApn);
      remote_cros_network_config_->CreateCustomApn(
          guid, std::move(pre_revamp_custom_apn));
    } else {
      NET_LOG(EVENT)
          << "Managed network's selected APN doesn't match the saved custom "
          << "APN, setting Shill with empty list for network: " << guid;
      base::Value::List empty_apn_list;
      CellularNetworkMetricsLogger::LogManagedCustomApnMigrationType(
          CellularNetworkMetricsLogger::ManagedApnMigrationType::
              kDoesNotMatchSelectedApn);
      SetShillCustomApnListForNetwork(*network, &empty_apn_list);
    }
  } else {
    NET_LOG(EVENT)
        << "Migrating network with non-managed flow, is network managed: "
        << is_network_managed;
    if (!last_connected_attach_apn && !last_connected_default_apn) {
      absl::optional<ApnPropertiesPtr> last_good_apn =
          GetPreRevampApnFromDict(cellular_dict, ::onc::cellular::kLastGoodAPN);

      if (last_good_apn && pre_revamp_custom_apn->access_point_name ==
                               (*last_good_apn)->access_point_name) {
        NET_LOG(EVENT) << "Network's last good APN matches the saved "
                       << "custom APN, migrating APN: " << guid
                       << "in the Enabled state";
        // Ensure the APN is enabled when it's migrated so that it's
        // attempted to be used by the new UI.
        pre_revamp_custom_apn->state = ApnState::kEnabled;
        CellularNetworkMetricsLogger::LogUnmanagedCustomApnMigrationType(
            CellularNetworkMetricsLogger::UnmanagedApnMigrationType::
                kMatchesLastGoodApn);
      } else {
        NET_LOG(EVENT) << "Network's last good APN does not match the saved "
                       << "custom APN, migrating APN: " << guid
                       << "in the Disabled state";
        // The custom APN was last unsuccessful in connecting when the flag was
        // off. Preserve the details of the custom APN but with a state of
        // Disabled.
        pre_revamp_custom_apn->state = ApnState::kDisabled;
        CellularNetworkMetricsLogger::LogUnmanagedCustomApnMigrationType(
            CellularNetworkMetricsLogger::UnmanagedApnMigrationType::
                kDoesNotMatchLastGoodApn);

        // Surfaces a notification that indicates that the Network's last good
        // APN does not match the saved custom APN, and that the APN will be
        // migrated in a disabled state to the new UI.
        ShowApnConfigurationDisabledNotification(
            pre_revamp_custom_apn->access_point_name, guid);
      }
      pre_revamp_custom_apn->apn_types =
          GetMigratedApnTypes(pre_revamp_custom_apn);
      remote_cros_network_config_->CreateCustomApn(
          guid, std::move(pre_revamp_custom_apn));
    } else if (last_connected_attach_apn && last_connected_default_apn &&
               pre_revamp_custom_apn->access_point_name ==
                   (*last_connected_attach_apn)->access_point_name &&
               pre_revamp_custom_apn->access_point_name ==
                   (*last_connected_default_apn)->access_point_name) {
      NET_LOG(EVENT)
          << "Network's last connected default APN and attach APN match the "
          << "saved custom APN, migrating APN: " << guid
          << " in the Enabled state with Apn types Attach and Default";

      pre_revamp_custom_apn->state = ApnState::kEnabled;
      pre_revamp_custom_apn->apn_types = {ApnType::kAttach, ApnType::kDefault};
      CellularNetworkMetricsLogger::LogUnmanagedCustomApnMigrationType(
          CellularNetworkMetricsLogger::UnmanagedApnMigrationType::
              kMatchesLastConnectedAttachAndDefault);
      remote_cros_network_config_->CreateCustomApn(
          guid, std::move(pre_revamp_custom_apn));
    } else if (last_connected_attach_apn && last_connected_default_apn &&
               pre_revamp_custom_apn->access_point_name ==
                   (*last_connected_attach_apn)->access_point_name &&
               pre_revamp_custom_apn->access_point_name !=
                   (*last_connected_default_apn)->access_point_name) {
      NET_LOG(EVENT) << "Network's last connected attach APN matches the saved "
                        "custom APN, but not the last connected default APN.";
      bool has_matching_default_apn = ContainsMatchingApn(
          cellular_dict, (*last_connected_default_apn)->access_point_name);

      if (has_matching_default_apn) {
        NET_LOG(EVENT) << "Network's last connected default APN matches an "
                       << "APN in the network list, migrating last connected "
                       << "default and attach APN: " << guid
                       << " in the Enabled state";

        (*last_connected_attach_apn)->state = ApnState::kEnabled;
        (*last_connected_attach_apn)->apn_types = {ApnType::kAttach};

        (*last_connected_default_apn)->state = ApnState::kEnabled;
        (*last_connected_default_apn)->apn_types = {ApnType::kDefault};

        CellularNetworkMetricsLogger::LogUnmanagedCustomApnMigrationType(
            CellularNetworkMetricsLogger::UnmanagedApnMigrationType::
                kMatchesLastConnectedAttachHasMatchingDatabaseApn);
        remote_cros_network_config_->CreateCustomApn(
            guid, last_connected_default_apn->Clone());
      } else {
        //  Fallback to the catch-all case where the attach APN with a disabled
        //  state is migrated so that Shill will know to use the revamped logic.
        NET_LOG(EVENT)
            << "Network's last connected default APN does not match an "
            << "APN in the network list, migrating last connected "
            << "attach APN: " << guid << " in the Disabled state";
        (*last_connected_attach_apn)->state = ApnState::kDisabled;
        (*last_connected_attach_apn)->apn_types = {ApnType::kAttach};

        CellularNetworkMetricsLogger::LogUnmanagedCustomApnMigrationType(
            CellularNetworkMetricsLogger::UnmanagedApnMigrationType::
                kMatchesLastConnectedAttachHasNoMatchingDatabaseApn);
      }

      remote_cros_network_config_->CreateCustomApn(
          guid, last_connected_attach_apn->Clone());
    } else if (!last_connected_attach_apn && last_connected_default_apn &&
               pre_revamp_custom_apn->access_point_name ==
                   (*last_connected_default_apn)->access_point_name) {
      NET_LOG(EVENT) << "Network has no last connected attach APN but has "
                     << "a last connected default APN that matches the "
                     << "saved custom APN, migrating APN: " << guid
                     << " in the Enabled state with Apn type Default";

      pre_revamp_custom_apn->state = ApnState::kEnabled;
      pre_revamp_custom_apn->apn_types = {ApnType::kDefault};

      CellularNetworkMetricsLogger::LogUnmanagedCustomApnMigrationType(
          CellularNetworkMetricsLogger::UnmanagedApnMigrationType::
              kMatchesLastConnectedDefaultNoLastConnectedAttach);
      remote_cros_network_config_->CreateCustomApn(
          guid, std::move(pre_revamp_custom_apn));
    } else {
      NET_LOG(EVENT) << "Network's last connected default APN and attach APN "
                     << "do not match the saved custom APN, migrating APN: "
                     << guid << " in the Disabled state.";
      pre_revamp_custom_apn->state = ApnState::kDisabled;
      pre_revamp_custom_apn->apn_types =
          GetMigratedApnTypes(pre_revamp_custom_apn);

      CellularNetworkMetricsLogger::LogUnmanagedCustomApnMigrationType(
          CellularNetworkMetricsLogger::UnmanagedApnMigrationType::
              kNoMatchingConnectedApn);
      remote_cros_network_config_->CreateCustomApn(
          guid, std::move(pre_revamp_custom_apn));
    }
  }

  NET_LOG(EVENT) << "ApnMigrator: Mark network with ICCID: " << iccid
                 << " as migrated";
  managed_cellular_pref_handler_->AddApnMigratedIccid(iccid);
  iccids_in_migration_.erase(iccid);
}

NetworkMetadataStore* ApnMigrator::GetNetworkMetadataStore() {
  if (network_metadata_store_for_testing_) {
    return network_metadata_store_for_testing_;
  }

  return NetworkHandler::Get()->network_metadata_store();
}

}  // namespace ash
