// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/device_policy_status_provider_lacros.h"

#include <utility>
#include "base/values.h"

DevicePolicyStatusProviderLacros::DevicePolicyStatusProviderLacros()
    : PolicyStatusProvider() {}

DevicePolicyStatusProviderLacros::~DevicePolicyStatusProviderLacros() {}

void DevicePolicyStatusProviderLacros::SetDevicePolicyStatus(
    base::Value::Dict status) {
  device_policy_status_ = std::move(status);
}

base::Value::Dict DevicePolicyStatusProviderLacros::GetStatus() {
  return device_policy_status_.Clone();
}
