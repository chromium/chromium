// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/fake_service_discovery_device_lister.h"

#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/strcat.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_discovery {

DeferringDelegate::DeferringDelegate() = default;

DeferringDelegate::~DeferringDelegate() = default;

void DeferringDelegate::OnDeviceChanged(
    const std::string& service_type,
    bool added,
    const ServiceDescription& service_description) {
  if (actual_) {
    actual_->OnDeviceChanged(service_type, added, service_description);
  } else {
    deferred_callbacks_.push_back(base::BindOnce(
        &DeferringDelegate::OnDeviceChanged, base::Unretained(this),
        service_type, added, service_description));
  }
}

void DeferringDelegate::OnDeviceRemoved(const std::string& service_type,
                                        const std::string& service_name) {
  if (actual_) {
    actual_->OnDeviceRemoved(service_type, service_name);
  } else {
    deferred_callbacks_.push_back(
        base::BindOnce(&DeferringDelegate::OnDeviceRemoved,
                       base::Unretained(this), service_type, service_name));
  }
}

void DeferringDelegate::OnDeviceCacheFlushed(const std::string& service_type) {
  if (actual_) {
    actual_->OnDeviceCacheFlushed(service_type);
  } else {
    deferred_callbacks_.push_back(
        base::BindOnce(&DeferringDelegate::OnDeviceCacheFlushed,
                       base::Unretained(this), service_type));
  }
}

void DeferringDelegate::SetActual(
    ServiceDiscoveryDeviceLister::Delegate* actual) {
  CHECK(!actual_);
  actual_ = actual;
  for (auto& cb : deferred_callbacks_)
    std::move(cb).Run();

  deferred_callbacks_.clear();
}

FakeServiceDiscoveryDeviceLister::FakeServiceDiscoveryDeviceLister(
    base::TaskRunner* task_runner,
    const std::string& service_type)
    : task_runner_(task_runner), service_type_(service_type) {}

FakeServiceDiscoveryDeviceLister::~FakeServiceDiscoveryDeviceLister() = default;

void FakeServiceDiscoveryDeviceLister::Start() {
  if (start_called_)
    ADD_FAILURE() << "Start called multiple times";

  start_called_ = true;
}

void FakeServiceDiscoveryDeviceLister::DiscoverNewDevices() {
  if (!start_called_)
    ADD_FAILURE() << "DiscoverNewDevices called before Start";

  discovery_started_ = true;
  for (const auto& update : queued_updates_)
    SendUpdate(update);

  queued_updates_.clear();
}

const std::string& FakeServiceDiscoveryDeviceLister::service_type() const {
  return service_type_;
}

void FakeServiceDiscoveryDeviceLister::SetDelegate(
    ServiceDiscoveryDeviceLister::Delegate* delegate) {
  deferring_delegate_.SetActual(delegate);
}

void FakeServiceDiscoveryDeviceLister::Announce(
    const ServiceDescription& description) {
  if (description.service_type() != service_type_)
    return;

  if (!discovery_started_)
    queued_updates_.push_back(description);
  else
    SendUpdate(description);
}

void FakeServiceDiscoveryDeviceLister::Remove(const std::string& name) {
  std::string service_name = base::StrCat({name, ".", service_type_});
  announced_services_.erase(service_name);
  CHECK(task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceDiscoveryDeviceLister::Delegate::OnDeviceRemoved,
                     base::Unretained(&deferring_delegate_), service_type_,
                     service_name)));
}

void FakeServiceDiscoveryDeviceLister::Clear() {
  announced_services_.clear();
  discovery_started_ = false;
  CHECK(task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ServiceDiscoveryDeviceLister::Delegate::OnDeviceCacheFlushed,
          base::Unretained(&deferring_delegate_), service_type_)));
}

bool FakeServiceDiscoveryDeviceLister::discovery_started() {
  return discovery_started_;
}

void FakeServiceDiscoveryDeviceLister::SendUpdate(
    const ServiceDescription& description) {
  bool is_new;
  if (!base::Contains(announced_services_, description.service_name)) {
    is_new = true;
    announced_services_.insert(description.service_name);
  } else {
    is_new = false;
  }

  CHECK(task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceDiscoveryDeviceLister::Delegate::OnDeviceChanged,
                     base::Unretained(&deferring_delegate_), service_type_,
                     is_new, description)));
}

}  // namespace local_discovery
