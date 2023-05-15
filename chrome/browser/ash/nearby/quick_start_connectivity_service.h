// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NEARBY_QUICK_START_CONNECTIVITY_SERVICE_H_
#define CHROME_BROWSER_ASH_NEARBY_QUICK_START_CONNECTIVITY_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_process_manager.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"

class NearbyConnectionsManager;

namespace ash::quick_start {

class QuickStartConnectivityService : public KeyedService {
 public:
  explicit QuickStartConnectivityService(
      nearby::NearbyProcessManager* nearby_process_manager);
  QuickStartConnectivityService(const QuickStartConnectivityService&) = delete;
  QuickStartConnectivityService& operator=(
      const QuickStartConnectivityService&) = delete;
  ~QuickStartConnectivityService() override;

  // A NearbyConnectionsManager is created the first time a reference is
  // requested via this method. On service shutdown the NearbyConnectionsManager
  // will be destroyed and the utility process will be terminated.
  base::WeakPtr<NearbyConnectionsManager> GetNearbyConnectionsManager();

  mojo::SharedRemote<mojom::QuickStartDecoder> GetQuickStartDecoder();

 private:
  void OnNearbyProcessStopped(
      nearby::NearbyProcessManager::NearbyProcessShutdownReason
          shutdown_reason);

  std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager_;
  raw_ptr<nearby::NearbyProcessManager, ExperimentalAsh>
      nearby_process_manager_;

  std::unique_ptr<nearby::NearbyProcessManager::NearbyProcessReference>
      nearby_process_reference_;

  base::WeakPtrFactory<QuickStartConnectivityService> weak_ptr_factory_{this};
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_NEARBY_QUICK_START_CONNECTIVITY_SERVICE_H_
