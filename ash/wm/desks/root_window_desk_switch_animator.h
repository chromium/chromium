// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_ROOT_WINDOW_DESK_SWITCH_ANIMATOR_H_
#define ASH_WM_DESKS_ROOT_WINDOW_DESK_SWITCH_ANIMATOR_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/compositor/layer_animation_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class LayerTreeOwner;
class Layer;
}  // namespace ui

namespace viz {
class CopyOutputResult;
}  // namespace viz

namespace ash {

class Desk;

// Performs the desk switch animation on a root window (i.e. display). Since a
// desk spans all displays, one instance of this object will be created for each
// display when a new desk is activated.
//
// Screenshots of the starting and ending desks are taken, and we animate
// between them such that the starting desk can appear sliding out of the
// screen, while the ending desk is sliding in. We take screenshots to make the
// visible state of the desks seem constant to the user (e.g. if the starting
// desk is in overview, it appears to remain in overview while sliding out).
// This approach makes it possible to show an empty black space separating both
// desks while we animate them (See |kDesksSpacing|).
//
// - `starting` desk: is the currently activated desk which will be deactivated
//    shortly.
// - `ending` desk: is the desk desired to be activated with this animation.
//
// The animation goes through the following phases:
//
// - Phase (1) begins by calling TakeStartingDeskScreenshot(), which should be
//   called before the ending desk is activated.
//   * Once the screenshot is taken, it is placed in a layer that covers
//     everything on the screen, so that desk activation can happen without
//     being seen.
//   * Delegate::OnStartingDeskScreenshotTaken() is called, and the owner of
//     this object can check that all animators of all root windows have
//     finished taking the starting desk screenshots (through checking
//     starting_desk_screenshot_taken()), upon which the actual ending desk
//     activation can take place, and phase (2) of the animation can be
//     triggered.
//
// - Phase (2) should begin after the ending desk had been activated,
//   by calling TakeEndingDeskScreenshot().
//   * Once the screenshot is taken, it is placed in a sibling layer to the
//     starting desk screenshot layer, with an offset of |kDesksSpacing| between
//     the two layers.
//   * Delegate::OnEndingDeskScreenshotTaken() will be called, upon which the
//     owner of this object can check if all ending desks screenshots on all
//     roots are taken by all animators (through checking
//     ending_desk_screenshot_taken()), so that it can start phase (3) on all of
//     them at the same time.
//
// - Phase (3) begins when StartAnimation() is called.
//   * The parent layer of both screenshot layers is animated, either:
//     - To the left (move_left_ == true); when the starting desk is on the
//       left.
//
//              <<<<<-------------------------- move left.
//                       +-----------+
//                       | Animation |
//                       |  layer    |
//                       +-----------+
//                         /        \
//              +------------+      +------------+
//              | start desk |      | end desk   |
//              | screenshot |      | screenshot |
//              |  layer     |      |  layer     |
//              +------------+      +------------+
//                    ^
//                start here
//
//       Animation layer transforms:
//       * Begin transform: Identity (since starting desk screeshot is already
//         visible).
//       * End transform: Negative translation to the left to slide out starting
//         desk, and slide in ending desk screenshots into the screen.
//
//     - Or to the right (move_left_ == false), when the starting desk is on the
//       right.
//
//          move right. -------------------------->>>>>
//                       +-----------+
//                       | Animation |
//                       |  layer    |
//                       +-----------+
//                         /        \
//              +------------+      +------------+
//              | end desk   |      | start desk |
//              | screenshot |      | screenshot |
//              |  layer     |      |  layer     |
//              +------------+      +------------+
//                                        ^
//                                    start here
//
//       Animation layer transforms:
//       * Begin transform: Negative translation to the left (to make starting
//         desk screeshot visible on the screen).
//       * End transform: Identity to make a translation to the right to slide
//         out starting desk, and slide in ending desk screenshots into the
//         screen.
//
//   * The animation always begins such that the starting desk screenshot layer
//     is the one visible on the screen, and the parent (animation layer) always
//     moves in the direction such that the ending desk screenshot becomes
//     visible on the screen.
//   * Once the animation finishes, Delegate::OnDeskSwitchAnimationFinished() is
//     triggered. The owner of this object can then check that all animators on
//     all roots have finished their animations (by checking
//     animation_finished()), upon which it can delete these animators which
//     will destroy all the screenshot layers and the real screen contents will
//     be visible again.
//
// This cooperative interaction between the animators and their owner
// (DesksController::AbstractDeskSwitchAnimation) is needed for the following
// reasons:
// 1- The new desk is only activated after all starting desk screenshots on all
//    roots have been taken and placed on top of everything (between phase (1)
//    and (2)), so that the effects of desk activation (windows hiding and
//    showing, overview exiting .. etc.) are not visible to the user.
// 2- The animation doesn't start until all ending desk screenshots on all
//    root windows are ready (between phase (2) and (3)). This is needed to
//    synchronize the animations on all displays together (otherwise the
//    animations will lag behind each other).
//
// When this animator is used to implement the remove-active-desk animation
// (which also involves switching desks; from the to-be-removed desk to another
// desk), `for_remove` is set to true in the constructor. The animation is
// slightly tweaked to do the following:
// - Instead of taking a screenshot of the starting desk, we replace it by a
//   black solid color layer, to indicate the desk is being removed.
// - The layer tree of the active-desk container is recreated, and the old
//   layers are detached and animated vertically by
//   `kRemovedDeskWindowYTranslation`.
// - That old layer tree is then translated back down by the same amount while
//   the desks screenshots are animating horizontally.
// This gives the effect that the removed desk windows are jumping from their
// desk to the target desk.
class RootWindowDeskSwitchAnimator : public ui::ImplicitAnimationObserver {
 public:
  class Delegate {
   public:
    // Called when phase (1) completes. The starting desk screenshot has been
    // taken and put on the screen. |ending_desk| is the desk that will be
    // activated after all starting desk screenshots on all roots are taken.
    virtual void OnStartingDeskScreenshotTaken(const Desk* ending_desk) = 0;

