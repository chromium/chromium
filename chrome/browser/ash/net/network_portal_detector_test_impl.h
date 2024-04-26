// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_PORTAL_DETECTOR_TEST_IMPL_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_PORTAL_DETECTOR_TEST_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"

namespace ash {

class NetworkState;

class NetworkPortalDetectorTestImpl : public NetworkPortalDetector {
 public:
  NetworkPortalDetectorTestImpl();

  NetworkPortalDetectorTestImpl(const NetworkPortalDetectorTestImpl&) = delete;
  NetworkPortalDetectorTestImpl& operator=(
      const NetworkPortalDetectorTestImpl&) = delete;

  ~NetworkPortalDetectorTestImpl() override;

  void SetDefaultNetworkForTesting(const std::string& guid);

  // Returns the GUID of the network the detector considers to be default.
  std::string GetDefaultNetworkGuid() const;

  // NetworkPortalDetector implementation:
  bool IsEnabled() override;
  void Enable() override;

 private:
  bool enabled_ = false;
  std::unique_ptr<NetworkState> default_network_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_PORTAL_DETECTOR_TEST_IMPL_H_
