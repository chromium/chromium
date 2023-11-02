// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SECURE_CHANNEL_FAKE_NEARBY_ENDPOINT_FINDER_H_
#define CHROME_BROWSER_ASH_SECURE_CHANNEL_FAKE_NEARBY_ENDPOINT_FINDER_H_

#include "chrome/browser/ash/secure_channel/nearby_endpoint_finder.h"

namespace ash {
namespace secure_channel {

class FakeNearbyEndpointFinder : public NearbyEndpointFinder {
 public:
  FakeNearbyEndpointFinder();
  ~FakeNearbyEndpointFinder() override;

  // Make functions public for testing.
  using NearbyEndpointFinder::NotifyEndpointDiscoveryFailure;
  using NearbyEndpointFinder::NotifyEndpointFound;
  using NearbyEndpointFinder::remote_device_bluetooth_address;

 private:
  // NearbyEndpointFinder:
  void PerformFindEndpoint() override {}
};

}  // namespace secure_channel
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SECURE_CHANNEL_FAKE_NEARBY_ENDPOINT_FINDER_H_
