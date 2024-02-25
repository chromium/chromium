// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/local_device_data/fake_nearby_share_device_data_updater.h"

FakeNearbyShareDeviceDataUpdater::FakeNearbyShareDeviceDataUpdater(
    const std::string& device_id)
    : NearbyShareDeviceDataUpdater(device_id) {}

FakeNearbyShareDeviceDataUpdater::~FakeNearbyShareDeviceDataUpdater() = default;

void FakeNearbyShareDeviceDataUpdater::RunNextRequest(
    const std::optional<nearby::sharing::proto::UpdateDeviceResponse>&
        response) {
  FinishAttempt(response);
}

void FakeNearbyShareDeviceDataUpdater::HandleNextRequest() {}

FakeNearbyShareDeviceDataUpdaterFactory::
    FakeNearbyShareDeviceDataUpdaterFactory() = default;

FakeNearbyShareDeviceDataUpdaterFactory::
    ~FakeNearbyShareDeviceDataUpdaterFactory() = default;

std::unique_ptr<NearbyShareDeviceDataUpdater>
FakeNearbyShareDeviceDataUpdaterFactory::CreateInstance(
    const std::string& device_id,
    base::TimeDelta timeout,
    NearbyShareClientFactory* client_factory) {
  latest_timeout_ = timeout;
  latest_client_factory_ = client_factory;

  auto instance = std::make_unique<FakeNearbyShareDeviceDataUpdater>(device_id);
  instances_.push_back(instance.get());

  return instance;
}
