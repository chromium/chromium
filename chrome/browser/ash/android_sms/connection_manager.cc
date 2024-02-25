// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/android_sms/connection_manager.h"

#include <utility>

#include "chrome/browser/ash/android_sms/android_sms_urls.h"
#include "chrome/browser/ash/android_sms/connection_establisher.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"

namespace ash {
namespace android_sms {

ConnectionManager::ServiceWorkerProvider::ServiceWorkerProvider() = default;

ConnectionManager::ServiceWorkerProvider::~ServiceWorkerProvider() = default;

content::ServiceWorkerContext* ConnectionManager::ServiceWorkerProvider::Get(
    const GURL& url,
    Profile* profile) {
  return profile->GetDefaultStoragePartition()->GetServiceWorkerContext();
}

ConnectionManager::ConnectionManager(
    std::unique_ptr<ConnectionEstablisher> connection_establisher,
    Profile* profile,
    AndroidSmsAppManager* android_sms_app_manager,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
    : ConnectionManager(std::move(connection_establisher),
                        profile,
                        android_sms_app_manager,
                        multidevice_setup_client,
                        std::make_unique<ServiceWorkerProvider>()) {}

ConnectionManager::ConnectionManager(
    std::unique_ptr<ConnectionEstablisher> connection_establisher,
    Profile* profile,
    AndroidSmsAppManager* android_sms_app_manager,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    std::unique_ptr<ServiceWorkerProvider> service_worker_provider)
    : connection_establisher_(std::move(connection_establisher)),
      profile_(profile),
      android_sms_app_manager_(android_sms_app_manager),
      multidevice_setup_client_(multidevice_setup_client),
      service_worker_provider_(std::move(service_worker_provider)) {
  android_sms_app_manager_->AddObserver(this);
  multidevice_setup_client_->AddObserver(this);

  UpdateConnectionStatus();
}

ConnectionManager::~ConnectionManager() {
  android_sms_app_manager_->RemoveObserver(this);
  multidevice_setup_client_->RemoveObserver(this);

  if (GetCurrentServiceWorkerContext())
    GetCurrentServiceWorkerContext()->RemoveObserver(this);
}

void ConnectionManager::StartConnection() {
  if (!enabled_pwa_url_) {
    return;
  }
  PA_LOG(INFO) << "ConnectionManager::StartConnection(): Establishing "
               << "connection to PWA at " << *enabled_pwa_url_ << ".";
  connection_establisher_->EstablishConnection(
      *enabled_pwa_url_,
      ConnectionEstablisher::ConnectionMode::kStartConnection,
      GetCurrentServiceWorkerContext());
}

void ConnectionManager::OnVersionActivated(int64_t version_id,
                                           const GURL& scope) {
  if (!enabled_pwa_url_ || !scope.EqualsIgnoringRef(*enabled_pwa_url_))
    return;

  prev_active_version_id_ = active_version_id_;
  active_version_id_ = version_id;

  connection_establisher_->EstablishConnection(
      *enabled_pwa_url_,
      ConnectionEstablisher::ConnectionMode::kResumeExistingConnection,
      GetCurrentServiceWorkerContext());
}

void ConnectionManager::OnVersionRedundant(int64_t version_id,
                                           const GURL& scope) {
  if (!enabled_pwa_url_ || !scope.EqualsIgnoringRef(*enabled_pwa_url_))
    return;

  if (active_version_id_ != version_id)
    return;

  // If the active version is marked redundant then it cannot handle messages
  // anymore, so stop tracking it.
  prev_active_version_id_ = active_version_id_;
  active_version_id_.reset();
}

void ConnectionManager::OnNoControllees(int64_t version_id, const GURL& scope) {
  if (!enabled_pwa_url_ || !scope.EqualsIgnoringRef(*enabled_pwa_url_))
    return;

  // Set active_version_id_ in case we missed version activated.
  // This is unlikely but protects against a case where a Android Messages for
  // Web page may have opened before the ConnectionManager is created.
  if (!active_version_id_ && prev_active_version_id_ != version_id)
    active_version_id_ = version_id;

  if (active_version_id_ != version_id)
    return;

  connection_establisher_->EstablishConnection(
      *enabled_pwa_url_,
      ConnectionEstablisher::ConnectionMode::kResumeExistingConnection,
      GetCurrentServiceWorkerContext());
}

void ConnectionManager::OnInstalledAppUrlChanged() {
  UpdateConnectionStatus();
}

void ConnectionManager::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  UpdateConnectionStatus();
}

void ConnectionManager::UpdateConnectionStatus() {
  std::optional<GURL> updated_pwa_url =
      ConnectionManager::GenerateEnabledPwaUrl();
  if (enabled_pwa_url_ == updated_pwa_url)
    return;

  // If the URL was previously enabled, stop the ServiceWorker. This occurs when
  // the feature was disabled and when migrating from the old URL to the new
  // one.
  if (enabled_pwa_url_) {
    PA_LOG(INFO) << "ConnectionManager::UpdateConnectionStatus(): Stopping "
                 << "connection to PWA at " << *enabled_pwa_url_ << ".";
    connection_establisher_->TearDownConnection(
        *enabled_pwa_url_, GetCurrentServiceWorkerContext());
    GetCurrentServiceWorkerContext()->RemoveObserver(this);
    active_version_id_.reset();
    prev_active_version_id_.reset();
  }

  enabled_pwa_url_ = updated_pwa_url;
  if (!enabled_pwa_url_)
    return;

  GetCurrentServiceWorkerContext()->AddObserver(this);
  StartConnection();
}

std::optional<GURL> ConnectionManager::GenerateEnabledPwaUrl() {
  const auto it = multidevice_setup_client_->GetFeatureStates().find(
      multidevice_setup::mojom::Feature::kMessages);

  // If the feature is not enabled, there is no enabled URL.
  if (it->second != multidevice_setup::mojom::FeatureState::kEnabledByUser)
    return std::nullopt;

  // Return the installed app URL if the PWA is installed.
  std::optional<GURL> installed_url =
      android_sms_app_manager_->GetCurrentAppUrl();
  if (installed_url)
    return installed_url;

  // Otherwise, return the default URL.
  return GetAndroidMessagesURL();
}

content::ServiceWorkerContext*
ConnectionManager::GetCurrentServiceWorkerContext() {
  if (!enabled_pwa_url_)
    return nullptr;

  return service_worker_provider_->Get(*enabled_pwa_url_, profile_);
}

void ConnectionManager::SetServiceWorkerProviderForTesting(
    std::unique_ptr<ServiceWorkerProvider> service_worker_provider) {
  service_worker_provider_ = std::move(service_worker_provider);
}

}  // namespace android_sms
}  // namespace ash
