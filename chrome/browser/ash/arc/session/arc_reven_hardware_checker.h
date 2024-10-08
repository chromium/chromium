// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_ARC_REVEN_HARDWARE_CHECKER_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_ARC_REVEN_HARDWARE_CHECKER_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {

// This class is responsible for checking whether the hardware is compatible
// with ARC (Android Runtime for Chrome). It interacts with the CrosHealthd
// service to obtain hardware telemetry and perform compatibility checks.
class ArcRevenHardwareChecker {
 public:
  ArcRevenHardwareChecker();
  virtual ~ArcRevenHardwareChecker();
  // Initiates a hardware compatibility check. The result of the check will
  // be reported through the provided callback.
  virtual void IsRevenDeviceCompatibleForArc(
      base::OnceCallback<void(bool)> callback);

 private:
  // Callback invoked when the CrosHealthd service disconnects.
  void OnDisconnect();

  // Checks for non-removable block devices and calls a provided
  // callback function when they are ready.
  void OnCheckNonRemovableBlockDevices(
      base::OnceCallback<void(bool)> callback,
      ash::cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  // Retries the check for non-removable block devices.
  void OnRetryNonRemovableBlockDevicesCheck(
      base::OnceCallback<void(bool)> callback);

  // Callback invoked when telemetry information is received from CrosHealthd.
  // This processes the telemetry info and invokes the provided callback with
  // the result of the hardware compatibility check.
  void OnRevenHardwareChecked(
      base::OnceCallback<void(bool)> callback,
      ash::cros_healthd::mojom::TelemetryInfoPtr info_ptr);

  // Checks whether the system memory meets the compatibility requirements.
  // Requirement: System memory (RAM) must be at least 4 GB.
  bool CheckMemoryRequirements(
      const ash::cros_healthd::mojom::TelemetryInfoPtr& info_ptr) const;

  // Checks whether the CPU meets the compatibility requirements.
  // Requirement: Host CPU must support hardware virtualization.
  bool CheckCpuRequirements(
      const ash::cros_healthd::mojom::TelemetryInfoPtr& info_ptr) const;

  // Checks whether the storage meets the compatibility requirements.
  // Requirements:
  // 1. Boot disk must not be a spinning HDD.
  // 2. Disk must be at least 32 GB.
  bool CheckStorageRequirements(
      const ash::cros_healthd::mojom::TelemetryInfoPtr& info_ptr) const;

  // Checks whether the PCI devices meet the compatibility requirements.
  // Requirement: WiFi/GPU must be one of the supported chipsets.
  bool CheckPciRequirements(
      const ash::cros_healthd::mojom::TelemetryInfoPtr& info_ptr) const;

  // The probe for interacting with the CrosHealthd service to obtain hardware
  // telemetry information.
  mojo::Remote<ash::cros_healthd::mojom::CrosHealthdProbeService>
      probe_service_;

  // Counter for the number of retry attempts made when checking for
  // non-removable block devices.
  int retry_count_ = 0;

  // One-shot timer for retrying checks on non-removable block devices.
  base::OneShotTimer retry_timer_;

  static const std::unordered_set<std::string> kSupportedWiFiIds;
  static const std::unordered_set<std::string> kSupportedGpuIds;

  // Weak pointer factory for safely handling callbacks
  base::WeakPtrFactory<ArcRevenHardwareChecker> weak_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_ARC_REVEN_HARDWARE_CHECKER_H_
