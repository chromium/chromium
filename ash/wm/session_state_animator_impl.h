// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SESSION_STATE_ANIMATOR_IMPL_H_
#define ASH_WM_SESSION_STATE_ANIMATOR_IMPL_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/session_state_animator.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"

namespace ui {
class LayerAnimationObserver;
}  // namespace ui

namespace ash {

// Displays onscreen animations for session state changes (lock/unlock, sign
// out, shut down).
class ASH_EXPORT SessionStateAnimatorImpl : public SessionStateAnimator {
 public:
  // Child containers of `NON_LOCK_SCREEN_CONTAINERS` should be animated on
  // session state changes.
  static constexpr int ContainersToAnimateInNonLockScreenContainer[] = {
      kShellWindowId_HomeScreenContainer,  kShellWindowId_AlwaysOnTopContainer,
      kShellWindowId_FloatContainer,       kShellWindowId_PipContainer,
      kShellWindowId_SystemModalContainer,
  };

  SessionStateAnimatorImpl();

  SessionStateAnimatorImpl(const SessionStateAnimatorImpl&) = delete;
  SessionStateAnimatorImpl& operator=(const SessionStateAnimatorImpl&) = delete;

  ~SessionStateAnimatorImpl() override;

  // Fills |containers| with the containers included in |container_mask|.
  static void GetContainers(int container_mask,
                            aura::Window::Windows* containers);

  // ash::SessionStateAnimator:
  void StartAnimation(int container_mask,
                      AnimationType type,
                      AnimationSpeed speed) override;
  void StartAnimationWithCallback(int container_mask,
                                  AnimationType type,
                                  AnimationSpeed speed,
                                  base::OnceClosure callback) override;
  AnimationSequence* BeginAnimationSequence(
      AnimationCallback callback) override;
  bool IsWallpaperHidden() const override;
  void ShowWallpaper() override;
  void HideWallpaper() override;

 private:
  class AnimationSequence;
  friend class AnimationSequence;

  virtual void StartAnimationInSequence(int container_mask,
                                        AnimationType type,
                                        AnimationSpeed speed,
                                        AnimationSequence* observer);

  // Apply animation |type| to window |window| with |speed| and add |observer|
  // if it is not NULL to the last animation sequence.
  void RunAnimationForWindow(aura::Window* window,
                             SessionStateAnimator::AnimationType type,
                             SessionStateAnimator::AnimationSpeed speed,
                             ui::LayerAnimationObserver* observer);
};

}  // namespace ash

#endif  // ASH_WM_SESSION_STATE_ANIMATOR_IMPL_H_
