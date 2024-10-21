// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_TEST_NETWORK_STATE_MIXIN_H_
#define CHROME_BROWSER_ASH_APP_MODE_TEST_NETWORK_STATE_MIXIN_H_

#include <optional>

#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"

namespace ash {

// Wraps a `NetworkStateTestHelper` to set it up and surface a simpler API to
// toggle network states between online and offline.
class NetworkStateMixin : public InProcessBrowserTestMixin {
 public:
  explicit NetworkStateMixin(InProcessBrowserTestMixinHost* host);

  NetworkStateMixin(const NetworkStateMixin&) = delete;
  NetworkStateMixin& operator=(const NetworkStateMixin&) = delete;

  ~NetworkStateMixin() override;

  void SimulateOffline();

  void SimulateOnline();

  NetworkStateTestHelper& network_state_test_helper() {
    return network_helper_.value();
  }

 protected:
  void SetUpOnMainThread() override;

  void TearDownOnMainThread() override;

 private:
  // Used to change network online/offline states. Configured to start offline.
  std::optional<NetworkStateTestHelper> network_helper_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_TEST_NETWORK_STATE_MIXIN_H_
