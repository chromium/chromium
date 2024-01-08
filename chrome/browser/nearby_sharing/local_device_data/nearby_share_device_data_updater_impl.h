// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_DEVICE_DATA_UPDATER_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_DEVICE_DATA_UPDATER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_device_data_updater.h"
#include "chromeos/ash/components/nearby/common/client/nearby_http_result.h"
#include "third_party/nearby/sharing/proto/device_rpc.pb.h"

class NearbyShareClient;
class NearbyShareClientFactory;

// An implementation of NearbyShareDeviceDataUpdater that uses the
// NearbyShareClient to make UpdateDevice RPC calls to the Nearby server. An
// UpdateDeviceData() attempt will fail if a response is not received within the
// specified timeout value.
class NearbyShareDeviceDataUpdaterImpl : public NearbyShareDeviceDataUpdater {
 public:
  class Factory {
   public:
    static std::unique_ptr<NearbyShareDeviceDataUpdater> Create(
        const std::string& device_id,
        base::TimeDelta timeout,
        NearbyShareClientFactory* client_factory);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<NearbyShareDeviceDataUpdater> CreateInstance(
        const std::string& device_id,
        base::TimeDelta timeout,
        NearbyShareClientFactory* client_factory) = 0;

   private:
    static Factory* test_factory_;
  };

  ~NearbyShareDeviceDataUpdaterImpl() override;

 private:
  NearbyShareDeviceDataUpdaterImpl(const std::string& device_id,
                                   base::TimeDelta timeout,
                                   NearbyShareClientFactory* client_factory);

  void HandleNextRequest() override;
  void OnTimeout();
  void OnRpcSuccess(
      const nearby::sharing::proto::UpdateDeviceResponse& response);
  void OnRpcFailure(ash::nearby::NearbyHttpError error);

  base::TimeDelta timeout_;
  raw_ptr<NearbyShareClientFactory> client_factory_ = nullptr;
  std::unique_ptr<NearbyShareClient> client_;
  base::OneShotTimer timer_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_LOCAL_DEVICE_DATA_NEARBY_SHARE_DEVICE_DATA_UPDATER_IMPL_H_
