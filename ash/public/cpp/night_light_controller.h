// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_NIGHT_LIGHT_CONTROLLER_H_
#define ASH_PUBLIC_CPP_NIGHT_LIGHT_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/schedule_enums.h"
#include "base/observer_list.h"

namespace ash {

struct SimpleGeoposition;

class ASH_PUBLIC_EXPORT NightLightController {
 public:
  class Observer {
   public:
    // Notifies observers with the new schedule type whenever it changes.
    virtual void OnScheduleTypeChanged(ScheduleType new_type) {}

    // Emitted when the NightLight status is changed.
    virtual void OnNightLightEnabledChanged(bool enabled) {}

   protected:
    virtual ~Observer() {}
  };

  static NightLightController* GetInstance();

  NightLightController(const NightLightController&) = delete;
  NightLightController& operator=(const NightLightController&) = delete;

  // Provides the NightLightController with the user's geoposition so that it
  // can calculate the sunset and sunrise times. This should only be called when
  // the schedule type is set to "Sunset to Sunrise".
  virtual void SetCurrentGeoposition(const SimpleGeoposition& position) = 0;

  // Whether Night Light is enabled.
  virtual bool GetEnabled() const = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  NightLightController();
  virtual ~NightLightController();

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_NIGHT_LIGHT_CONTROLLER_H_
