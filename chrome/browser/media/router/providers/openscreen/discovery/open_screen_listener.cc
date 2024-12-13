// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/openscreen/discovery/open_screen_listener.h"

#include <cstddef>
#include <utility>

#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "third_party/openscreen/src/osp/public/osp_constants.h"
#include "third_party/openscreen/src/platform/base/ip_address.h"

namespace media_router {

namespace {

using openscreen::osp::kOpenScreenServiceType;
using openscreen::osp::ServiceInfo;

ServiceInfo ServiceInfoFromServiceDescription(
    const local_discovery::ServiceDescription& desc) {
  openscreen::ErrorOr<openscreen::IPAddress> address =
      openscreen::IPAddress::Parse(desc.ip_address.ToString());
  DCHECK(address);

  ServiceInfo service_info;
  service_info.instance_name = desc.instance_name();
  if (address.value().IsV4()) {
    service_info.v4_endpoint =
        openscreen::IPEndpoint{address.value(), desc.address.port()};
  } else {
    service_info.v6_endpoint =
        openscreen::IPEndpoint{address.value(), desc.address.port()};
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

OpenScreenListener::OpenScreenListener(
    scoped_refptr<local_discovery::ServiceDiscoverySharedClient>
        service_discovery_client)
    : service_discovery_client_(service_discovery_client) {}

OpenScreenListener::~OpenScreenListener() = default;

bool OpenScreenListener::Start() {
  if (state_ != State::kStopped) {
    return false;
  }

  CreateDeviceLister();
  state_ = State::kRunning;
  for (auto* observer : observers_) {
    observer->OnStarted();
  }

  device_lister_->DiscoverNewDevices();
  state_ = State::kSearching;
  for (auto* observer : observers_) {
    observer->OnSearching();
  }
  return true;
}

bool OpenScreenListener::StartAndSuspend() {
  if (state_ != State::kStopped) {
    return false;
  }

  CreateDeviceLister();
  state_ = State::kSuspended;
  for (auto* observer : observers_) {
    observer->OnStarted();
    observer->OnSuspended();
  }
  return true;
}

bool OpenScreenListener::Stop() {
  if (state_ == State::kStopped) {
    return false;
  }

  device_lister_.reset();
  state_ = State::kStopped;
  for (auto* observer : observers_) {
    observer->OnStopped();
  }
  return true;
}

bool OpenScreenListener::Suspend() {
  if (state_ == State::kStopped || state_ == State::kSuspended) {
    return false;
  }

  // `device_lister_` does not provide interface for suspending, so we can only
  // reset it here.
  device_lister_.reset();
  state_ = State::kSuspended;
  for (auto* observer : observers_) {
    observer->OnSuspended();
  }
  return true;
}

bool OpenScreenListener::Resume() {
  if (state_ != State::kSuspended) {
    return false;
  }

  // `device_lister_` is reset when calling `Suspend()`, so we need to create it
  // again.
  if (!device_lister_) {
    CreateDeviceLister();
  }

  device_lister_->DiscoverNewDevices();
  for (auto* observer : observers_) {
    observer->OnSearching();
  }
  return true;
}

bool OpenScreenListener::SearchNow() {
  if (state_ == State::kStopped || !device_lister_) {
    return false;
  }

  device_lister_->DiscoverNewDevices();
  for (auto* observer : observers_) {
    observer->OnSearching();
  }
  return true;
}

void OpenScreenListener::AddObserver(
    openscreen::osp::ServiceListener::Observer& observer) {
  observers_.push_back(&observer);
}

void OpenScreenListener::RemoveObserver(
    openscreen::osp::ServiceListener::Observer& observer) {
  observers_.erase(std::remove(observers_.begin(), observers_.end(), &observer),
                   observers_.end());
}

const std::vector<ServiceInfo>& OpenScreenListener::GetReceivers() const {
  return receivers_;
}

void OpenScreenListener::OnDeviceChanged(
    const std::string& service_type,
    bool added,
    const local_discovery::ServiceDescription& service_description) {
  CHECK_EQ(service_type, kOpenScreenServiceType);
  if (state_ == State::kSuspended || state_ == State::kStopped) {
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
    auto it = base::ranges::find(receivers_, service_info.instance_name,
                                 &ServiceInfo::instance_name);
    *it = std::move(service_info);

    for (auto* observer : observers_) {
      observer->OnReceiverChanged(*it);
    }
  }
}

void OpenScreenListener::OnDeviceRemoved(const std::string& service_type,
                                         const std::string& service_name) {
  CHECK_EQ(service_type, kOpenScreenServiceType);
  if (state_ == State::kSuspended || state_ == State::kStopped) {
    return;
  }

  std::vector<std::string> results = base::SplitString(
      service_name, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // We expect the service name follows the format
  // <instance_name>.<service_type>.
  if (results.size() != 2u) {
    return;
  }

  auto removed_it =
      base::ranges::find(receivers_, results[0], &ServiceInfo::instance_name);

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
  CHECK_EQ(service_type, kOpenScreenServiceType);
  receivers_.clear();

  // We still flush even if not running, since it's not going to be accurate.
  if (state_ == State::kSuspended || state_ == State::kStopped) {
    return;
  }

  for (auto* observer : observers_) {
    observer->OnAllReceiversRemoved();
  }
}

void OpenScreenListener::OnPermissionRejected() {}

void OpenScreenListener::CreateDeviceLister() {
  CHECK(!device_lister_);

  device_lister_ = local_discovery::ServiceDiscoveryDeviceLister::Create(
      this, service_discovery_client_.get(), kOpenScreenServiceType);
  device_lister_->Start();
}

}  // namespace media_router
