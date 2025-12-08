// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/mocks/mock_handoff_button_controller.h"

namespace actor::ui {

MockHandoffButtonController::MockHandoffButtonController(
    views::View* anchor_view,
    ActorUiWindowController* window_controller)
    : HandoffButtonController(anchor_view, window_controller) {}
MockHandoffButtonController::~MockHandoffButtonController() = default;

}  // namespace actor::ui
