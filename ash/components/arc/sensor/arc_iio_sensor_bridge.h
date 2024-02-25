// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SENSOR_ARC_IIO_SENSOR_BRIDGE_H_
#define ASH_COMPONENTS_ARC_SENSOR_ARC_IIO_SENSOR_BRIDGE_H_

#include <optional>

#include "ash/components/arc/mojom/iio_sensor.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class handles Sensor-related requests from the ARC container.
class ArcIioSensorBridge : public KeyedService,
                           public mojom::IioSensorHost,
                           public ConnectionObserver<mojom::IioSensorInstance>,
                           public chromeos::PowerManagerClient::Observer {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcIioSensorBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcIioSensorBridge(content::BrowserContext* context,
                     ArcBridgeService* bridge_service);
  ~ArcIioSensorBridge() override;
  ArcIioSensorBridge(const ArcIioSensorBridge&) = delete;
  ArcIioSensorBridge& operator=(const ArcIioSensorBridge&) = delete;

  // mojom::IioSensorHost overrides:
  void RegisterSensorHalClient(
      mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient> remote)
      override;

  // ConnectionObserver<mojom::IioSensorInstance> overrides:
  void OnConnectionReady() override;

  // chromeos::PowerManagerClient::Observer overrides:
  void TabletModeEventReceived(chromeos::PowerManagerClient::TabletMode mode,
                               base::TimeTicks timestamp) override;

  static void EnsureFactoryBuilt();

 private:
  // Send tablet mode changed event to ARC.
  void SendTabletMode();

  // Sets is_tablet_mode_on and sends the event to ARC.
  void SetIsTabletModeOn(bool is_tablet_mode_on);

  // Called with PowerManagerClient::GetSwitchStates() result.
  void OnGetSwitchStates(
      std::optional<chromeos::PowerManagerClient::SwitchStates> states);

  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.
  std::optional<bool> is_tablet_mode_on_;

  base::WeakPtrFactory<ArcIioSensorBridge> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SENSOR_ARC_IIO_SENSOR_BRIDGE_H_
