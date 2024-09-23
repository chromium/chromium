// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/mdns/nearby_connections_mdns_manager.h"

#include <string>

#include "components/cross_device/logging/logging.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace {

::sharing::mojom::NsdServiceInfoPtr ConvertNsdServiceInfo(
    const local_discovery::ServiceDescription& service_description) {
  ::sharing::mojom::NsdServiceInfoPtr output =
      ::sharing::mojom::NsdServiceInfo::New();

  // IP address is expected as bytes, rather than ToString().
  auto ip_vec = service_description.ip_address.CopyBytesToVector();
  output->ip_address = std::string(ip_vec.begin(), ip_vec.end());
  output->port = service_description.address.port();
  output->service_name = service_description.instance_name();
  output->service_type = service_description.service_type();

  CD_LOG(INFO, Feature::NS)
      << __func__ << " Found service: " << output->service_name
      << " for service_type: " << output->service_type
      << " with ip_address & port: " << output->ip_address.value() << ":"
      << output->port.value();

  // Txt records contain important information like endpoint name.
  for (const std::string& entry : service_description.metadata) {
    const std::string_view key_value(entry);
    const size_t equal_pos = key_value.find("=");
    // We expect to find key/value pairs separated by an equals sign.
    if (equal_pos == std::string_view::npos) {
      continue;
    }

    std::string_view key = key_value.substr(0, equal_pos);
    std::string_view value = key_value.substr(equal_pos + 1);
    if (!output->txt_records) {
      // Lazily construct the optional map the first time.
      output->txt_records.emplace();
    }
    output->txt_records->insert_or_assign(key, value);
    CD_LOG(INFO, Feature::NS)
        << __func__ << " TXT record: {" << key << ": " << value << "}";
  }

  return output;
}

}  // namespace

namespace nearby::sharing {

NearbyConnectionsMdnsManager::NearbyConnectionsMdnsManager() {
  CD_LOG(VERBOSE, Feature::NS) << __func__;
}

NearbyConnectionsMdnsManager::~NearbyConnectionsMdnsManager() {
  CD_LOG(VERBOSE, Feature::NS) << __func__;
}

void NearbyConnectionsMdnsManager::StartDiscoverySession(
    const std::string& service_type,
    StartDiscoverySessionCallback callback) {
  CD_LOG(INFO, Feature::NS)
      << __func__
      << " Starting mDNS discovery session for service_type:" << service_type;

  if (base::Contains(device_listers_, service_type)) {
    CD_LOG(INFO, Feature::NS)
        << __func__ << " Already discovering for this service type.";
    std::move(callback).Run(true);
    return;
  }

  // ServiceDiscoverySharedClient::GetInstance is thread protected, and
  // calls from unit tests will come from the MainThread, causing a CHECK crash.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  discovery_client_ =
      local_discovery::ServiceDiscoverySharedClient::GetInstance();
  auto lister = local_discovery::ServiceDiscoveryDeviceLister::Create(
      this, discovery_client_.get(), service_type);
  lister->Start();
  lister->DiscoverNewDevices();
  device_listers_[service_type] = std::move(lister);
  std::move(callback).Run(true);
}

void NearbyConnectionsMdnsManager::StopDiscoverySession(
    const std::string& service_type,
    StopDiscoverySessionCallback callback) {
  int num_erased = device_listers_.erase(service_type);
  if (num_erased > 0) {
    CD_LOG(INFO, Feature::NS)
        << __func__ << " Stopping mDNS discovery session for service_type: "
        << service_type;
    std::move(callback).Run(true);
    return;
  }

  CD_LOG(INFO, Feature::NS)
      << __func__ << " Failed to find service type to stop: " << service_type;
  std::move(callback).Run(false);
}

void NearbyConnectionsMdnsManager::AddObserver(
    ::mojo::PendingRemote<::sharing::mojom::MdnsObserver> observer) {
  CD_LOG(VERBOSE, Feature::NS) << __func__ << " Adding mDNS observer.";
  observers_.Add(std::move(observer));
}

void NearbyConnectionsMdnsManager::OnDeviceChanged(
    const std::string& service_type,
    bool added,
    const local_discovery::ServiceDescription& service_description) {
  CD_LOG(VERBOSE, Feature::NS) << __func__ << " Device changed.";
  // We expect the caller to handle new services, duplicate services,
  // and services with updated information.
  for (auto& observer : observers_) {
    observer->ServiceFound(ConvertNsdServiceInfo(service_description));
  }
}

void NearbyConnectionsMdnsManager::OnDeviceRemoved(
    const std::string& service_type,
    const std::string& service_name) {
  CD_LOG(VERBOSE, Feature::NS) << __func__ << " Device removed.";
  // We expect the caller to handle checking if the removed service
  // was previously found or double-removed, etc.
  ::sharing::mojom::NsdServiceInfoPtr service_info =
      ::sharing::mojom::NsdServiceInfo::New();
  // The returned service name is always of the form
  // <service_name>.<service_type>, and we want just the first half.
  service_info->service_name =
      service_name.substr(0, service_name.find_first_of('.'));
  service_info->service_type = service_type;
  for (auto& observer : observers_) {
    observer->ServiceLost(std::move(service_info));
  }
}

void NearbyConnectionsMdnsManager::OnDeviceCacheFlushed(
    const std::string& service_type) {
  CD_LOG(VERBOSE, Feature::NS) << __func__ << " Device cache flushed.";

  auto existing_lister = device_listers_.find(service_type);
  CHECK(existing_lister != device_listers_.end());

  // Request a new round of discovery from the lister.
  existing_lister->second->DiscoverNewDevices();
  return;
}

void NearbyConnectionsMdnsManager::SetDeviceListersForTesting(
    std::map<std::string,
             std::unique_ptr<local_discovery::ServiceDiscoveryDeviceLister>>*
        device_listers) {
  device_listers_.swap(*device_listers);
  // Normally listers are started in StartDiscovery, but unit tests
  // use fake service listers & run on the MainThread while StartDiscovery
  // must be called on the UIThread.
  for (auto& device_lister : device_listers_) {
    device_lister.second->Start();
    device_lister.second->DiscoverNewDevices();
  }
}

}  // namespace nearby::sharing
