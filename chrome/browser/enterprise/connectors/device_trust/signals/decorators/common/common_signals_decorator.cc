// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/common_signals_decorator.h"

#include "components/policy/core/common/cloud/cloud_policy_util.h"

namespace enterprise_connectors {

CommonSignalsDecorator::CommonSignalsDecorator() = default;
CommonSignalsDecorator::~CommonSignalsDecorator() = default;

void CommonSignalsDecorator::Decorate(DeviceTrustSignals& signals) {
  signals.set_os(policy::GetOSPlatform());
  signals.set_os_version(policy::GetOSVersion());
  signals.set_device_model(policy::GetDeviceModel());
  signals.set_device_manufacturer(policy::GetDeviceManufacturer());
  signals.set_display_name(policy::GetDeviceName());
}

}  // namespace enterprise_connectors
