// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/dns_sd_device_lister.h"

#include "chrome/browser/media/router/discovery/mdns/dns_sd_delegate.h"

using local_discovery::ServiceDescription;

namespace media_router {

namespace {

void FillServiceInfo(const ServiceDescription& service_description,
                     DnsSdService* service) {
  service->service_name = service_description.service_name;
  service->service_host_port = service_description.address;
  if (service_description.ip_address.IsValid()) {
    service->ip_address = service_description.ip_address.ToString();
  }
  service->service_data = service_description.metadata;
}

}  // namespace

DnsSdDeviceLister::DnsSdDeviceLister(
    local_discovery::ServiceDiscoveryClient* service_discovery_client,
    DnsSdDelegate* delegate,
    const std::string& service_type)
    : delegate_(delegate)
#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
      ,
      service_discovery_client_(service_discovery_client),
      service_type_(service_type)
#endif
{
}

DnsSdDeviceLister::~DnsSdDeviceLister() {}

void DnsSdDeviceLister::Discover() {
#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
  if (!device_lister_) {
    device_lister_ = local_discovery::ServiceDiscoveryDeviceLister::Create(
        this, service_discovery_client_, service_type_);
    device_lister_->Start();
  }
  device_lister_->DiscoverNewDevices();
#endif
}

void DnsSdDeviceLister::Reset() {
  device_lister_.reset();
}

void DnsSdDeviceLister::OnDeviceChanged(
    const std::string& service_type,
    bool added,
    const ServiceDescription& service_description) {
  DnsSdService service;
  FillServiceInfo(service_description, &service);
  delegate_->ServiceChanged(device_lister_->service_type(), added, service);
}

void DnsSdDeviceLister::OnDeviceRemoved(const std::string& service_type,
                                        const std::string& service_name) {
  delegate_->ServiceRemoved(service_type, service_name);
}

void DnsSdDeviceLister::OnDeviceCacheFlushed(const std::string& service_type) {
  delegate_->ServicesFlushed(device_lister_->service_type());
  device_lister_->DiscoverNewDevices();
}

void DnsSdDeviceLister::OnPermissionRejected() {
  delegate_->ServicesPermissionRejected();
}

}  // namespace media_router
