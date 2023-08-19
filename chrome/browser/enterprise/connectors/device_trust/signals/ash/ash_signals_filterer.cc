// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/ash/ash_signals_filterer.h"

#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/device_signals/core/common/signals_constants.h"

namespace enterprise_connectors {

AshSignalsFilterer::~AshSignalsFilterer() = default;

void AshSignalsFilterer::Filter(base::Value::Dict& signals) {
  if (ShouldRemoveStableDeviceIdentifiers()) {
    RemoveStableDeviceIdentifiers(signals);
  }
}

void AshSignalsFilterer::RemoveStableDeviceIdentifiers(
    base::Value::Dict& signals) {
  signals.Remove(device_signals::names::kDeviceHostName);
  signals.Remove(device_signals::names::kDisplayName);
  signals.Remove(device_signals::names::kImei);
  signals.Remove(device_signals::names::kMacAddresses);
  signals.Remove(device_signals::names::kMeid);
  signals.Remove(device_signals::names::kSerialNumber);
  signals.Remove(device_signals::names::kSystemDnsServers);
}

bool AshSignalsFilterer::ShouldRemoveStableDeviceIdentifiers() {
  // Stable device identifiers should to be removed only if the device is
  // unmanaged.
  return !ash::InstallAttributes::Get()->IsEnterpriseManaged();
}

}  // namespace enterprise_connectors
