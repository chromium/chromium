// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_NETWORK_PORTAL_DETECTOR_MIXIN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_NETWORK_PORTAL_DETECTOR_MIXIN_H_

#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"

namespace chromeos {

class NetworkPortalDetectorTestImpl;

// An InBrowserProcessTestMixin that provides utility methods for faking
// network captive portal detector state.
class NetworkPortalDetectorMixin : public InProcessBrowserTestMixin {
 public:
  explicit NetworkPortalDetectorMixin(InProcessBrowserTestMixinHost* host);
  ~NetworkPortalDetectorMixin() override;

  NetworkPortalDetectorMixin(const NetworkPortalDetectorMixin& other) = delete;
  NetworkPortalDetectorMixin& operator=(
      const NetworkPortalDetectorMixin& other) = delete;

  // Changes the default network as seen by network portal detector, sets its
  // state and notifies NetworkPortalDetector observers of the portal detection
  // completion.
  void SetDefaultNetwork(const std::string& network_guid,
                         NetworkPortalDetector::CaptivePortalStatus status);

  // Simulates no network state. It notifies NetworkPortalDetector observers of
  // the portal detection state.
  void SimulateNoNetwork();

  // Sets the default network's captive portal state. It notifies
  // NetworkPortalDetector observers of the new portal detection state.
  void SimulateDefaultNetworkState(
      NetworkPortalDetector::CaptivePortalStatus status);

  // Runs loop until the NetworkPortalDetector is requested to start portal
  // detection. It will return immediately if a detection is already in
  // progress.
  void WaitForPortalDetectionRequest();

  // InProcessBrowserTestMixin:
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownInProcessBrowserTestFixture() override;

 private:
  NetworkPortalDetectorTestImpl* network_portal_detector_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_NETWORK_PORTAL_DETECTOR_MIXIN_H_
