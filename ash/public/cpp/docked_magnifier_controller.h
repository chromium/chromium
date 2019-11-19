// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_DOCKED_MAGNIFIER_CONTROLLER_H_
#define ASH_PUBLIC_CPP_DOCKED_MAGNIFIER_CONTROLLER_H_

#include "ash/ash_export.h"

namespace gfx {
class Point;
}

namespace ash {

// Used by Chrome to notify ash of focus change events of nodes in webpages.
class ASH_EXPORT DockedMagnifierController {
 public:
  // Returns the instance of docked magnifier.
  static DockedMagnifierController* Get();

  // Requests that the Docked Magnifier centers its viewport around this given
  // screen point. This can be used by a client (e.g. Chrome) to notify ash of
  // focus change events in e.g. webpages when feature is enabled. Note that ash
  // observes the focus change events of the text input carets in editable nodes
  // by itself.
  virtual void CenterOnPoint(const gfx::Point& point_in_screen) = 0;

  // Returns docked magnifier height.
  virtual int GetMagnifierHeightForTesting() const = 0;

 protected:
  virtual ~DockedMagnifierController() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_DOCKED_MAGNIFIER_CONTROLLER_H_
