// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_BACKGROUND_ANIMATOR_H_
#define ASH_SHELF_SHELF_BACKGROUND_ANIMATOR_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/shelf/shelf_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/animation_delegate.h"

namespace gfx {
class SlideAnimation;
}  // namespace gfx

namespace ash {

enum class AnimationChangeType;
class Shelf;
class ShelfBackgroundAnimatorObserver;
class ShelfBackgroundAnimatorTestApi;
class WallpaperControllerImpl;

// Central controller for the Shelf and Dock opacity animations.
//
// The ShelfBackgroundAnimator is capable of observing a Shelf instance for
// background type changes or clients can call PaintBackground() directly.
//
// The Shelf uses 2 surfaces for the animations:
//
//  Material Design:
//    1. Shelf button backgrounds
//    2. Overlay for the ShelfBackgroundType::kMaximized state.
class ASH_EXPORT ShelfBackgroundAnimator : public ShelfObserver,
                                           public gfx::AnimationDelegate,
                                           public WallpaperControllerObserver {
 public:
  // The maximum alpha value that can be used.
  static const int kMaxAlpha = SK_AlphaOPAQUE;

  ShelfBackgroundAnimator(Shelf* shelf,
                          WallpaperControllerImpl* wallpaper_controller);

  ShelfBackgroundAnimator(const ShelfBackgroundAnimator&) = delete;
  ShelfBackgroundAnimator& operator=(const ShelfBackgroundAnimator&) = delete;

  ~ShelfBackgroundAnimator() override;

  // Initializes this with the given |background_type|. This will observe the
  // |shelf| for background type changes and the |wallpaper_controller| for
  // wallpaper changes if not null.
  void Init(ShelfBackgroundType background_type);

  ShelfBackgroundType target_background_type() const {
    return target_background_type_;
  }

  // Initializes |observer| with current values using the Initialize() function.
  void AddObserver(ShelfBackgroundAnimatorObserver* observer);
  void RemoveObserver(ShelfBackgroundAnimatorObserver* observer);

  // Updates |observer| with current values.
  void NotifyObserver(ShelfBackgroundAnimatorObserver* observer);

  // Conditionally animates the background to the specified |background_type|
  // and notifies observers of the new background parameters (e.g. color).
  // If |change_type| is BACKGROUND_CHANGE_IMMEDIATE then the
  // observers will only receive one notification with the final background
  // state, otherwise the observers will be notified multiple times in order to
  // animate the changes to the backgrounds.
  //
  // NOTE: If a second request to paint the same |background_type| using the
  // BACKGROUND_CHANGE_ANIMATE change type is received it will be ignored and
  // observers will NOT be notified.
  void PaintBackground(ShelfBackgroundType background_type,
                       AnimationChangeType change_type);

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // Gets the color corresponding with |background_type|.
  SkColor GetBackgroundColor(ShelfBackgroundType background_type) const;

  // Drives the current animation to the end.
  void CompleteAnimationForTesting();

 protected:
  // ShelfObserver:
  void OnBackgroundTypeChanged(ShelfBackgroundType background_type,
                               AnimationChangeType change_type) override;

  // WallpaperControllerObserver:
  void OnWallpaperColorsChanged() override;

 private:
  friend class ShelfBackgroundAnimatorTestApi;

  // Track the values related to a single animation (e.g. Shelf background,
  // shelf item background)
  class AnimationValues {
   public:
    AnimationValues();

    AnimationValues(const AnimationValues&) = delete;
    AnimationValues& operator=(const AnimationValues&) = delete;

    ~AnimationValues();

    SkColor current_color() const { return current_color_; }
    SkColor target_color() const { return target_color_; }

    // Updates the |current_color_| based on |t| value between 0 and 1.
    void UpdateCurrentValues(double t);

    // Set the target color and assign the current color to the initial color.
    void SetTargetValues(SkColor target_color);

    // Returns true if the initial values of |this| equal the target values of
    // |other|.
    bool InitialValuesEqualTargetValuesOf(const AnimationValues& other) const;

   private:
    SkColor initial_color_ = SK_ColorTRANSPARENT;
    SkColor current_color_ = SK_ColorTRANSPARENT;
    SkColor target_color_ = SK_ColorTRANSPARENT;
  };

  // Helper function used by PaintBackground() to animate the background.
  void AnimateBackground(ShelfBackgroundType background_type,
                         AnimationChangeType change_type);

  // Returns true if the current |animator_| should be re-used to animate to the
  // given |background_type|.  This allows for pre-empted animations to take the
  // same amount of time to reverse to the |previous_background_type_|.
  bool CanReuseAnimator(ShelfBackgroundType background_type) const;

  // Creates a new |animator_| configured with the correct duration. If the
  // |animator_| is currently animating it will be stopped.
  void CreateAnimator(ShelfBackgroundType background_type);

  // Stops the animator owned by this.
  void StopAnimator();

  // Updates the |shelf_background_values_| and |shelf_item_background_values_|.
  void SetTargetValues(ShelfBackgroundType background_type);

  // Sets the target values for |shelf_background_values| and
  // |item_background_values| according to |background_type|.
  void GetTargetValues(ShelfBackgroundType background_type,
                       AnimationValues* shelf_background_values) const;

  // Updates the animation values corresponding to the |t| value between 0 and
  // 1.
  void SetAnimationValues(double t);

  // Called when observers need to be notified.
  void NotifyObservers();

  // The shelf to observe for changes to the shelf background type, can be null.
  raw_ptr<Shelf> shelf_;

  // The wallpaper controller to observe for changes and to extract colors from.
  raw_ptr<WallpaperControllerImpl> wallpaper_controller_;

  // The background type that this is animating towards or has reached.
  ShelfBackgroundType target_background_type_ = ShelfBackgroundType::kDefaultBg;

  // The last background type this is animating away from.
  ShelfBackgroundType previous_background_type_ =
      ShelfBackgroundType::kMaximized;

  // Drives the animation.
  std::unique_ptr<gfx::SlideAnimation> animator_;

  // Tracks the shelf background animation values.
  AnimationValues shelf_background_values_;

  // Tracks the item background animation values.
  AnimationValues item_background_values_;

  base::ObserverList<ShelfBackgroundAnimatorObserver>::Unchecked observers_;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_BACKGROUND_ANIMATOR_H_
