// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/sensor/arc_iio_sensor_bridge.h"

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/memory/singleton.h"
#include "chromeos/components/sensors/ash/sensor_hal_dispatcher.h"

namespace arc {

namespace {

// Singleton factory for ArcIioSensorBridge.
class ArcIioSensorBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcIioSensorBridge,
          ArcIioSensorBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcIioSensorBridgeFactory";

  static ArcIioSensorBridgeFactory* GetInstance() {
    return base::Singleton<ArcIioSensorBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcIioSensorBridgeFactory>;
  ArcIioSensorBridgeFactory() = default;
  ~ArcIioSensorBridgeFactory() override = default;
};

}  // namespace

// static
ArcIioSensorBridge* ArcIioSensorBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcIioSensorBridgeFactory::GetForBrowserContext(context);
}

ArcIioSensorBridge::ArcIioSensorBridge(content::BrowserContext* context,
                                       ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  arc_bridge_service_->iio_sensor()->AddObserver(this);
  arc_bridge_service_->iio_sensor()->SetHost(this);

  // Get the current tablet mode.
  chromeos::PowerManagerClient::Get()->GetSwitchStates(base::BindOnce(
      &ArcIioSensorBridge::OnGetSwitchStates, weak_ptr_factory_.GetWeakPtr()));
}

ArcIioSensorBridge::~ArcIioSensorBridge() {
  arc_bridge_service_->iio_sensor()->SetHost(nullptr);
  arc_bridge_service_->iio_sensor()->RemoveObserver(this);
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

void ArcIioSensorBridge::RegisterSensorHalClient(
    mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient> remote) {
  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterClient(
      std::move(remote));
}

void ArcIioSensorBridge::OnConnectionReady() {
  // Send the current tablet mode just after initialization.
  if (is_tablet_mode_on_.has_value())
    SendTabletMode();
}

void ArcIioSensorBridge::TabletModeEventReceived(
    chromeos::PowerManagerClient::TabletMode mode,
    base::TimeTicks timestamp) {
  SetIsTabletModeOn(mode == chromeos::PowerManagerClient::TabletMode::ON);
}

void ArcIioSensorBridge::SendTabletMode() {
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->iio_sensor(), OnTabletModeChanged);
  if (instance)
    instance->OnTabletModeChanged(is_tablet_mode_on_.value());
}

void ArcIioSensorBridge::SetIsTabletModeOn(bool is_tablet_mode_on) {
  is_tablet_mode_on_ = is_tablet_mode_on;
  SendTabletMode();
}

void ArcIioSensorBridge::OnGetSwitchStates(
    std::optional<chromeos::PowerManagerClient::SwitchStates> states) {
  if (states.has_value()) {
    SetIsTabletModeOn(states->tablet_mode ==
                      chromeos::PowerManagerClient::TabletMode::ON);
  }
}

// static
void ArcIioSensorBridge::EnsureFactoryBuilt() {
  ArcIioSensorBridgeFactory::GetInstance();
}

}  // namespace arc
