// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/recent_apps_interaction_handler_impl.h"

#include <memory>

#include "ash/components/multidevice/logging/logging.h"
#include "ash/components/phonehub/icon_decoder.h"
#include "ash/components/phonehub/notification.h"
#include "ash/components/phonehub/pref_names.h"
#include "ash/components/phonehub/proto/phonehub_api.pb.h"
#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {
namespace phonehub {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;
using multidevice_setup::mojom::HostStatus;
using HostStatusWithDevice =
    multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice;
using FeatureStatesMap =
    multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap;

const size_t kMaxMostRecentApps = 5;
const size_t kMaxSavedRecentApps = 10;

// static
void RecentAppsInteractionHandlerImpl::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kRecentAppsHistory);
}

RecentAppsInteractionHandlerImpl::RecentAppsInteractionHandlerImpl(
    PrefService* pref_service,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    MultideviceFeatureAccessManager* multidevice_feature_access_manager,
    std::unique_ptr<IconDecoder> icon_decoder)
    : pref_service_(pref_service),
      multidevice_setup_client_(multidevice_setup_client),
      multidevice_feature_access_manager_(multidevice_feature_access_manager),
      icon_decoder_(std::move(icon_decoder)) {
  multidevice_setup_client_->AddObserver(this);
  multidevice_feature_access_manager_->AddObserver(this);
}

RecentAppsInteractionHandlerImpl::~RecentAppsInteractionHandlerImpl() {
  multidevice_setup_client_->RemoveObserver(this);
  multidevice_feature_access_manager_->RemoveObserver(this);
}

void RecentAppsInteractionHandlerImpl::AddRecentAppClickObserver(
    RecentAppClickObserver* observer) {
  observer_list_.AddObserver(observer);
}

