// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_ASH_NOTIFICATION_EXPAND_BUTTON_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_ASH_NOTIFICATION_EXPAND_BUTTON_H_

#include <string>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/metadata/view_factory.h"

namespace views {
class Label;
class ImageView;
}  // namespace views

namespace ash {

// Customized expand button for ash notification view. Used for grouped as
// well as singular notifications.
class ASH_EXPORT AshNotificationExpandButton : public views::Button {
  METADATA_HEADER(AshNotificationExpandButton, views::Button)

 public:
  // The type of animation. This is used for animation smoothness histograms.
  enum class AnimationType {
    // `label_` fading in animation.
    kFadeInLabel,
    // `label_` fading out animation.
    kFadeOutLabel,
    // Animating bounds change for the whole button layer.
    kBoundsChange,
  };

  AshNotificationExpandButton();
  AshNotificationExpandButton(const AshNotificationExpandButton&) = delete;
  AshNotificationExpandButton& operator=(const AshNotificationExpandButton&) =
      delete;
  ~AshNotificationExpandButton() override;

  // Change the expanded state. The icon will change.
  void SetExpanded(bool expanded);

  // Whether the label displaying the number of notifications in a grouped
  // notification needs to be displayed.
  bool ShouldShowLabel() const;

  // Update the count of total grouped child views in the parent container and
  // update the text for the label accordingly.
  void UpdateCounter(int count);

  // Generate the icons used for chevron in the expanded and collapsed state.
  void UpdateIcons();

  // Perform expand/collapse animation. This include bounds change and fade
  // in/out `label_`.
  void AnimateExpandCollapse();

  // Perform converting from single to group notification
  // animation. This include bounds change and fade in/out `label_`.
  void AnimateSingleToGroupNotification();

  // Returns the animation smoothness histogram name used for the animation type
  // `type`.
  const std::string GetAnimationHistogramName(AnimationType type);

  // views::Button:
  void OnThemeChanged() override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  void SetNotificationTitleForButtonTooltip(
      const std::u16string& notification_title);

  void set_label_fading_out(bool label_fading_out) {
    label_fading_out_ = label_fading_out;
  }

  void set_previous_bounds(gfx::Rect previous_bounds) {
    previous_bounds_ = previous_bounds;
  }

  views::Label* label() const { return label_; }

 private:
  // Bounds change animation happens during expand/collapse and converting from
  // single to group animation.
  void AnimateBoundsChange(int duration_in_ms,
                           gfx::Tween::Type tween_type,
                           const std::string& animation_histogram_name);

  void UpdateBackgroundColor();

  void UpdateTooltip();

  // Owned by views hierarchy.
  raw_ptr<views::Label> label_;
  raw_ptr<views::ImageView> image_;

  // Cached icons used to display the chevron in the button.
  gfx::ImageSkia expanded_image_;
  gfx::ImageSkia collapsed_image_;

  // Used in layer bounds animation.
  gfx::Rect previous_bounds_;

  // Total number of grouped child notifications in this button's parent view.
  int total_grouped_notifications_ = 0;

  // The expand state of the button.
  bool expanded_ = false;

  // True if `label_` is in its fade out animation.
  bool label_fading_out_ = false;

  // Cache of the notification title. Used this to display in the button
  // tooltip.
  std::u16string notification_title_;

  base::WeakPtrFactory<AshNotificationExpandButton> weak_factory_{this};
};
BEGIN_VIEW_BUILDER(/*no export*/, AshNotificationExpandButton, views::Button)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::AshNotificationExpandButton)

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_ASH_NOTIFICATION_EXPAND_BUTTON_H_
