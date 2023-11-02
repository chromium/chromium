// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_MEMORY_PRESSURE_ARC_MEMORY_PRESSURE_BRIDGE_H_
#define ASH_COMPONENTS_ARC_MEMORY_PRESSURE_ARC_MEMORY_PRESSURE_BRIDGE_H_

#include "ash/components/arc/metrics/arc_metrics_service.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

class ArcMemoryPressureBridge
    : public KeyedService,
      public ash::ResourcedClient::ArcVmObserver,
      public ConnectionObserver<mojom::ProcessInstance> {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcMemoryPressureBridge* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcMemoryPressureBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcMemoryPressureBridge(content::BrowserContext* context,
                          ArcBridgeService* bridge);

  ArcMemoryPressureBridge(const ArcMemoryPressureBridge&) = delete;
  ArcMemoryPressureBridge& operator=(const ArcMemoryPressureBridge&) = delete;

  ~ArcMemoryPressureBridge() override;

  // Implements ResourcedClient::ArcVmObserver.
  void OnMemoryPressure(ash::ResourcedClient::PressureLevelArcVm level,
                        uint64_t reclaim_target_kb) override;

  // ConnectionObserver<mojom::ProcessInstance> overrides.
  // We use the OnConnectionClosed method to know when we should reset
  // memory_pressure_in_flight_.
  void OnConnectionClosed() override;

 private:
  // Called by Mojo when the memory pressure call into ARCVM completes.
  // killed - The number of apps killed in reponse to this signal.
  // reclaimed - An estimate of the number of bytes freed.
  void OnHostMemoryPressureComplete(uint32_t killed, uint64_t reclaimed);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
  ArcMetricsService* const arc_metrics_service_;

  // Set between OnMemoryPressure and OnHostMemoryPressureComplete, so we can
  // throttle calls into ARCVM if it is unresponsive.
  bool memory_pressure_in_flight_ = false;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<ArcMemoryPressureBridge> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_MEMORY_PRESSURE_ARC_MEMORY_PRESSURE_BRIDGE_H_
