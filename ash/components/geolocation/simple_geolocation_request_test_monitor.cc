// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/geolocation/simple_geolocation_request_test_monitor.h"

namespace ash {

SimpleGeolocationRequestTestMonitor::SimpleGeolocationRequestTestMonitor() =
    default;

SimpleGeolocationRequestTestMonitor::~SimpleGeolocationRequestTestMonitor() =
    default;

void SimpleGeolocationRequestTestMonitor::OnRequestCreated(
    SimpleGeolocationRequest* request) {}

void SimpleGeolocationRequestTestMonitor::OnStart(
    SimpleGeolocationRequest* request) {}

}  // namespace ash
