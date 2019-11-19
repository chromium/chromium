// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/network_portal_detector_mixin.h"

#include "base/run_loop.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chromeos/dbus/shill/fake_shill_manager_client.h"

namespace chromeos {

NetworkPortalDetectorMixin::NetworkPortalDetectorMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

NetworkPortalDetectorMixin::~NetworkPortalDetectorMixin() = default;

void NetworkPortalDetectorMixin::SetDefaultNetwork(
    const std::string& network_guid,
    NetworkPortalDetector::CaptivePortalStatus status) {
  network_portal_detector_->SetDefaultNetworkForTesting(network_guid);

  SimulateDefaultNetworkState(status);
}

void NetworkPortalDetectorMixin::SimulateNoNetwork() {
  network_portal_detector_->SetDefaultNetworkForTesting("");

  network_portal_detector_->NotifyObserversForTesting();
}

void NetworkPortalDetectorMixin::SimulateDefaultNetworkState(
    NetworkPortalDetector::CaptivePortalStatus status) {
  std::string default_network_guid =
      network_portal_detector_->GetDefaultNetworkGuid();
  DCHECK(!default_network_guid.empty());

  NetworkPortalDetector::CaptivePortalState detection_state;
  detection_state.status = status;
  if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE) {
    detection_state.response_code = 204;
  } else if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL) {
    detection_state.response_code = 200;
  } else {
    detection_state.response_code = -1;
  }

  network_portal_detector_->SetDetectionResultsForTesting(default_network_guid,
                                                          detection_state);
  network_portal_detector_->NotifyObserversForTesting();
}

void NetworkPortalDetectorMixin::WaitForPortalDetectionRequest() {
  if (network_portal_detector_->portal_detection_in_progress())
    return;
  base::RunLoop run_loop;
  network_portal_detector_->RegisterPortalDetectionStartCallback(
      run_loop.QuitClosure());
  run_loop.Run();
}

void NetworkPortalDetectorMixin::SetUpInProcessBrowserTestFixture() {
  // Setup network portal detector to return online state for both
  // ethernet and wifi networks. Ethernet is an active network by
  // default.
  network_portal_detector_ = new NetworkPortalDetectorTestImpl();
  network_portal_detector::InitializeForTesting(network_portal_detector_);
  SetDefaultNetwork(FakeShillManagerClient::kFakeEthernetNetworkGuid,
                    NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
}
void NetworkPortalDetectorMixin::TearDownInProcessBrowserTestFixture() {
  network_portal_detector::Shutdown();
}

}  // namespace chromeos
