// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_UTILITY_ROUNDED_WINDOW_TARGETER_H_
#define ASH_UTILITY_ROUNDED_WINDOW_TARGETER_H_

#include "ash/ash_export.h"
#include "ui/aura/window_targeter.h"
#include "ui/gfx/geometry/rrect_f.h"

namespace ash {

// A WindowTargeter for windows with rounded corners.
class ASH_EXPORT RoundedWindowTargeter : public aura::WindowTargeter {
 public:
  // Constructor for a circular window defined by |radius|.
  explicit RoundedWindowTargeter(int radius);
  RoundedWindowTargeter(int width, int height, int radius);
  RoundedWindowTargeter(const RoundedWindowTargeter&) = delete;
  RoundedWindowTargeter& operator=(const RoundedWindowTargeter&) = delete;
  ~RoundedWindowTargeter() override;

 private:
  // aura::WindowTargeter:
  bool EventLocationInsideBounds(aura::Window* target,
                                 const ui::LocatedEvent& event) const override;

  gfx::RRectF rrectf_;
};

}  // namespace ash

#endif  // ASH_UTILITY_ROUNDED_WINDOW_TARGETER_H_
