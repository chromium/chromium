// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_EXPAND_BUTTON_H_
#define ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_EXPAND_BUTTON_H_

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
class AshNotificationExpandButton : public views::Button {
 public:
  METADATA_HEADER(AshNotificationExpandButton);
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

  // Update the count of total grouped notifications in the parent view and
  // update the text for the label accordingly.
  void UpdateGroupedNotificationsCount(int count);

  // Generate the icons used for chevron in the expanded and collapsed state.
  void UpdateIcons();

  // Perform expand/collapse and converting from single to group notification
  // animation. Both of these include bounds change and fade in/out `label_`.
  void AnimateExpandCollapse();
  void AnimateSingleToGroupNotification();

  // views::Button:
  void OnThemeChanged() override;
  gfx::Size CalculatePreferredSize() const override;

  void SetExpandCollapseEnabled(bool enabled);

  void set_label_fading_out(bool label_fading_out) {
    label_fading_out_ = label_fading_out;
  }

  void set_previous_bounds(gfx::Rect previous_bounds) {
    previous_bounds_ = previous_bounds;
  }

  views::Label* label_for_test() { return label_; }

 private:
  // Bounds change animation happens during expand/collapse and converting from
  // single to group animation.
  void AnimateBoundsChange(int duration_in_ms,
                           gfx::Tween::Type tween_type,
                           const std::string& animation_histogram_name);

  void UpdateBackgroundColor();

  void UpdateTooltip();

  // Owned by views hierarchy.
  raw_ptr<views::Label, ExperimentalAsh> label_;
  raw_ptr<views::ImageView, ExperimentalAsh> image_;

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

  bool disable_expand_collapse_ = false;

  base::WeakPtrFactory<AshNotificationExpandButton> weak_factory_{this};
};
BEGIN_VIEW_BUILDER(/*no export*/, AshNotificationExpandButton, views::Button)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::AshNotificationExpandButton)

#endif  // ASH_SYSTEM_MESSAGE_CENTER_ASH_NOTIFICATION_EXPAND_BUTTON_H_
