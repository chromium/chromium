// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_OBSERVER_UMA_H_
#define ASH_TOUCH_TOUCH_OBSERVER_UMA_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/gesture_action_type.h"
#include "base/macros.h"
#include "base/memory/singleton.h"

namespace aura {
class Window;
}

namespace ui {
class GestureEvent;
class TouchEvent;
}

namespace ash {

// Records some touch/gesture event specific details (e.g. what gestures are
// targeted to which components etc.)
class ASH_EXPORT TouchUMA {
 public:
  // Returns the singleton instance.
  static TouchUMA* GetInstance();

  void RecordGestureEvent(aura::Window* target, const ui::GestureEvent& event);
  void RecordGestureAction(GestureActionType action);
  void RecordTouchEvent(aura::Window* target, const ui::TouchEvent& event);

 private:
  friend struct base::DefaultSingletonTraits<TouchUMA>;

  TouchUMA();
  ~TouchUMA();

  GestureActionType FindGestureActionType(aura::Window* window,
                                          const ui::GestureEvent& event);

  base::TimeTicks last_touch_down_time_;

  DISALLOW_COPY_AND_ASSIGN(TouchUMA);
};

}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_OBSERVER_UMA_H_
