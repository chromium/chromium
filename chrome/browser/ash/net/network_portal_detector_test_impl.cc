// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_portal_detector_test_impl.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash {

NetworkPortalDetectorTestImpl::NetworkPortalDetectorTestImpl() = default;

NetworkPortalDetectorTestImpl::~NetworkPortalDetectorTestImpl() = default;

void NetworkPortalDetectorTestImpl::SetDefaultNetworkForTesting(
    const std::string& guid) {
  DVLOG(1) << "SetDefaultNetworkForTesting: " << guid;
  if (guid.empty()) {
    default_network_.reset();
  } else {
    default_network_ = std::make_unique<NetworkState>("/service/" + guid);
    default_network_->SetGuid(guid);
  }
}

void NetworkPortalDetectorTestImpl::SetDetectionResultsForTesting(
    const std::string& guid,
    CaptivePortalStatus status,
    int response_code) {
  DVLOG(1) << "SetDetectionResultsForTesting: " << guid << " = "
           << NetworkPortalDetector::CaptivePortalStatusString(status);
  if (!guid.empty())
    portal_status_map_[guid] = status;
}

std::string NetworkPortalDetectorTestImpl::GetDefaultNetworkGuid() const {
  if (!default_network_)
    return "";

  return default_network_->guid();
}

NetworkPortalDetector::CaptivePortalStatus
NetworkPortalDetectorTestImpl::GetCaptivePortalStatus() {
  if (!default_network_)
    return CAPTIVE_PORTAL_STATUS_UNKNOWN;
  auto it = portal_status_map_.find(default_network_->guid());
  if (it == portal_status_map_.end())
    return CAPTIVE_PORTAL_STATUS_UNKNOWN;
  return it->second;
}

bool NetworkPortalDetectorTestImpl::IsEnabled() {
  return enabled_;
}

void NetworkPortalDetectorTestImpl::Enable() {
  DVLOG(1) << "NetworkPortalDetectorTestImpl: Enabled.";
  enabled_ = true;
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->SetCheckPortalList(
        NetworkStateHandler::kDefaultCheckPortalList);
  }
}

}  // namespace ash
