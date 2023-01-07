// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SENSOR_ARC_SENSOR_BRIDGE_H_
#define ASH_COMPONENTS_ARC_SENSOR_ARC_SENSOR_BRIDGE_H_

#include "ash/components/arc/mojom/sensor.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class handles Sensor-related requests from the ARC container.
class ArcSensorBridge : public KeyedService, public mojom::SensorHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcSensorBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcSensorBridge(content::BrowserContext* context,
                  ArcBridgeService* bridge_service);
  ~ArcSensorBridge() override;
  ArcSensorBridge(const ArcSensorBridge&) = delete;
  ArcSensorBridge& operator=(const ArcSensorBridge&) = delete;

  // mojom::SensorHost overrides:
  void GetSensorService(
      mojo::PendingReceiver<mojom::SensorService> receiver) override;

 private:
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SENSOR_ARC_SENSOR_BRIDGE_H_
