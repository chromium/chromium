// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_BASE_BUBBLE_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_BASE_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/login/ui/login_button.h"
#include "ash/style/system_shadow.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

class LoginBubbleHandler;

// Base bubble view for login screen bubbles.
class ASH_EXPORT LoginBaseBubbleView : public views::View,
                                       public ui::LayerAnimationObserver {
  METADATA_HEADER(LoginBaseBubbleView, views::View)

 public:
  enum class PositioningStrategy {
    // Try to show the bubble after the anchor (on the right side in LTR), if
    // there is no space show before.
    kTryAfterThenBefore,
    // Try to show the bubble before the anchor (on the left side in LTR), if
    // there is no space show after.
    kTryBeforeThenAfter,
    // Show the bubble above the anchor.
    kShowAbove,
    // Show the bubble on the bottom left of the anchor.
    kShowBelow,
  };

  // Without specifying a parent_window, the bubble will default to being in the
  // same container as anchor_view.
  explicit LoginBaseBubbleView(base::WeakPtr<views::View> anchor_view);
  explicit LoginBaseBubbleView(base::WeakPtr<views::View> anchor_view,
                               gfx::NativeView parent_window);
  ~LoginBaseBubbleView() override;
  LoginBaseBubbleView(const LoginBaseBubbleView&) = delete;
  LoginBaseBubbleView& operator=(const LoginBaseBubbleView&) = delete;

  void Show();
  void Hide();

  // Returns the button responsible for opening this bubble.
  virtual LoginButton* GetBubbleOpener() const;

  // Returns whether or not this bubble should show persistently.
  bool is_persistent() const { return is_persistent_; }
  // Change the persistence of the bubble.
  void set_persistent(bool is_persistent) { is_persistent_ = is_persistent; }

  void SetAnchorView(base::WeakPtr<views::View> anchor_view);
  // Returns the anchor view. May be `nullptr`.
  views::View* GetAnchorView() const;

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;
  void OnBlur() override;

  void set_positioning_strategy(PositioningStrategy positioning_strategy) {
    positioning_strategy_ = positioning_strategy;
  }
  void SetPadding(int horizontal_padding, int vertical_padding);

 protected:
  // Return area where bubble could be shown in.
  gfx::Rect GetBoundsAvailableToShowBubble() const;

  void set_notify_alert_on_show(bool notify_alert_on_show) {
    notify_a11y_alert_on_show_ = notify_alert_on_show;
  }

 private:
  // Create a layer for this view if doesn't exist.
  void EnsureLayer();

  // Return bounds of the anchors root view. This bounds excludes virtual
  // keyboard.
  gfx::Rect GetRootViewBounds() const;
  // Return bounds of working area. This bounds excludes shelf.
  gfx::Rect GetWorkArea() const;
  void ScheduleAnimation(bool visible);

  // Determine the position of the bubble prior to showing.
  virtual gfx::Point CalculatePosition();

  base::WeakPtr<views::View> anchor_view_;

  std::unique_ptr<LoginBubbleHandler> bubble_handler_;

  // The dialog shadow.
  std::unique_ptr<SystemShadow> shadow_;

  bool is_persistent_ = false;

  // Positioning strategy of the bubble.
  PositioningStrategy positioning_strategy_ = PositioningStrategy::kShowBelow;
  int horizontal_padding_ = 0;
  int vertical_padding_ = 0;

  // Whether or not to read an alert when the bubble is shown.
  bool notify_a11y_alert_on_show_ = true;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_BASE_BUBBLE_VIEW_H_
