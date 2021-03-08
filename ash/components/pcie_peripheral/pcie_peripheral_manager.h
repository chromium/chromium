// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PCIE_PERIPHERAL_PCIE_PERIPHERAL_MANAGER_H_
#define ASH_COMPONENTS_PCIE_PERIPHERAL_PCIE_PERIPHERAL_MANAGER_H_

#include <memory>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/dbus/typecd/typecd_client.h"

namespace ash {

// This class is responsible for listening to TypeCd and Pciguard D-Bus calls
// and translating those signals to notification observer events. It handles
// additional logic such determining if notifications are required or whether a
// guest-session notification is needed.
class COMPONENT_EXPORT(ASH_PCIE_PERIPHERAL) PciePeripheralManager
    : public chromeos::TypecdClient::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called to notify observers, primarily notification controllers, that a
    // recently plugged in Thunderbolt/USB4 device is running at limited
    // performance. This can be called multiple times.
    virtual void OnLimitedPerformancePeripheralReceived() = 0;

    // Called to notify observers, primarily notification controllers, that a
    // Thunderbolt/USB4 device has been plugged in during a guest session. Can
    // be called multiple times.
    virtual void OnGuestModeNotificationReceived(bool is_thunderbolt_only) = 0;
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum PciePeripheralConnectivityResults {
    kTBTSupportedAndAllowed = 0,
    kTBTOnlyAndBlockedByPciguard = 1,
    kTBTOnlyAndBlockedInGuestSession = 2,
    kAltModeFallbackDueToPciguard = 3,
    kAltModeFallbackInGuestSession = 4,
    kMaxValue = kAltModeFallbackInGuestSession,
  };

  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize(bool is_guest_profile, bool is_pcie_tunneling_allowed);

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance pointer.
  static PciePeripheralManager* Get();

  // Returns true if the global instance is initialized.
  static bool IsInitialized();

  void SetPcieTunnelingAllowedState(bool is_pcie_tunneling_allowed);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class PciePeripheralManagerTest;

  PciePeripheralManager(bool is_guest_profile, bool is_pcie_tunneling_allowed);
  PciePeripheralManager(const PciePeripheralManager&) = delete;
  PciePeripheralManager& operator=(const PciePeripheralManager&) = delete;
  ~PciePeripheralManager() override;

  // TypecdClient::Observer:
  void OnThunderboltDeviceConnected(bool is_thunderbolt_only) override;

  // Call to notify observers that a new notification is needed.
  void NotifyLimitedPerformancePeripheralReceived();
  void NotifyGuestModeNotificationReceived(bool is_thunderbolt_only);

  const bool is_guest_profile_;
  // Pcie tunneling refers to allowing Thunderbolt/USB4 peripherals to run at
  // full capacity by utilizing the PciExpress protocol. If this is set to
  // false, we anticipate that the plugged in Thunderbolt/USB4 periphal is
  // operating at either Alt-mode (i.e. fallback to an older protocol) or
  // in a restricted state (e.g. certain devices are Thunderbolt only).
  bool is_pcie_tunneling_allowed_;
  base::ObserverList<Observer> observer_list_;
};

}  // namespace ash

#endif  // ASH_COMPONENTS_PCIE_PERIPHERAL_PCIE_PERIPHERAL_MANAGER_H_
