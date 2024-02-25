// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_DUPLICATE_ID_FINDER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_DUPLICATE_ID_FINDER_H_

#include "ash/ash_export.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace ash {

// Responsible for keeping track of which device ids have the same VID/PID as
// each other.
class ASH_EXPORT InputDeviceDuplicateIdFinder
    : public ui::InputDeviceEventObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDuplicateDevicesUpdated() {}
  };

  InputDeviceDuplicateIdFinder();
  InputDeviceDuplicateIdFinder(const InputDeviceDuplicateIdFinder&) = delete;
  InputDeviceDuplicateIdFinder& operator=(const InputDeviceDuplicateIdFinder&) =
      delete;
  ~InputDeviceDuplicateIdFinder() override;

  // ui::InputDeviceEventObserver
  void OnInputDeviceConfigurationChanged(uint8_t input_device_type) override;
  void OnDeviceListsComplete() override;

  // Gets the set of duplicate device ids for the given `device_id`.
  const base::flat_set<int>* GetDuplicateDeviceIds(int id) const;
  const base::flat_set<int>* GetDuplicateDeviceIds(
      const VendorProductId& vid_pid) const;
  std::optional<VendorProductId> GetVendorProductIdForDevice(int id) const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  void RefreshKeyboards();
  void RefreshMice();
  void RefreshPointingSticks();
  void RefreshTouchpads();
  void RefreshTouchscreens();
  void RefreshGraphicsTablets();
  void RefreshUncategorized();

  void NotifyObservers();

  // Contains the list of device ids that map back to the given VID/PID
  // Identifier.
  base::flat_map<VendorProductId, base::flat_set<int>> duplicate_ids_map_;
  // Maps from id -> VID/PID identifier to make searching quicker.
  base::flat_map<int, VendorProductId> vid_pid_map_;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INPUT_DEVICE_DUPLICATE_ID_FINDER_H_
