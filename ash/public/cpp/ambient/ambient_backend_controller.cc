// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ambient/ambient_backend_controller.h"

#include <utility>

#include "base/check_op.h"

namespace ash {

namespace {

AmbientBackendController* g_ambient_backend_controller = nullptr;

}  // namespace

// static
AmbientBackendController* AmbientBackendController::Get() {
  return g_ambient_backend_controller;
}

// AmbientModeTopic-------------------------------------------------------------
AmbientModeTopic::AmbientModeTopic() = default;

AmbientModeTopic::AmbientModeTopic(const AmbientModeTopic&) = default;

AmbientModeTopic& AmbientModeTopic::operator=(const AmbientModeTopic&) =
    default;

AmbientModeTopic::AmbientModeTopic(AmbientModeTopic&&) = default;

AmbientModeTopic& AmbientModeTopic::operator=(AmbientModeTopic&&) = default;

AmbientModeTopic::~AmbientModeTopic() = default;

// WeatherInfo------------------------------------------------------------------
WeatherInfo::WeatherInfo() = default;

WeatherInfo::WeatherInfo(const WeatherInfo&) = default;

WeatherInfo& WeatherInfo::operator=(const WeatherInfo&) = default;

WeatherInfo::~WeatherInfo() = default;

// ScreenUpdate-----------------------------------------------------------------
ScreenUpdate::ScreenUpdate() = default;

ScreenUpdate::ScreenUpdate(const ScreenUpdate&) = default;

ScreenUpdate& ScreenUpdate::operator=(const ScreenUpdate&) = default;

ScreenUpdate::~ScreenUpdate() = default;

AmbientBackendController::AmbientBackendController() {
  DCHECK(!g_ambient_backend_controller);
  g_ambient_backend_controller = this;
}

AmbientBackendController::~AmbientBackendController() {
  DCHECK_EQ(g_ambient_backend_controller, this);
  g_ambient_backend_controller = nullptr;
}

}  // namespace ash
