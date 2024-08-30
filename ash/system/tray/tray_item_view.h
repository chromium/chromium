// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_ITEM_VIEW_H_
#define ASH_SYSTEM_TRAY_TRAY_ITEM_VIEW_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace gfx {
class SlideAnimation;
}

namespace views {
class ImageView;
}

namespace ash {
class Shelf;

// Label view which can be given a different data from the visible label.
// IME icons like "US" (US keyboard) or "あ(Google Japanese Input)" are
// rendered as a label, but reading such text literally will not always be
// understandable.
class ASH_EXPORT IconizedLabel : public views::Label {
  METADATA_HEADER(IconizedLabel, views::Label)

 public:
  void SetCustomAccessibleName(const std::u16string& name);

  std::u16string GetAccessibleNameString() const {
    return custom_accessible_name_;
  }

  // views::View:
  void AdjustAccessibleName(std::u16string& new_name,
                            ax::mojom::NameFrom& name_from) override;

 private:
  // The accessible role depends on the `custom_accessible_name_` when it is
  // non-empty.
  void UpdateAccessibleRole();

  std::u16string custom_accessible_name_;

  base::CallbackListSubscription text_context_changed_callback_ =
      AddTextContextChangedCallback(
          base::BindRepeating(&IconizedLabel::UpdateAccessibleRole,
                              base::Unretained(this)));
};

// Base-class for items in the tray. It makes sure the widget is updated
// correctly when the visibility/size of the tray item changes. It also adds
// animation when showing/hiding the item in the tray.
//
// A derived class can implement its own custom visibility animations by
// overriding `PerformVisibilityAnimation()`. If the QS revamp is enabled, then
// it is also important to override `ImmediatelyUpdateVisibility()`, which will
// be called in certain scenarios like at the end of the
// `NotificationCenterTray`'s hide animation or when the
// `NotificationCenterTray`'s hide animation is interrupted by its show
// animation. Also note that `IsAnimationEnabled()` should be checked whenever
// attempting to perform the custom animations, as there are times when a
// `TrayItemView`'s visibility should change but that change should not be
// animated (for instance, when the `NotificationCenterTray` is hidden).
class ASH_EXPORT TrayItemView : public views::View,
                                public views::AnimationDelegateViews {
  METADATA_HEADER(TrayItemView, views::View)

 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when this tray item's visibility is going to change but has not
    // yet changed. `target_visibility` is the visibility the tray item is going
    // to have.
    virtual void OnTrayItemVisibilityAboutToChange(bool target_visibility) = 0;
  };

  explicit TrayItemView(Shelf* shelf);

  TrayItemView(const TrayItemView&) = delete;
  TrayItemView& operator=(const TrayItemView&) = delete;

  ~TrayItemView() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Convenience function for creating a child Label or ImageView.
  // Only one of the two should be called.
  void CreateLabel();
  void CreateImageView();

  // Methods for destroying a child label or ImageView, which a user of
  // `TrayItemView` should do if they know a child view is no longer visible and
  // is expected to remain as such for longer than ~0.1 seconds.
  void DestroyLabel();
  void DestroyImageView();

  // Called when locale change is detected (which should not happen after the
  // user session starts). It should reload any strings the view is using.
  virtual void HandleLocaleChange() = 0;

  // For Material Next: Updates the color of `label_` or `image_view_` based on
  // whether the view is active or not.
  virtual void UpdateLabelOrImageViewColor(bool active);

  // Temporarily disables the use of animation on visibility changes. Animation
  // will be disabled until the returned scoped closure is run.
  [[nodiscard]] base::ScopedClosureRunner DisableAnimation();

  // Sets `animation_idle_closure_`. Used by tests only.
  void SetAnimationIdleClosureForTest(base::OnceClosure closure);

  // Returns true if a visibility animation is currently running, false
  // otherwise.
  bool IsAnimating();

