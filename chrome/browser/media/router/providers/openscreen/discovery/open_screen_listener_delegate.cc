// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/openscreen/discovery/open_screen_listener_delegate.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "base/strings/string_split.h"
#include "components/openscreen_platform/network_util.h"
#include "net/base/ip_endpoint.h"
#include "third_party/openscreen/src/osp/public/osp_constants.h"

namespace media_router {

namespace {

using openscreen::osp::kOpenScreenServiceType;
using openscreen::osp::ServiceInfo;
using Config = openscreen::osp::ServiceListener::Config;
using State = openscreen::osp::ServiceListener::State;

ServiceInfo ServiceInfoFromServiceDescription(
    const local_discovery::ServiceDescription& desc) {
  ServiceInfo service_info;
  service_info.instance_name = desc.instance_name();
  net::IPEndPoint ip_endpoint(desc.ip_address, desc.address.port());
  if (ip_endpoint.address().IsIPv4()) {
    service_info.v4_endpoint =
        openscreen_platform::ToOpenScreenEndPoint(ip_endpoint);
  } else {
    service_info.v6_endpoint =
        openscreen_platform::ToOpenScreenEndPoint(ip_endpoint);
  }

  // Parse TXT records, which contain `auth_token` and `fingerprint`.
  for (auto& item : desc.metadata) {
    std::vector<std::string> results = base::SplitString(
        item, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    // We expect to find key/value pairs separated by an equals sign.
    if (results.size() != 2u) {
      continue;
    }

    if (results[0] == "at") {
      service_info.auth_token = results[1];
    } else if (results[0] == "fp") {
      service_info.fingerprint = results[1];
    }
  }

  return service_info;
}

}  // namespace

OpenScreenListenerDelegate::OpenScreenListenerDelegate(
    scoped_refptr<local_discovery::ServiceDiscoverySharedClient>
        service_discovery_client)
    : service_discovery_client_(service_discovery_client) {}

OpenScreenListenerDelegate::~OpenScreenListenerDelegate() = default;

void OpenScreenListenerDelegate::StartListener(const Config& /*config*/) {
  CreateDeviceLister();
  SetState(State::kRunning);

  device_lister_->DiscoverNewDevices();
  SetState(State::kSearching);
}

void OpenScreenListenerDelegate::StartAndSuspendListener(
    const Config& /*config*/) {
  CreateDeviceLister();
  SetState(State::kRunning);
  SetState(State::kSuspended);
}

void OpenScreenListenerDelegate::StopListener() {
  device_lister_.reset();
  receivers_.clear();
  SetState(State::kStopped);
}

void OpenScreenListenerDelegate::SuspendListener() {
  // `device_lister_` does not provide interface for suspending, so we can only
  // reset it here.
  device_lister_.reset();
  receivers_.clear();
  SetState(State::kSuspended);
}

void OpenScreenListenerDelegate::ResumeListener() {
  // `device_lister_` is reset when calling `Suspend()`, so we need to create it
  // again.
  if (!device_lister_) {
    CreateDeviceLister();
  }

  device_lister_->DiscoverNewDevices();
  SetState(State::kSearching);
}

void OpenScreenListenerDelegate::SearchNow(State /*from*/) {
  // `device_lister_` is reset when calling `Suspend()`, so we need to create it
  // again.
  if (!device_lister_) {
    CreateDeviceLister();
  }

  device_lister_->DiscoverNewDevices();
  SetState(State::kSearching);
}

void OpenScreenListenerDelegate::OnDeviceChanged(
    const std::string& service_type,
    bool added,
    const local_discovery::ServiceDescription& service_description) {
  CHECK_EQ(service_type, kOpenScreenServiceType);

  ServiceInfo service_info =
      ServiceInfoFromServiceDescription(service_description);
  if (added) {
    auto it = std::ranges::find(receivers_, service_info.instance_name,
                                &ServiceInfo::instance_name);
    CHECK(it == receivers_.end());
    receivers_.push_back(std::move(service_info));
    listener_->OnReceiverUpdated(receivers_);
  } else {
    auto it = std::ranges::find(receivers_, service_info.instance_name,
                                &ServiceInfo::instance_name);
    CHECK(it != receivers_.end());
    *it = std::move(service_info);
    listener_->OnReceiverUpdated(receivers_);
  }
}

void OpenScreenListenerDelegate::OnDeviceRemoved(
    const std::string& service_type,
    const std::string& service_name) {
  CHECK_EQ(service_type, kOpenScreenServiceType);

  std::vector<std::string> results = base::SplitString(
      service_name, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // We expect the service name follows the format
  // <instance_name>.<service_type>.
  if (results.size() != 2u) {
    return;
  }

  auto removed_it =
      std::ranges::find(receivers_, results[0], &ServiceInfo::instance_name);

  // Move the receiver we want to remove to the end, so we don't have to shift.
  CHECK(removed_it != receivers_.end());
  if (removed_it != receivers_.end() - 1) {
    *removed_it = std::move(receivers_.back());
  }
  receivers_.pop_back();
  listener_->OnReceiverUpdated(receivers_);
}

void OpenScreenListenerDelegate::OnDeviceCacheFlushed(
    const std::string& service_type) {
  CHECK_EQ(service_type, kOpenScreenServiceType);

  receivers_.clear();
  listener_->OnReceiverUpdated(receivers_);
}

void OpenScreenListenerDelegate::OnPermissionRejected() {}

void OpenScreenListenerDelegate::CreateDeviceLister() {
  CHECK(!device_lister_);

  device_lister_ = local_discovery::ServiceDiscoveryDeviceLister::Create(
      this, service_discovery_client_.get(), kOpenScreenServiceType);
  device_lister_->Start();
}

}  // namespace media_router