    // Called when phase (2) completes. The ending desk screenshot has been
    // taken and put on the screen.
    virtual void OnEndingDeskScreenshotTaken() = 0;

    // Called when phase (3) completes. The animation completes and the ending
    // desk screenshot is now showing on the screen.
    virtual void OnDeskSwitchAnimationFinished() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  RootWindowDeskSwitchAnimator(aura::Window* root,
                               const Desk* ending_desk,
                               Delegate* delegate,
                               bool move_left,
                               bool for_remove);

  ~RootWindowDeskSwitchAnimator() override;

  bool starting_desk_screenshot_taken() const {
    return starting_desk_screenshot_taken_;
  }
  bool ending_desk_screenshot_taken() const {
    return ending_desk_screenshot_taken_;
  }
  bool animation_finished() const { return animation_finished_; }

  // Begins phase (1) of the animation by taking a screenshot of the starting
  // desk content. Delegate::OnStartingDeskScreenshotTaken() will be called once
  // the screenshot is taken and placed on top of everything on the screen.
  void TakeStartingDeskScreenshot();

  // Begins phase (2) of the animation, after the ending desk has already
  // been activated. Delegate::OnEndingDeskScreenshotTaken() will be called once
  // the screenshot is taken.
  void TakeEndingDeskScreenshot();

  // Begins phase (3) of the animation by actually animating the screenshot
  // layers such that we have a movement from the starting desk screenshot
  // towards the ending desk screenshot.
  // Delegate::OnDeskSwitchAnimationFinished() will be called once the animation
  // finishes.
  void StartAnimation();

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

 private:
  // Completes the first phase of the animation using the given |layer| as the
  // screenshot layer of the starting desk. This layer will be parented to the
  // animation layer, which will be setup with its initial transform according
  // to |move_left|. If |for_remove_| is true, the detached old layer tree of
  // the soon-to-be-removed-desk's windows will be translated up vertically to
  // simulate a jump from the removed desk to the target desk.
  // |Delegate::OnStartingDeskScreenshotTaken()| will be called at the end.
  void CompleteAnimationPhase1WithLayer(std::unique_ptr<ui::Layer> layer);

  void OnStartingDeskScreenshotTaken(
      std::unique_ptr<viz::CopyOutputResult> copy_result);
  void OnEndingDeskScreenshotTaken(
      std::unique_ptr<viz::CopyOutputResult> copy_result);

  // The root window that this animator is associated with.
  aura::Window* const root_window_;

  // The active desk at the start of the animation.
  const Desk* const starting_desk_;

  // The desk to activate and animate to with this animator.
  const Desk* const ending_desk_;

  Delegate* const delegate_;

  // The owner of the layer tree of the old detached layers of the removed
  // desk's windows. This is only valid if |for_remove_| is true. This layer
  // tree is animated to simulate that the windows are jumping from the removed
  // desk to the target desk.
  std::unique_ptr<ui::LayerTreeOwner> old_windows_layer_tree_owner_;

  // The owner of the layer tree that contains the parent "animation layer" and
  // both its child starting and ending desks "screenshot layers".
  std::unique_ptr<ui::LayerTreeOwner> animation_layer_owner_;

  // The amount by which the animation layer will be translated horizontally
  // either startingly or at the end of the animation, depending on the value of
  // |move_left_|.
  const int x_translation_offset_;

  // Number of retires for taking the starting and ending screenshots, if we
  // get an empty result.
  int starting_desk_screenshot_retries_ = 0;
  int ending_desk_screenshot_retries_ = 0;

  // True when the animation layer should be translated towards the left, which
  // means the starting desk is on the left of the ending desk.
  const bool move_left_;

  // True if this animator is handling the remove-active-desk animation.
  const bool for_remove_;

  // True when phase (1) finishes.
  bool starting_desk_screenshot_taken_ = false;

  // True when phase (2) finishes.
  bool ending_desk_screenshot_taken_ = false;

  // True when phase (3) finishes.
  bool animation_finished_ = false;

  base::WeakPtrFactory<RootWindowDeskSwitchAnimator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RootWindowDeskSwitchAnimator);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_ROOT_WINDOW_DESK_SWITCH_ANIMATOR_H_
