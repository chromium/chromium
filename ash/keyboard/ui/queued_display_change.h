// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UI_QUEUED_DISPLAY_CHANGE_H_
#define ASH_KEYBOARD_UI_QUEUED_DISPLAY_CHANGE_H_

#include "base/bind.h"
#include "base/optional.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"

namespace keyboard {

// TODO(shend): refactor and merge this into QueuedContainerType and rename it
// to something like QueuedVisualChange or similar.
class QueuedDisplayChange {
 public:
  QueuedDisplayChange(const display::Display& display,
                      const gfx::Rect& new_bounds_in_local);
  ~QueuedDisplayChange();

  display::Display new_display() { return new_display_; }
  gfx::Rect new_bounds_in_local() { return new_bounds_in_local_; }

 private:
  display::Display new_display_;
  gfx::Rect new_bounds_in_local_;
};

}  // namespace keyboard

#endif  // ASH_KEYBOARD_UI_QUEUED_DISPLAY_CHANGE_H_
