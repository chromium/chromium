// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_FAKE_NEARBY_SHARE_DEVICE_DATA_UPDATER_H_
#define CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_FAKE_NEARBY_SHARE_DEVICE_DATA_UPDATER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_device_data_updater.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_device_data_updater_impl.h"
#include "third_party/nearby/sharing/proto/device_rpc.pb.h"

// An fake implementation of NearbyShareDeviceDataUpdater for use in unit tests.
class FakeNearbyShareDeviceDataUpdater : public NearbyShareDeviceDataUpdater {
 public:
  explicit FakeNearbyShareDeviceDataUpdater(const std::string& device_id);
  ~FakeNearbyShareDeviceDataUpdater() override;

  // Advances the request queue and invokes request callback with the input
  // parameter |response|.
  void RunNextRequest(
      const std::optional<nearby::sharing::proto::UpdateDeviceResponse>&
          response);

  const std::string& device_id() const { return device_id_; }

  const base::queue<Request>& pending_requests() const {
    return pending_requests_;
  }

 private:
  // NearbyShareDeviceDataUpdater:
  void HandleNextRequest() override;
};

// A fake NearbyShareDeviceDataUpdaterImpl factory for use in unit tests.
class FakeNearbyShareDeviceDataUpdaterFactory
    : public NearbyShareDeviceDataUpdaterImpl::Factory {
 public:
  FakeNearbyShareDeviceDataUpdaterFactory();
  ~FakeNearbyShareDeviceDataUpdaterFactory() override;

  // Returns all FakeNearbyShareDeviceDataUpdater instances created by
  // CreateInstance().
  std::vector<raw_ptr<FakeNearbyShareDeviceDataUpdater, VectorExperimental>>&
  instances() {
    return instances_;
  }

  base::TimeDelta latest_timeout() const { return latest_timeout_; }

  NearbyShareClientFactory* latest_client_factory() const {
    return latest_client_factory_;
  }

 private:
  // NearbyShareDeviceDataUpdaterImpl::Factory:
  std::unique_ptr<NearbyShareDeviceDataUpdater> CreateInstance(
      const std::string& device_id,
      base::TimeDelta timeout,
      NearbyShareClientFactory* client_factory) override;

  std::vector<raw_ptr<FakeNearbyShareDeviceDataUpdater, VectorExperimental>>
      instances_;
  base::TimeDelta latest_timeout_;
  raw_ptr<NearbyShareClientFactory> latest_client_factory_ = nullptr;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_FAKE_NEARBY_SHARE_DEVICE_DATA_UPDATER_H_
