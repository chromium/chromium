// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_SYSTEM_DATA_PROVIDER_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_SYSTEM_DATA_PROVIDER_H_

#include <memory>
#include <optional>

#include "ash/webui/diagnostics_ui/backend/system/cpu_usage_data.h"
#include "ash/webui/diagnostics_ui/mojom/system_data_provider.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace base {
class RepeatingTimer;
}  // namespace base

namespace ash {
namespace diagnostics {

class SystemDataProvider : public mojom::SystemDataProvider,
                           public chromeos::PowerManagerClient::Observer {
 public:
  SystemDataProvider();

  ~SystemDataProvider() override;

  SystemDataProvider(const SystemDataProvider&) = delete;
  SystemDataProvider& operator=(const SystemDataProvider&) = delete;

  // mojom::SystemDataProvider:
  void GetSystemInfo(GetSystemInfoCallback callback) override;
  void GetBatteryInfo(GetBatteryInfoCallback callback) override;
  void ObserveBatteryChargeStatus(
      mojo::PendingRemote<mojom::BatteryChargeStatusObserver> observer)
      override;
  void ObserveBatteryHealth(
      mojo::PendingRemote<mojom::BatteryHealthObserver> observer) override;
  void ObserveMemoryUsage(
      mojo::PendingRemote<mojom::MemoryUsageObserver> observer) override;
  void ObserveCpuUsage(
      mojo::PendingRemote<mojom::CpuUsageObserver> observer) override;

  // PowerManagerClient::Observer:
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

  void BindInterface(
      mojo::PendingReceiver<mojom::SystemDataProvider> pending_receiver);

  void SetBatteryChargeStatusTimerForTesting(
      std::unique_ptr<base::RepeatingTimer> timer);

  void SetBatteryHealthTimerForTesting(
      std::unique_ptr<base::RepeatingTimer> timer);

  void SetMemoryUsageTimerForTesting(
      std::unique_ptr<base::RepeatingTimer> timer);

  void SetCpuUsageTimerForTesting(std::unique_ptr<base::RepeatingTimer> timer);

 private:
  void BindCrosHealthdProbeServiceIfNeccessary();

  void OnProbeServiceDisconnect();

  void OnSystemInfoProbeResponse(
      GetSystemInfoCallback callback,
      cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  void OnBatteryInfoProbeResponse(
      GetBatteryInfoCallback callback,
      cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  void UpdateBatteryChargeStatus();

  void UpdateBatteryHealth();

  void UpdateMemoryUsage();

  void UpdateCpuUsage();

  void NotifyBatteryChargeStatusObservers(
      const mojom::BatteryChargeStatusPtr& battery_charge_status);

  void NotifyBatteryHealthObservers(
      const mojom::BatteryHealthPtr& battery_health);

  void NotifyMemoryUsageObservers(const mojom::MemoryUsagePtr& memory_usage);

  void NotifyCpuUsageObservers(const mojom::CpuUsagePtr& cpu_usage);

  void OnBatteryChargeStatusUpdated(
      const std::optional<power_manager::PowerSupplyProperties>&
          power_supply_properties,
      cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  void OnBatteryHealthUpdated(cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  void OnMemoryUsageUpdated(cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  void OnCpuUsageUpdated(cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  void ComputeAndPopulateCpuUsage(const cros_healthd::mojom::CpuInfo& cpu_info,
                                  mojom::CpuUsage& out_cpu_usage);

  CpuUsageData previous_cpu_usage_data_;

  mojo::Remote<cros_healthd::mojom::CrosHealthdProbeService> probe_service_;
  mojo::RemoteSet<mojom::BatteryChargeStatusObserver>
      battery_charge_status_observers_;
  mojo::RemoteSet<mojom::BatteryHealthObserver> battery_health_observers_;
  mojo::RemoteSet<mojom::MemoryUsageObserver> memory_usage_observers_;
  mojo::RemoteSet<mojom::CpuUsageObserver> cpu_usage_observers_;

  mojo::Receiver<mojom::SystemDataProvider> receiver_{this};

  std::unique_ptr<base::RepeatingTimer> battery_charge_status_timer_;
  std::unique_ptr<base::RepeatingTimer> battery_health_timer_;
  std::unique_ptr<base::RepeatingTimer> memory_usage_timer_;
  std::unique_ptr<base::RepeatingTimer> cpu_usage_timer_;

  base::WeakPtrFactory<SystemDataProvider> weak_factory_{this};
};

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SYSTEM_SYSTEM_DATA_PROVIDER_H_
