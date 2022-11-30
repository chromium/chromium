// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_EXTERNAL_ARC_OVERLAY_ARC_OVERLAY_CONTROLLER_H_
#define ASH_PUBLIC_CPP_EXTERNAL_ARC_OVERLAY_ARC_OVERLAY_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace aura {
class Window;
}

namespace ash {

class ASH_PUBLIC_EXPORT ArcOverlayController {
 public:
  ArcOverlayController();
  virtual ~ArcOverlayController();

  // Attaches the window that is intended to be used as the overlay.
  // This is expected to be a toplevel window, and it will be reparented.
  virtual void AttachOverlay(aura::Window* overlay_window) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_EXTERNAL_ARC_OVERLAY_ARC_OVERLAY_CONTROLLER_H_
