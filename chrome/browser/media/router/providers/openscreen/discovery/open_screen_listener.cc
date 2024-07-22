// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/openscreen/discovery/open_screen_listener.h"

#include <utility>
#include <vector>

#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"

using openscreen::osp::ServiceInfo;

namespace media_router {
namespace {

const char kOpenScreenServiceType[] = "openscreen_.udp_";

ServiceInfo ServiceInfoFromServiceDescription(
    const local_discovery::ServiceDescription& desc) {
  openscreen::ErrorOr<openscreen::IPAddress> address =
      openscreen::IPAddress::Parse(desc.address.host());
  DCHECK(address);

  ServiceInfo service_info;
  service_info.service_id = desc.service_name;
  service_info.friendly_name = desc.instance_name();

  if (address.value().IsV4()) {
    service_info.v4_endpoint =
        openscreen::IPEndpoint{address.value(), desc.address.port()};
    service_info.v6_endpoint = {};
  } else {
    service_info.v4_endpoint = {};
    service_info.v6_endpoint =
        openscreen::IPEndpoint{address.value(), desc.address.port()};
  }

  return service_info;
}
}  // namespace

OpenScreenListener::OpenScreenListener(std::string service_type)
    : service_type_(kOpenScreenServiceType) {}

OpenScreenListener::~OpenScreenListener() {}

bool OpenScreenListener::Start() {
  is_running_ = true;

  // TODO(jophba): instantiate local_discovery::ServiceDiscoveryClient
  for (auto* observer : observers_) {
    observer->OnStarted();
  }
  return true;
}

bool OpenScreenListener::StartAndSuspend() {
  for (auto* observer : observers_) {
    observer->OnStarted();
    observer->OnSuspended();
  }
  return true;
}

bool OpenScreenListener::Stop() {
  DCHECK(is_running_);
  is_running_ = false;
  for (auto* observer : observers_) {
    observer->OnStopped();
  }
  return true;
}

bool OpenScreenListener::Suspend() {
  DCHECK(is_running_);
  is_running_ = false;
  for (auto* observer : observers_) {
    observer->OnSuspended();
  }
  return true;
}

bool OpenScreenListener::Resume() {
  DCHECK(!is_running_);
  is_running_ = true;
  for (auto* observer : observers_) {
    observer->OnStarted();
  }
  return true;
}

bool OpenScreenListener::SearchNow() {
  is_running_ = true;
  for (auto* observer : observers_) {
    observer->OnSearching();
  }
  return true;
}

const std::vector<ServiceInfo>& OpenScreenListener::GetReceivers() const {
  return receivers_;
}

void OpenScreenListener::AddObserver(ServiceListener::Observer* observer) {
  CHECK(observer);
  observers_.push_back(observer);
}

void OpenScreenListener::RemoveObserver(ServiceListener::Observer* observer) {
  CHECK(observer);
  std::erase(observers_, observer);
}

void OpenScreenListener::OnDeviceChanged(
    const std::string& service_type,
    bool added,
    const local_discovery::ServiceDescription& service_description) {
  CHECK_EQ(service_type, service_type_);
  if (!is_running_) {
    return;
  }

  ServiceInfo service_info =
      ServiceInfoFromServiceDescription(service_description);
  if (added) {
    receivers_.push_back(std::move(service_info));

    const ServiceInfo& ref = receivers_.back();
    for (auto* observer : observers_) {
      observer->OnReceiverAdded(ref);
    }
  } else {
    auto it = base::ranges::find(receivers_, service_info.service_id,
                                 &ServiceInfo::service_id);

    *it = std::move(service_info);

    for (auto* observer : observers_) {
      observer->OnReceiverChanged(*it);
    }
  }
}

void OpenScreenListener::OnDeviceRemoved(const std::string& service_type,
                                         const std::string& service_name) {
  CHECK(service_type == service_type_);
  if (!is_running_) {
    return;
  }

  const auto& removed_it =
      base::ranges::find(receivers_, service_name, &ServiceInfo::service_id);

  // Move the receiver we want to remove to the end, so we don't have to shift.
  CHECK(removed_it != receivers_.end(), base::NotFatalUntil::M130);
  const ServiceInfo removed_info = std::move(*removed_it);
  if (removed_it != receivers_.end() - 1) {
    *removed_it = std::move(receivers_.back());
  }
  receivers_.pop_back();

  for (auto* observer : observers_) {
    observer->OnReceiverRemoved(removed_info);
  }
}

void OpenScreenListener::OnDeviceCacheFlushed(const std::string& service_type) {
  CHECK(service_type == service_type_);
  receivers_.clear();

  // We still flush even if not running, since it's not going to be accurate.
  if (!is_running_) {
    return;
  }

  for (auto* observer : observers_) {
    observer->OnAllReceiversRemoved();
  }
}

}  // namespace media_router
