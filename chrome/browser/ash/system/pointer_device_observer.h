// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_POINTER_DEVICE_OBSERVER_H_
#define CHROME_BROWSER_ASH_SYSTEM_POINTER_DEVICE_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace ash {
namespace system {

class PointerDeviceObserver : public ui::InputDeviceEventObserver {
 public:
  PointerDeviceObserver();

  PointerDeviceObserver(const PointerDeviceObserver&) = delete;
  PointerDeviceObserver& operator=(const PointerDeviceObserver&) = delete;

  ~PointerDeviceObserver() override;

  // Start observing device hierarchy.
  void Init();

  // Check for presence of devices.
  void CheckDevices();

  class Observer {
   public:
    virtual void TouchpadExists(bool exists) = 0;
    virtual void HapticTouchpadExists(bool exists) = 0;
    virtual void MouseExists(bool exists) = 0;
    virtual void PointingStickExists(bool exists) = 0;

   protected:
    Observer() {}
    virtual ~Observer();
  };
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // InputDeviceEventObserver:
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;

  // Check for pointer devices.
  void CheckTouchpadExists();
  void CheckHapticTouchpadExists();
  void CheckMouseExists();
  void CheckPointingStickExists();

  // Callback for pointer device checks.
  void OnTouchpadExists(bool exists);
  void OnHapticTouchpadExists(bool exists);
  void OnMouseExists(bool exists);
  void OnPointingStickExists(bool exists);

  base::ObserverList<Observer>::Unchecked observers_;

  base::WeakPtrFactory<PointerDeviceObserver> weak_factory_{this};
};

}  // namespace system
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_POINTER_DEVICE_OBSERVER_H_
