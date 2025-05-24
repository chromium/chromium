// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/test/network_state_mixin.h"

#include <optional>
#include <string>

#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

NetworkStateMixin::NetworkStateMixin(InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

NetworkStateMixin::~NetworkStateMixin() = default;

void NetworkStateMixin::SimulateOffline() {
  network_helper_.value().ResetDevicesAndServices();
}

void NetworkStateMixin::SimulateOnline() {
  network_helper_.value().ConfigureWiFi(shill::kStateOnline);
}

void NetworkStateMixin::SetUpOnMainThread() {
  network_helper_.emplace(
      /*use_default_devices_and_services=*/false);
}

void NetworkStateMixin::TearDownOnMainThread() {
  network_helper_.reset();
}

}  // namespace ash
