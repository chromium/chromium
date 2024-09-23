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

std::string NetworkPortalDetectorTestImpl::GetDefaultNetworkGuid() const {
  if (!default_network_) {
    return "";
  }

  return default_network_->guid();
}

bool NetworkPortalDetectorTestImpl::IsEnabled() {
  return enabled_;
}

void NetworkPortalDetectorTestImpl::Enable() {
  DVLOG(1) << "NetworkPortalDetectorTestImpl: Enabled.";
  enabled_ = true;
}

}  // namespace ash
