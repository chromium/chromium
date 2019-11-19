// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/queued_display_change.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "base/bind.h"
#include "ui/display/display.h"

namespace keyboard {

QueuedDisplayChange::QueuedDisplayChange(const display::Display& display,
                                         const gfx::Rect& new_bounds_in_local)
    : new_display_(display), new_bounds_in_local_(new_bounds_in_local) {}

QueuedDisplayChange::~QueuedDisplayChange() = default;

}  // namespace keyboard
