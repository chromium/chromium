// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/immersive_context_lacros.h"

#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

ImmersiveContextLacros::ImmersiveContextLacros() = default;

ImmersiveContextLacros::~ImmersiveContextLacros() = default;

void ImmersiveContextLacros::OnEnteringOrExitingImmersive(
    chromeos::ImmersiveFullscreenController* controller,
    bool entering) {
  NOTIMPLEMENTED_LOG_ONCE();
}

gfx::Rect ImmersiveContextLacros::GetDisplayBoundsInScreen(
    views::Widget* widget) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          widget->GetNativeWindow());
  return display.bounds();
}

bool ImmersiveContextLacros::DoesAnyWindowHaveCapture() {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}
