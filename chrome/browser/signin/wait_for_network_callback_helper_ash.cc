// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/wait_for_network_callback_helper_ash.h"

#include "base/functional/callback.h"
#include "chrome/browser/ash/net/delay_network_call.h"
#include "chromeos/ash/components/network/network_handler.h"

bool WaitForNetworkCallbackHelperAsh::AreNetworkCallsDelayed() {
  if (delaying_network_calls_disabled_for_testing_) {
    return false;
  }
  return ash::AreNetworkCallsDelayed();
}

void WaitForNetworkCallbackHelperAsh::DelayNetworkCall(
    base::OnceClosure callback) {
  if (!AreNetworkCallsDelayed()) {
    std::move(callback).Run();
    return;
  }
  ash::DelayNetworkCall(std::move(callback));
}

void WaitForNetworkCallbackHelperAsh::DisableNetworkCallsDelayedForTesting(
    bool disable) {
  if (!disable) {
    // Delay network calls if offline is enabled.
    // `NetworkHandler` must be initialized.
    CHECK(ash::NetworkHandler::IsInitialized())
        << "NetworkHandler must be Initialized";
  }
  delaying_network_calls_disabled_for_testing_ = disable;
}