  // Updates this `TrayItemView`'s visibility according to `target_visible_`
  // without animating. Only called when the QS revamp is enabled. This is
  // called in certain scenarios like at the end of the
  // `NotificationCenterTray`'s hide animation or when the
  // `NotificationCenterTray`'s hide animation is interrupted by its show
  // animation.
  virtual void ImmediatelyUpdateVisibility();

  // Returns the target visibility. For testing only.
  bool target_visible_for_testing() const { return target_visible_; }

  // Returns this `TrayItemView`'s animation. For testing only.
  gfx::SlideAnimation* animation_for_testing() const {
    return animation_.get();
  }

  IconizedLabel* label() const { return label_; }
  views::ImageView* image_view() const { return image_view_; }

  bool is_active() { return is_active_; }

  // views::View.
  void SetVisible(bool visible) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  void set_use_scale_in_animation(bool use_scale_in_animation) {
    use_scale_in_animation_ = use_scale_in_animation;
  }

 protected:
  // Returns whether the shelf is horizontal.
  bool IsHorizontalAlignment() const;

  // Perform visibility animation for this view. This function can be overridden
  // so that the visibility animation can be customized. If the QS revamp is
  // enabled then `ImmediatelyUpdateVisibility()` should also be overridden.
  virtual void PerformVisibilityAnimation(bool visible);

  // Checks if we should use animation on visibility changes.
  bool ShouldVisibilityChangeBeAnimated() const {
    return disable_animation_count_ == 0u;
  }

  // views::AnimationDelegateViews.
  void AnimationEnded(const gfx::Animation* animation) override;

  bool target_visible() { return target_visible_; }

  const Shelf* shelf() { return shelf_; }

 private:
  // views::View.
  void ChildPreferredSizeChanged(View* child) override;

  // views::AnimationDelegateViews.
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // Return true if the animation is in resize animation stage, which
  // happens before item animating in and after item animating out.
  bool InResizeAnimation(double animation_value) const;

  // Converts the overall visibility animation progress to the progress for the
  // animation stage that resize the tray container.
  double GetResizeProgressFromAnimationProgress(double animation_value) const;

  // Converts the overall visibility animation progress to the progress for the
  // animation stage that fades and scales the tray item.
  double GetItemScaleProgressFromAnimationProgress(
      double animation_value) const;

  const raw_ptr<Shelf, DanglingUntriaged> shelf_;

  // When showing the item in tray, the animation is executed with 2 stages:
  // 1. Resize: The size reserved for tray item view gradually increases.
  // 2. Item animation: After size has changed to the target size, the actual
  //    tray item starts appearing.
  // The steps reverse when hiding the item (the item disappears, then width
  // change animation).
  std::unique_ptr<gfx::SlideAnimation> animation_;

  // The target visibility for the item when all the animation is done.
  // Initialized to true because View visibility defaults to true during
  // construction.
  bool target_visible_ = true;

  // Use scale in animating in the item to the tray.
  bool use_scale_in_animation_ = true;

  // For Material Next: if this view is active or not in `UnifiedSystemTray`.
  // This is used for coloring and is set in `UpdateLabelOrImageViewColor()`.
  // Note: the value is only accurate when the Jelly flag is set.
  bool is_active_ = false;

  // Only one of |label_| and |image_view_| should be non-null.
  raw_ptr<IconizedLabel, DanglingUntriaged> label_ = nullptr;
  raw_ptr<views::ImageView, DanglingUntriaged> image_view_ = nullptr;

  // Measures animation smoothness metrics for "show" animation.
  std::optional<ui::ThroughputTracker> show_throughput_tracker_;

  // Measures animation smoothness metrics for "hide" animation.
  std::optional<ui::ThroughputTracker> hide_throughput_tracker_;

  // Number of active requests to disable animation.
  size_t disable_animation_count_ = 0u;

  // A closure called when the visibility animation finishes. Used for tests
  // only.
  base::OnceClosure animation_idle_closure_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<TrayItemView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_ITEM_VIEW_H_
