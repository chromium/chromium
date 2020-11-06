// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_FAKE_NEARBY_ENDPOINT_FINDER_H_
#define CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_FAKE_NEARBY_ENDPOINT_FINDER_H_

#include "chrome/browser/chromeos/secure_channel/nearby_endpoint_finder.h"

namespace chromeos {
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
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_FAKE_NEARBY_ENDPOINT_FINDER_H_
