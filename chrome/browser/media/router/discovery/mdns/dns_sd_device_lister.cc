// Copyright 2013 The Chromium Authors. All rights reserved.
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

  VLOG(1) << "Found " << service->service_name << ", "
          << service->service_host_port.ToString() << ", "
          << service->ip_address;
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
    VLOG(1) << "Started device lister for service type "
            << device_lister_->service_type();
  }
  device_lister_->DiscoverNewDevices();
  VLOG(1) << "Discovery new devices for service type "
          << device_lister_->service_type();
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
  VLOG(1) << "OnDeviceChanged: "
          << "service_name: " << service.service_name << ", "
          << "added: " << added << ", "
          << "service_type: " << device_lister_->service_type();
  delegate_->ServiceChanged(device_lister_->service_type(), added, service);
}

void DnsSdDeviceLister::OnDeviceRemoved(const std::string& service_type,
                                        const std::string& service_name) {
  VLOG(1) << "OnDeviceRemoved: "
          << "service_name: " << service_name << ", "
          << "service_type: " << service_type;
  delegate_->ServiceRemoved(service_type, service_name);
}

void DnsSdDeviceLister::OnDeviceCacheFlushed(const std::string& service_type) {
  VLOG(1) << "OnDeviceCacheFlushed: "
          << "service_type: " << device_lister_->service_type();
  delegate_->ServicesFlushed(device_lister_->service_type());
  device_lister_->DiscoverNewDevices();
}

}  // namespace media_router
