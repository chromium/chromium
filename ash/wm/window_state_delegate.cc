// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_state_delegate.h"

namespace ash {

WindowStateDelegate::WindowStateDelegate() = default;

WindowStateDelegate::~WindowStateDelegate() = default;

bool WindowStateDelegate::ToggleFullscreen(WindowState* window_state) {
  return false;
}

}  // namespace ash
