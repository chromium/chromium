// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NEARBY_FAKE_QUICK_START_CONNECTIVITY_SERVICE_H_
#define CHROME_BROWSER_ASH_NEARBY_FAKE_QUICK_START_CONNECTIVITY_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/nearby/quick_start_connectivity_service.h"
#include "chromeos/ash/components/nearby/common/connections_manager/fake_nearby_connections_manager.h"
#include "chromeos/ash/components/quick_start/fake_quick_start_decoder.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace ash::quick_start {

class FakeQuickStartConnectivityService : public QuickStartConnectivityService {
 public:
  FakeQuickStartConnectivityService();
  FakeQuickStartConnectivityService(const FakeQuickStartConnectivityService&) =
      delete;
  FakeQuickStartConnectivityService& operator=(
      const FakeQuickStartConnectivityService&) = delete;
  ~FakeQuickStartConnectivityService() override;

  // QuickStartConnectivityService:
  raw_ptr<NearbyConnectionsManager> GetNearbyConnectionsManager() override;
  mojo::SharedRemote<mojom::QuickStartDecoder> GetQuickStartDecoder() override;
  void Cleanup() override;

  raw_ptr<FakeNearbyConnectionsManager> GetFakeNearbyConnectionsManager();

  bool get_is_cleanup_called() { return is_cleanup_called_; }

 private:
  FakeNearbyConnectionsManager fake_nearby_connections_manager_;
  std::unique_ptr<FakeQuickStartDecoder> fake_quick_start_decoder_ =
      std::make_unique<FakeQuickStartDecoder>();
  bool is_cleanup_called_ = false;
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_NEARBY_FAKE_QUICK_START_CONNECTIVITY_SERVICE_H_
