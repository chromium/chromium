// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_NETWORK_PORTAL_DETECTOR_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_NETWORK_PORTAL_DETECTOR_MIXIN_H_

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

class NetworkPortalDetectorTestImpl;

// DEPRECATED, DO NOT USE IN NEW TESTS. NetworkStateHandler should be used
// to track portal state. This mixin is maintained for compatibility with
// existing tests. See NetworkStateTestHelper for testing with NetworkState.

// An InBrowserProcessTestMixin that provides utility methods for faking
// network captive portal detector state.
class NetworkPortalDetectorMixin : public InProcessBrowserTestMixin {
 public:
  enum class NetworkStatus {
    kUnknown,
    kOffline,
    kOnline,
    kPortal,
  };

  explicit NetworkPortalDetectorMixin(InProcessBrowserTestMixinHost* host);
  ~NetworkPortalDetectorMixin() override;

  NetworkPortalDetectorMixin(const NetworkPortalDetectorMixin& other) = delete;
  NetworkPortalDetectorMixin& operator=(
      const NetworkPortalDetectorMixin& other) = delete;

  // Changes the default network as seen by network portal detector, sets its
  // state and notifies NetworkPortalDetector observers of the portal detection
  // completion.
  void SetDefaultNetwork(const std::string& network_guid,
                         const std::string& network_type,
                         NetworkStatus network_status);

  // Simulates no network state. It notifies NetworkPortalDetector observers of
  // the portal detection state.
  void SimulateNoNetwork();

  // Sets the default network's captive portal state. It notifies
  // NetworkPortalDetector observers of the new portal detection state.
  void SimulateDefaultNetworkState(NetworkStatus status);

  // InProcessBrowserTestMixin:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 private:
  void SetShillDefaultNetwork(const std::string& network_guid,
                              const std::string& network_type,
                              NetworkStatus status);

  raw_ptr<NetworkPortalDetectorTestImpl, DanglingUntriaged>
      network_portal_detector_ = nullptr;
  std::string default_network_guid_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_NETWORK_PORTAL_DETECTOR_MIXIN_H_