void RecentAppsInteractionHandlerImpl::RemoveRecentAppClickObserver(
    RecentAppClickObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void RecentAppsInteractionHandlerImpl::NotifyRecentAppClicked(
    const Notification::AppMetadata& app_metadata) {
  for (auto& observer : observer_list_)
    observer.OnRecentAppClicked(app_metadata);
}

// Load the |recent_app_metadata_list_| from |pref_service_| if there is a
// history of |recent_app_metadata_list_| exist in |pref_service_|. Then add or
// update |app_metadata| into |recent_app_metadata_list_| and sort
// |recent_app_metadata_list_| based on |last_accessed_timestamp|. Also update
// this |app_metadata| back to |pref_service_|.
void RecentAppsInteractionHandlerImpl::NotifyRecentAppAddedOrUpdated(
    const Notification::AppMetadata& app_metadata,
    base::Time last_accessed_timestamp) {
  LoadRecentAppMetadataListFromPrefIfNeed();

  // Each element of |recent_app_metadata_list_| has a unique |package_name| and
  // |user_id|.
  for (auto it = recent_app_metadata_list_.begin();
       it != recent_app_metadata_list_.end(); ++it) {
    if (it->first.package_name == app_metadata.package_name &&
        it->first.user_id == app_metadata.user_id) {
      recent_app_metadata_list_.erase(it);
      break;
    }
  }

  recent_app_metadata_list_.emplace_back(app_metadata, last_accessed_timestamp);

  // Sort |recent_app_metadata_list_| from most recently visited to least
  // recently visited.
  std::sort(recent_app_metadata_list_.begin(), recent_app_metadata_list_.end(),
            [](const std::pair<Notification::AppMetadata, base::Time>& a,
               const std::pair<Notification::AppMetadata, base::Time>& b) {
              // More recently visited apps should come before earlier visited
              // apps.
              return a.second > b.second;
            });

  SaveRecentAppMetadataListToPref();
  ComputeAndUpdateUiState();
}

base::flat_set<int64_t>
RecentAppsInteractionHandlerImpl::GetUserIdsWithDisplayRecentApps() {
  base::flat_set<int64_t> user_ids;
  for (auto& user : user_states()) {
    if (user.is_enabled) {
      user_ids.emplace(user.user_id);
    }
  }
  // Skip filtering recent apps when not receiving user states.
  if (user_ids.empty()) {
    for (auto const& it : recent_app_metadata_list_) {
      if (!user_ids.contains(it.first.user_id)) {
        user_ids.emplace(it.first.user_id);
      }
    }
  }
  return user_ids;
}

std::vector<Notification::AppMetadata>
RecentAppsInteractionHandlerImpl::FetchRecentAppMetadataList() {
  LoadRecentAppMetadataListFromPrefIfNeed();

  base::flat_set<int64_t> active_user_ids = GetUserIdsWithDisplayRecentApps();
  std::vector<Notification::AppMetadata> app_metadata_list;

  for (auto const& it : recent_app_metadata_list_) {
    if (active_user_ids.contains(it.first.user_id)) {
      app_metadata_list.push_back(it.first);
      // At most |kMaxMostRecentApps| recent apps can be displayed.
      if (app_metadata_list.size() == kMaxMostRecentApps)
        break;
    }
  }
  return app_metadata_list;
}

void RecentAppsInteractionHandlerImpl::
    LoadRecentAppMetadataListFromPrefIfNeed() {
  if (!has_loaded_prefs_) {
    PA_LOG(INFO) << "LoadRecentAppMetadataListFromPref";
    const base::Value* recent_apps_history_pref =
        pref_service_->GetList(prefs::kRecentAppsHistory);
    for (const auto& value : recent_apps_history_pref->GetListDeprecated()) {
      DCHECK(value.is_dict());
      recent_app_metadata_list_.emplace_back(
          Notification::AppMetadata::FromValue(value),
          base::Time::FromDoubleT(0));
    }
    has_loaded_prefs_ = true;
  }
}

void RecentAppsInteractionHandlerImpl::SaveRecentAppMetadataListToPref() {
  PA_LOG(INFO) << "SaveRecentAppMetadataListToPref";
  size_t num_recent_apps_to_save =
      std::min(recent_app_metadata_list_.size(), kMaxSavedRecentApps);
  std::vector<base::Value> app_metadata_value_list;
  for (size_t i = 0; i < num_recent_apps_to_save; ++i) {
    app_metadata_value_list.push_back(
        recent_app_metadata_list_[i].first.ToValue());
  }
  pref_service_->Set(prefs::kRecentAppsHistory,
                     base::Value(std::move(app_metadata_value_list)));
}

void RecentAppsInteractionHandlerImpl::OnFeatureStatesChanged(
    const FeatureStatesMap& feature_states_map) {
  ComputeAndUpdateUiState();
}

void RecentAppsInteractionHandlerImpl::OnHostStatusChanged(
    const HostStatusWithDevice& host_device_with_status) {
  if (host_device_with_status.first != HostStatus::kHostVerified) {
    PA_LOG(INFO) << "ClearRecentAppMetadataListAndPref";
    ClearRecentAppMetadataListAndPref();
  }
}

void RecentAppsInteractionHandlerImpl::OnNotificationAccessChanged() {
  ComputeAndUpdateUiState();
}

void RecentAppsInteractionHandlerImpl::OnAppsAccessChanged() {
  ComputeAndUpdateUiState();
}

void RecentAppsInteractionHandlerImpl::SetStreamableApps(
    const proto::StreamableApps& streamable_apps) {
  PA_LOG(INFO) << "ClearRecentAppMetadataListAndPref to update the list of "
               << streamable_apps.apps_size() << " items.";
  ClearRecentAppMetadataListAndPref();
  std::unique_ptr<std::vector<IconDecoder::DecodingData>> decoding_data_list =
      std::make_unique<std::vector<IconDecoder::DecodingData>>();
  std::hash<std::string> str_hash;
  gfx::Image image =
      gfx::Image(CreateVectorIcon(kPhoneHubPhoneIcon, gfx::kGoogleGrey700));
  for (const auto& app : streamable_apps.apps()) {
    // TODO(nayebi): AppMetadata is no longer limited to Notification class,
    // let's move it outside of the Notification class.s2
    recent_app_metadata_list_.emplace_back(
        Notification::AppMetadata(base::UTF8ToUTF16(app.visible_name()),
                                  app.package_name(), image, absl::nullopt,
                                  app.icon_styling() ==
                                      proto::NotificationIconStyling::
                                          ICON_STYLE_MONOCHROME_SMALL_ICON,
                                  app.user_id()),
        base::Time::FromDoubleT(0));
    decoding_data_list->emplace_back(
        IconDecoder::DecodingData(str_hash(app.package_name()), app.icon()));
  }

  icon_decoder_->BatchDecode(
      std::move(decoding_data_list),
      base::BindOnce(&RecentAppsInteractionHandlerImpl::IconsDecoded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RecentAppsInteractionHandlerImpl::IconsDecoded(
    std::unique_ptr<std::vector<IconDecoder::DecodingData>>
        decoding_data_list) {
  std::hash<std::string> str_hash;
  for (const IconDecoder::DecodingData& decoding_data : *decoding_data_list) {
    if (decoding_data.result.IsEmpty())
      continue;
    // find the associated app metadata
    for (auto& app_metadata : recent_app_metadata_list_) {
      if (decoding_data.id == str_hash(app_metadata.first.package_name)) {
        app_metadata.first.icon = decoding_data.result;
        continue;
      }
    }
  }
  SaveRecentAppMetadataListToPref();
  ComputeAndUpdateUiState();
}

void RecentAppsInteractionHandlerImpl::ComputeAndUpdateUiState() {
  ui_state_ = RecentAppsUiState::HIDDEN;

  LoadRecentAppMetadataListFromPrefIfNeed();

  // There are three cases we need to handle:
  // 1. If no recent app in list and necessary permission be granted, the
  // placeholder view will be shown.
  // 2. If some recent apps in list and streaming is allowed, the recent apps
  // view will be shown.
  // 3. Otherwise, no recent apps view will be shown.
  bool allow_streaming = multidevice_setup_client_->GetFeatureState(
                             Feature::kEche) == FeatureState::kEnabledByUser;

  bool is_apps_access_required =
      features::IsEcheSWAEnabled() &&
      multidevice_feature_access_manager_->GetAppsAccessStatus() ==
          phonehub::MultideviceFeatureAccessManager::AccessStatus::
              kAvailableButNotGranted;

  if (!allow_streaming || is_apps_access_required) {
    NotifyRecentAppsViewUiStateUpdated();
    return;
  }
  if (recent_app_metadata_list_.empty()) {
    bool notifications_enabled =
        multidevice_setup_client_->GetFeatureState(
            Feature::kPhoneHubNotifications) == FeatureState::kEnabledByUser;
    bool grant_notification_access_on_host =
        multidevice_feature_access_manager_->GetNotificationAccessStatus() ==
        phonehub::MultideviceFeatureAccessManager::AccessStatus::kAccessGranted;
    if (notifications_enabled && grant_notification_access_on_host)
      ui_state_ = RecentAppsUiState::PLACEHOLDER_VIEW;
  } else {
    ui_state_ = RecentAppsUiState::ITEMS_VISIBLE;
  }
  NotifyRecentAppsViewUiStateUpdated();
}

void RecentAppsInteractionHandlerImpl::ClearRecentAppMetadataListAndPref() {
  recent_app_metadata_list_.clear();
  pref_service_->ClearPref(prefs::kRecentAppsHistory);
}

}  // namespace phonehub
}  // namespace ash
