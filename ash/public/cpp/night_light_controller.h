// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_NIGHT_LIGHT_CONTROLLER_H_
#define ASH_PUBLIC_CPP_NIGHT_LIGHT_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

class ASH_PUBLIC_EXPORT NightLightController {
 public:
  // These values are written to logs. New enum values can be added, but
  // existing enums must never be renumbered or deleted and reused.
  enum ScheduleType {
    // Automatic toggling of NightLight is turned off.
    kNone = 0,

    // Turned automatically on at the user's local sunset time, and off at the
    // user's local sunrise time.
    kSunsetToSunrise = 1,

    // Toggled automatically based on the custom set start and end times
    // selected by the user from the system settings.
    kCustom = 2,

    // kMaxValue is required for UMA_HISTOGRAM_ENUMERATION.
    kMaxValue = kCustom,
  };

  // Represents a geolocation position fix. It's "simple" because it doesn't
  // expose all the parameters of the position interface as defined by the
  // Geolocation API Specification:
  //   https://dev.w3.org/geo/api/spec-source.html#position_interface
  // The NightLightController is only interested in valid latitude and
  // longitude. It also doesn't require any specific accuracy. The more accurate
  // the positions, the more accurate sunset and sunrise times calculations.
  // However, an IP-based geoposition is considered good enough.
  struct SimpleGeoposition {
    bool operator==(const SimpleGeoposition& other) const {
      return latitude == other.latitude && longitude == other.longitude;
    }
    double latitude;
    double longitude;
  };

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

  // Provides the NightLightController with the user's geoposition so that it
  // can calculate the sunset and sunrise times. This should only be called when
  // the schedule type is set to "Sunset to Sunrise".
  virtual void SetCurrentGeoposition(const SimpleGeoposition& position) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  NightLightController();
  virtual ~NightLightController();

  base::ObserverList<Observer>::Unchecked observers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NightLightController);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_NIGHT_LIGHT_CONTROLLER_H_
