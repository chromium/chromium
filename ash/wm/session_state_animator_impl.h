// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SESSION_STATE_ANIMATOR_IMPL_H_
#define ASH_WM_SESSION_STATE_ANIMATOR_IMPL_H_

#include "ash/ash_export.h"
#include "ash/wm/session_state_animator.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"

namespace ui {
class LayerAnimationObserver;
}

namespace ash {

// Displays onscreen animations for session state changes (lock/unlock, sign
// out, shut down).
class ASH_EXPORT SessionStateAnimatorImpl : public SessionStateAnimator {
 public:
  // Helper class used by tests to access internal state.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(SessionStateAnimatorImpl* animator)
        : animator_(animator) {}

    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    // Returns true if containers of a given |container_mask|
    // were last animated with |type| (probably; the analysis is fairly ad-hoc).
    // |container_mask| is a bitfield of a Container.
    bool ContainersAreAnimated(int container_mask,
                               SessionStateAnimator::AnimationType type) const;

    // Returns true if root window was last animated with |type| (probably;
    // the analysis is fairly ad-hoc).
    bool RootWindowIsAnimated(SessionStateAnimator::AnimationType type) const;

   private:
    raw_ptr<SessionStateAnimatorImpl, ExperimentalAsh> animator_;  // not owned
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
