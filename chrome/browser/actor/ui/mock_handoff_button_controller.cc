// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/mock_handoff_button_controller.h"

namespace actor::ui {

MockHandoffButtonController::MockHandoffButtonController(
    tabs::TabInterface& tab_interface)
    : HandoffButtonController(tab_interface) {}
MockHandoffButtonController::~MockHandoffButtonController() = default;

}  // namespace actor::ui
