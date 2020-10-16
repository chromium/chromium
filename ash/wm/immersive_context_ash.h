// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_IMMERSIVE_CONTEXT_ASH_H_
#define ASH_WM_IMMERSIVE_CONTEXT_ASH_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "chromeos/ui/frame/immersive/immersive_context.h"

namespace ash {

class ASH_EXPORT ImmersiveContextAsh : public chromeos::ImmersiveContext {
 public:
  ImmersiveContextAsh();
  ~ImmersiveContextAsh() override;

  // ImmersiveContext:
  void OnEnteringOrExitingImmersive(
      chromeos::ImmersiveFullscreenController* controller,
      bool entering) override;
  gfx::Rect GetDisplayBoundsInScreen(views::Widget* widget) override;
  bool DoesAnyWindowHaveCapture() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImmersiveContextAsh);
};

}  // namespace ash

#endif  // ASH_WM_IMMERSIVE_CONTEXT_ASH_H_
