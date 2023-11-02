// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/geolocation/geolocation_controller_test_util.h"

namespace ash {

// -----------------------------------------------------------------------------
// GeolocationControllerObserver:

void GeolocationControllerObserver::OnGeopositionChanged(
    bool possible_change_in_timezone) {
  position_received_num_++;
  possible_change_in_timezone_ = possible_change_in_timezone;
}

// -----------------------------------------------------------------------------
// GeopositionResponsesWaiter:

GeopositionResponsesWaiter::GeopositionResponsesWaiter(
    GeolocationController* controller)
    : controller_(controller) {
  controller_->AddObserver(this);
}

GeopositionResponsesWaiter::~GeopositionResponsesWaiter() {
  controller_->RemoveObserver(this);
}

void GeopositionResponsesWaiter::Wait() {
  run_loop_.Run();
}

void GeopositionResponsesWaiter::OnGeopositionChanged(
    bool possible_change_in_timezone) {
  run_loop_.Quit();
}

}  // namespace ash