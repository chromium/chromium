// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/device_name_policy_handler.h"

namespace policy {

DeviceNamePolicyHandler::DeviceNamePolicyHandler() = default;

DeviceNamePolicyHandler::~DeviceNamePolicyHandler() = default;

void DeviceNamePolicyHandler::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void DeviceNamePolicyHandler::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void DeviceNamePolicyHandler::NotifyHostnamePolicyChanged() {
  for (auto& observer : observer_list_)
    observer.OnHostnamePolicyChanged();
}

}  // namespace policy
