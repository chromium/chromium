// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/android_sms/connection_manager.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "components/session_manager/core/session_manager.h"

namespace chromeos {

namespace android_sms {

ConnectionManager::ConnectionManager(
    content::ServiceWorkerContext* service_worker_context,
    std::unique_ptr<ConnectionEstablisher> connection_establisher,
    MultiDeviceSetupClient* multidevice_setup_client)
    : service_worker_context_(service_worker_context),
      connection_establisher_(std::move(connection_establisher)),
      multidevice_setup_client_(multidevice_setup_client) {
  service_worker_context_->AddObserver(this);
  multidevice_setup_client_->AddObserver(this);
  UpdateAndroidSmsFeatureState(multidevice_setup_client->GetFeatureState(
      multidevice_setup::mojom::Feature::kMessages));
}

ConnectionManager::~ConnectionManager() {
  service_worker_context_->RemoveObserver(this);
  multidevice_setup_client_->RemoveObserver(this);
}

void ConnectionManager::OnVersionActivated(int64_t version_id,
                                           const GURL& scope) {
  if (!scope.EqualsIgnoringRef(GetAndroidMessagesURL()))
    return;

  prev_active_version_id_ = active_version_id_;
  active_version_id_ = version_id;
  if (is_android_sms_enabled_)
    connection_establisher_->EstablishConnection(
        service_worker_context_,
        ConnectionEstablisher::ConnectionMode::kResumeExistingConnection);
}

void ConnectionManager::OnVersionRedundant(int64_t version_id,
                                           const GURL& scope) {
  if (!scope.EqualsIgnoringRef(GetAndroidMessagesURL()))
    return;

  if (active_version_id_ != version_id)
    return;

  // If the active version is marked redundant then it cannot handle messages
  // anymore, so stop tracking it.
  prev_active_version_id_ = active_version_id_;
  active_version_id_.reset();
}

void ConnectionManager::OnNoControllees(int64_t version_id, const GURL& scope) {
  if (!scope.EqualsIgnoringRef(GetAndroidMessagesURL()))
    return;

  // Set active_version_id_ in case we missed version activated.
  // This is unlikely but protects against a case where a Android Messages for
  // Web page may have opened before the ConnectionManager is created.
  if (!active_version_id_ && prev_active_version_id_ != version_id)
    active_version_id_ = version_id;

  if (active_version_id_ != version_id)
    return;

  if (is_android_sms_enabled_)
    connection_establisher_->EstablishConnection(
        service_worker_context_,
        ConnectionEstablisher::ConnectionMode::kResumeExistingConnection);
}

void ConnectionManager::OnFeatureStatesChanged(
    const MultiDeviceSetupClient::FeatureStatesMap& feature_states_map) {
  const auto it =
      feature_states_map.find(multidevice_setup::mojom::Feature::kMessages);
  if (it == feature_states_map.end())
    return;

  UpdateAndroidSmsFeatureState(it->second);
}

void ConnectionManager::UpdateAndroidSmsFeatureState(
    multidevice_setup::mojom::FeatureState feature_state) {
  bool is_enabled =
      feature_state == multidevice_setup::mojom::FeatureState::kEnabledByUser;
  if (is_android_sms_enabled_ == is_enabled)
    return;

  PA_LOG(INFO) << "ConnectionManager::UpdateAndroidSmsFeatureState enabled: "
               << is_enabled;
  if (is_enabled) {
    connection_establisher_->EstablishConnection(
        service_worker_context_,
        ConnectionEstablisher::ConnectionMode::kStartConnection);
  } else {
    service_worker_context_->StopAllServiceWorkersForOrigin(
        GetAndroidMessagesURL());
  }
  is_android_sms_enabled_ = is_enabled;
}

}  // namespace android_sms

}  // namespace chromeos
