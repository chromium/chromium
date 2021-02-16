// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_MOCK_NEARBY_PROCESS_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_MOCK_NEARBY_PROCESS_MANAGER_H_

#include "chrome/browser/nearby_sharing/nearby_process_manager.h"

#include "testing/gmock/include/gmock/gmock.h"

using NearbyConnectionsMojom =
    location::nearby::connections::mojom::NearbyConnections;
using NearbySharingDecoderMojom = sharing::mojom::NearbySharingDecoder;

class MockNearbyProcessManager : public NearbyProcessManager {
 public:
  MockNearbyProcessManager();
  MockNearbyProcessManager(const MockNearbyProcessManager&) = delete;
  MockNearbyProcessManager& operator=(const MockNearbyProcessManager&) = delete;
  ~MockNearbyProcessManager() override;

  MOCK_METHOD(NearbyConnectionsMojom*,
              GetOrStartNearbyConnections,
              (Profile * profile),
              (override));
  MOCK_METHOD(NearbySharingDecoderMojom*,
              GetOrStartNearbySharingDecoder,
              (Profile * profile),
              (override));
  MOCK_METHOD(bool, IsActiveProfile, (Profile * profile), (const, override));
  MOCK_METHOD(void, StopProcess, (Profile * profile), (override));
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_MOCK_NEARBY_PROCESS_MANAGER_H_
