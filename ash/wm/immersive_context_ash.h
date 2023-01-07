// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_IMMERSIVE_CONTEXT_ASH_H_
#define ASH_WM_IMMERSIVE_CONTEXT_ASH_H_

#include "ash/ash_export.h"
#include "chromeos/ui/frame/immersive/immersive_context.h"

namespace ash {

class ASH_EXPORT ImmersiveContextAsh : public chromeos::ImmersiveContext {
 public:
  ImmersiveContextAsh();

  ImmersiveContextAsh(const ImmersiveContextAsh&) = delete;
  ImmersiveContextAsh& operator=(const ImmersiveContextAsh&) = delete;

  ~ImmersiveContextAsh() override;

  // ImmersiveContext:
  void OnEnteringOrExitingImmersive(
      chromeos::ImmersiveFullscreenController* controller,
      bool entering) override;
  gfx::Rect GetDisplayBoundsInScreen(views::Widget* widget) override;
  bool DoesAnyWindowHaveCapture() override;
};

}  // namespace ash

#endif  // ASH_WM_IMMERSIVE_CONTEXT_ASH_H_
