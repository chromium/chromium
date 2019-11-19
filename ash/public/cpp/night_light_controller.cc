// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/night_light_controller.h"

#include "base/logging.h"

namespace ash {
namespace {
NightLightController* g_night_light_controller = nullptr;
}

// static
NightLightController* NightLightController::GetInstance() {
  return g_night_light_controller;
}

NightLightController::NightLightController() {
  DCHECK(!g_night_light_controller);
  g_night_light_controller = this;
}
NightLightController::~NightLightController() {
  DCHECK_EQ(g_night_light_controller, this);
  g_night_light_controller = nullptr;
}

void NightLightController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NightLightController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
