// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_COUNTER_EXPAND_BUTTON_H_
#define ASH_STYLE_COUNTER_EXPAND_BUTTON_H_

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

// Customized expand button for ash that contains a number counter that
// represents the number of children views and a chevron icon. Used for grouped
// bubble that are available for expanding and collapsing children views.
//  /------\
// |  5 \/  |
//  \------/

class ASH_EXPORT CounterExpandButton : public views::Button {
  METADATA_HEADER(CounterExpandButton, views::Button)
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

  CounterExpandButton();
  CounterExpandButton(const CounterExpandButton&) = delete;
  CounterExpandButton& operator=(const CounterExpandButton&) = delete;
  ~CounterExpandButton() override;

  void set_label_fading_out(bool label_fading_out) {
    label_fading_out_ = label_fading_out;
  }

  void set_previous_bounds(gfx::Rect previous_bounds) {
    previous_bounds_ = previous_bounds;
  }

  views::Label* label() { return label_; }

  // Changes the expanded state. The icon will change.
  void SetExpanded(bool expanded);
  bool expanded() const { return expanded_; }

  // Whether the label displaying the number of children in a grouped bubble
  // needs to be displayed.
  bool ShouldShowLabel() const;

  // Updates the count of total grouped child views in the parent container and
  // updates the text for the label accordingly.
  void UpdateCounter(int count);

  // Performs expand/collapse animation. This includes bounds change and fade
  // in/out `label_`.
  void AnimateExpandCollapse();

  // Returns the animation smoothness histogram name used for the animation type
  // `type`.
  virtual const std::string GetAnimationHistogramName(AnimationType type);

  // views::Button:
  void OnThemeChanged() override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  size_t counter_for_test() const { return counter_; }

 protected:
  views::ImageView* image() { return image_; }

  // Bounds change animation happens during expand/collapse and converting from
  // single to group animation.
  void AnimateBoundsChange(int duration_in_ms,
                           gfx::Tween::Type tween_type,
                           const std::string& animation_histogram_name);

  // Generates the icons used for chevron in the expanded and collapsed state.
  void UpdateIcons();

  // Updates the tooltip of the button.
  void UpdateTooltip();

  // Returns the tooltip text on the button in expanded/collapsed state.
  virtual std::u16string GetExpandedStateTooltipText() const;
  virtual std::u16string GetCollapsedStateTooltipText() const;

 private:
  void UpdateBackgroundColor();

  // Owned by views hierarchy.
  raw_ptr<views::Label> label_;
  raw_ptr<views::ImageView> image_;

  // Cached icons used to display the chevron in the button.
  gfx::ImageSkia expanded_image_;
  gfx::ImageSkia collapsed_image_;

  // Used in layer bounds animation.
  gfx::Rect previous_bounds_;

  // The number shown on `label_`. This is used to show the total number of
  // grouped child views in this button's parent view.
  size_t counter_ = 0;

  // The expand state of the button.
  bool expanded_ = false;

  // True if `label_` is in its fade out animation.
  bool label_fading_out_ = false;

  base::WeakPtrFactory<CounterExpandButton> weak_factory_{this};
};

BEGIN_VIEW_BUILDER(/*no export*/, CounterExpandButton, views::Button)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::CounterExpandButton)

#endif  // ASH_STYLE_COUNTER_EXPAND_BUTTON_H_
