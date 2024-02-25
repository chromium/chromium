// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_GEOLOCATION_GEOLOCATION_CONTROLLER_TEST_UTIL_H_
#define ASH_SYSTEM_GEOLOCATION_GEOLOCATION_CONTROLLER_TEST_UTIL_H_

#include "ash/system/geolocation/geolocation_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"

namespace ash {

// An observer class to GeolocationController which updates sunset and sunrise
// time.
class GeolocationControllerObserver : public GeolocationController::Observer {
 public:
  GeolocationControllerObserver() = default;

  GeolocationControllerObserver(const GeolocationControllerObserver&) = delete;
  GeolocationControllerObserver& operator=(
      const GeolocationControllerObserver&) = delete;

  ~GeolocationControllerObserver() override = default;

  // GeolocationController::Observer:
  void OnGeopositionChanged(bool possible_change_in_timezone) override;

  int position_received_num() const { return position_received_num_; }
  bool possible_change_in_timezone() const {
    return possible_change_in_timezone_;
  }

 private:
  // The number of times a new position is received.
  int position_received_num_ = 0;
  bool possible_change_in_timezone_ = false;
};

// Used for waiting for the geolocation controller to send geoposition fetched
// from the server to all observers.
class GeopositionResponsesWaiter : public GeolocationController::Observer {
 public:
  explicit GeopositionResponsesWaiter(GeolocationController* controller);

  GeopositionResponsesWaiter(const GeopositionResponsesWaiter&) = delete;
  GeopositionResponsesWaiter& operator=(const GeopositionResponsesWaiter&) =
      delete;

  ~GeopositionResponsesWaiter() override;

  void Wait();

  // GeolocationController::Observer:
  void OnGeopositionChanged(bool possible_change_in_timezone) override;

 private:
  const raw_ptr<GeolocationController> controller_;
  base::RunLoop run_loop_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_GEOLOCATION_GEOLOCATION_CONTROLLER_TEST_UTIL_H_
