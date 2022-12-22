// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/curtain/security_curtain_controller.h"

namespace ash::curtain {

SecurityCurtainController::InitParams::InitParams()
    : event_filter(base::BindRepeating(
          [](const ui::Event&) { return FilterResult::kSuppressEvent; })) {}

SecurityCurtainController::InitParams::InitParams(EventFilter filter,
                                                  ViewFactory curtain_factory)
    : event_filter(std::move(filter)),
      curtain_factory(std::move(curtain_factory)) {}

SecurityCurtainController::InitParams::InitParams(const InitParams&) = default;
SecurityCurtainController::InitParams&
SecurityCurtainController::InitParams::operator=(const InitParams&) = default;
SecurityCurtainController::InitParams::InitParams(InitParams&&) = default;
SecurityCurtainController::InitParams&
SecurityCurtainController::InitParams::operator=(InitParams&&) = default;
SecurityCurtainController::InitParams::~InitParams() = default;

}  // namespace ash::curtain
