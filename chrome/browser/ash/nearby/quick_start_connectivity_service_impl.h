// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NEARBY_QUICK_START_CONNECTIVITY_SERVICE_IMPL_H_
#define CHROME_BROWSER_ASH_NEARBY_QUICK_START_CONNECTIVITY_SERVICE_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/nearby/quick_start_connectivity_service.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_process_manager.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

class NearbyConnectionsManager;

namespace ash::quick_start {

// TODO(b/280308935): Shut down Nearby Connections when we exit the Quick Start
// flow.
class QuickStartConnectivityServiceImpl : public QuickStartConnectivityService {
 public:
  explicit QuickStartConnectivityServiceImpl(
      nearby::NearbyProcessManager* nearby_process_manager);
  QuickStartConnectivityServiceImpl(const QuickStartConnectivityServiceImpl&) =
      delete;
  QuickStartConnectivityServiceImpl& operator=(
      const QuickStartConnectivityServiceImpl&) = delete;
  ~QuickStartConnectivityServiceImpl() override;

  // A NearbyConnectionsManager is created the first time a reference is
  // requested via this method. On service shutdown the NearbyConnectionsManager
  // will be destroyed and the utility process will be terminated.
  raw_ptr<NearbyConnectionsManager> GetNearbyConnectionsManager() override;

  mojo::SharedRemote<mojom::QuickStartDecoder> GetQuickStartDecoder() override;

  void Cleanup() override;

 private:
  void OnNearbyProcessStopped(
      nearby::NearbyProcessManager::NearbyProcessShutdownReason
          shutdown_reason);

  std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager_;
  raw_ptr<nearby::NearbyProcessManager> nearby_process_manager_;

  std::unique_ptr<nearby::NearbyProcessManager::NearbyProcessReference>
      nearby_process_reference_;

  base::WeakPtrFactory<QuickStartConnectivityServiceImpl> weak_ptr_factory_{
      this};
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_NEARBY_QUICK_START_CONNECTIVITY_SERVICE_IMPL_H_
