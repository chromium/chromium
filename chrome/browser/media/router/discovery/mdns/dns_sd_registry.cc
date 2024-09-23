// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/dns_sd_registry.h"

#include <limits>
#include <utility>

#include "base/check.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"  // nogncheck
#include "chrome/browser/media/router/discovery/mdns/dns_sd_device_lister.h"
#include "chrome/common/buildflags.h"

namespace media_router {

DnsSdRegistry::ServiceTypeData::ServiceTypeData(
    std::unique_ptr<DnsSdDeviceLister> lister)
    : ref_count(1), lister_(std::move(lister)) {}

DnsSdRegistry::ServiceTypeData::~ServiceTypeData() {}

void DnsSdRegistry::ServiceTypeData::ListenerAdded() {
  ref_count++;
  CHECK(ref_count != std::numeric_limits<decltype(ref_count)>::max());
}

bool DnsSdRegistry::ServiceTypeData::ListenerRemoved() {
  return --ref_count == 0;
}

int DnsSdRegistry::ServiceTypeData::GetListenerCount() {
  return ref_count;
}

bool DnsSdRegistry::ServiceTypeData::UpdateService(
    bool added,
    const DnsSdService& service) {
  auto it = base::ranges::find(service_list_, service.service_name,
                               &DnsSdService::service_name);
  // Set to true when a service is updated in or added to the registry.
  bool updated_or_added = added;
  bool known = (it != service_list_.end());
  if (known) {
    // If added == true, but we still found the service in our cache, then just
    // update the existing entry, but this should not happen!
    DCHECK(!added);
    if (*it != service) {
      *it = service;
      updated_or_added = true;
    }
  } else if (added) {
    service_list_.push_back(service);
  }
  return updated_or_added;
}

bool DnsSdRegistry::ServiceTypeData::RemoveService(
    const std::string& service_name) {
  for (auto it = service_list_.begin(); it != service_list_.end(); ++it) {
    if ((*it).service_name == service_name) {
      service_list_.erase(it);
      return true;
    }
  }
  return false;
}

void DnsSdRegistry::ServiceTypeData::ResetAndDiscover() {
  lister_->Reset();
  ClearServices();
}

bool DnsSdRegistry::ServiceTypeData::ClearServices() {
  lister_->Discover();

  if (service_list_.empty())
    return false;

  service_list_.clear();
  return true;
}

const DnsSdRegistry::DnsSdServiceList&
DnsSdRegistry::ServiceTypeData::GetServiceList() {
  return service_list_;
}

DnsSdRegistry::DnsSdRegistry() {
#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
  service_discovery_client_ =
      local_discovery::ServiceDiscoverySharedClient::GetInstance();
#endif
}

DnsSdRegistry::DnsSdRegistry(
    local_discovery::ServiceDiscoverySharedClient* client) {
  service_discovery_client_ = client;
}

DnsSdRegistry::~DnsSdRegistry() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

// static
DnsSdRegistry* DnsSdRegistry::GetInstance() {
  return base::Singleton<DnsSdRegistry,
                         base::LeakySingletonTraits<DnsSdRegistry>>::get();
}

void DnsSdRegistry::AddObserver(DnsSdObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void DnsSdRegistry::RemoveObserver(DnsSdObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

DnsSdDeviceLister* DnsSdRegistry::CreateDnsSdDeviceLister(
    DnsSdDelegate* delegate,
    const std::string& service_type,
    local_discovery::ServiceDiscoverySharedClient* discovery_client) {
  return new DnsSdDeviceLister(discovery_client, delegate, service_type);
}

void DnsSdRegistry::Publish(const std::string& service_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DispatchApiEvent(service_type);
}

void DnsSdRegistry::ResetAndDiscover() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (const auto& next_service : service_data_map_) {
    next_service.second->ResetAndDiscover();
  }
}

void DnsSdRegistry::RegisterDnsSdListener(const std::string& service_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (service_type.empty())
    return;

  if (IsRegistered(service_type)) {
    service_data_map_[service_type]->ListenerAdded();
    DispatchApiEvent(service_type);
    return;
  }

  std::unique_ptr<DnsSdDeviceLister> dns_sd_device_lister(
      CreateDnsSdDeviceLister(this, service_type,
                              service_discovery_client_.get()));
  dns_sd_device_lister->Discover();
  service_data_map_[service_type] =
      std::make_unique<ServiceTypeData>(std::move(dns_sd_device_lister));
  DispatchApiEvent(service_type);
}

void DnsSdRegistry::UnregisterDnsSdListener(const std::string& service_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto it = service_data_map_.find(service_type);
  if (it == service_data_map_.end())
    return;

  if (service_data_map_[service_type]->ListenerRemoved())
    service_data_map_.erase(it);
}

void DnsSdRegistry::ResetForTest() {
  service_data_map_.clear();
  service_discovery_client_.reset();
}

void DnsSdRegistry::ServiceChanged(const std::string& service_type,
                                   bool added,
                                   const DnsSdService& service) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!IsRegistered(service_type))
    return;

  // TODO(imcheng): This should be validated upstream in
  // dns_sd_device_lister.cc, i.e., |service.ip_address| should be a
  // valid net::IPAddress.
  net::IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(service.ip_address)) {
    return;
  }
  bool is_updated =
      service_data_map_[service_type]->UpdateService(added, service);
  if (is_updated)
    DispatchApiEvent(service_type);
}

void DnsSdRegistry::ServiceRemoved(const std::string& service_type,
                                   const std::string& service_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!IsRegistered(service_type))
    return;

  bool is_removed =
      service_data_map_[service_type]->RemoveService(service_name);
  if (is_removed)
    DispatchApiEvent(service_type);
}

void DnsSdRegistry::ServicesFlushed(const std::string& service_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!IsRegistered(service_type))
    return;

  bool is_cleared = service_data_map_[service_type]->ClearServices();
  if (is_cleared)
    DispatchApiEvent(service_type);
}

void DnsSdRegistry::ServicesPermissionRejected() {
  for (auto& observer : observers_) {
    observer.OnDnsSdPermissionRejected();
  }
}

void DnsSdRegistry::DispatchApiEvent(const std::string& service_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& observer : observers_) {
    observer.OnDnsSdEvent(service_type,
                          service_data_map_[service_type]->GetServiceList());
  }
}

bool DnsSdRegistry::IsRegistered(const std::string& service_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return service_data_map_.find(service_type) != service_data_map_.end();
}

}  // namespace media_router
