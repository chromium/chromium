// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_IMMERSIVE_IMMERSIVE_CONTEXT_H_
#define ASH_PUBLIC_CPP_IMMERSIVE_IMMERSIVE_CONTEXT_H_

#include "ash/public/cpp/ash_public_export.h"

namespace gfx {
class Rect;
}

namespace views {
class Widget;
}

namespace ash {

class ImmersiveFullscreenController;

// ImmersiveContext abstracts away all the windowing related calls so that
// ImmersiveFullscreenController does not depend upon aura or ash.
class ASH_PUBLIC_EXPORT ImmersiveContext {
 public:
  virtual ~ImmersiveContext();

  // Returns the singleton instance.
  static ImmersiveContext* Get();

  // Used to setup state necessary for entering or existing immersive mode. It
  // is expected this interacts with the shelf, and installs any other necessary
  // state.
  virtual void OnEnteringOrExitingImmersive(
      ImmersiveFullscreenController* controller,
      bool entering) = 0;

  // Returns the bounds of the display the widget is on, in screen coordinates.
  virtual gfx::Rect GetDisplayBoundsInScreen(views::Widget* widget) = 0;

  // Returns true if any window has capture.
  virtual bool DoesAnyWindowHaveCapture() = 0;

 protected:
  ImmersiveContext();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_IMMERSIVE_IMMERSIVE_CONTEXT_H_
