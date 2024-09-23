// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/curtain/security_curtain_controller.h"

namespace ash::curtain {

SecurityCurtainController::InitParams::InitParams() = default;

SecurityCurtainController::InitParams::InitParams(ViewFactory curtain_factory)
    : curtain_factory(std::move(curtain_factory)) {}

SecurityCurtainController::InitParams::InitParams(const InitParams&) = default;
SecurityCurtainController::InitParams&
SecurityCurtainController::InitParams::operator=(const InitParams&) = default;
SecurityCurtainController::InitParams::InitParams(InitParams&&) = default;
SecurityCurtainController::InitParams&
SecurityCurtainController::InitParams::operator=(InitParams&&) = default;
SecurityCurtainController::InitParams::~InitParams() = default;

}  // namespace ash::curtain
