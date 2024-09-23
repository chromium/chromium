// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_SYSTEM_NUDGE_VIEW_H_
#define ASH_SYSTEM_TOAST_SYSTEM_NUDGE_VIEW_H_

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class ImageButton;
class ViewTracker;
class Widget;
}  // namespace views

namespace gfx {
class RoundedCornersF;
}  // namespace gfx

namespace ash {

struct AnchoredNudgeData;
class SystemShadow;

// The System Nudge view. (go/cros-educationalnudge-spec)
// This view supports different configurations depending on the provided
// nudge data parameters. It will always have a body text, and may have a
// leading image view, a title text, and up to two buttons placed on the bottom.
class ASH_EXPORT SystemNudgeView : public views::FlexLayoutView,
                                   public views::WidgetObserver {
  METADATA_HEADER(SystemNudgeView, views::FlexLayoutView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kBubbleIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPrimaryButtonIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSecondaryButtonIdForTesting);

  explicit SystemNudgeView(const AnchoredNudgeData& nudge_data,
                           base::RepeatingCallback<void(bool)>
                               hover_focus_callback = base::DoNothing());
  SystemNudgeView(const SystemNudgeView&) = delete;
  SystemNudgeView& operator=(const SystemNudgeView&) = delete;
  ~SystemNudgeView() override;

  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  class FocusableChildrenObserver;

  // Handles observed child focus state changes.
  void HandleOnChildFocusStateChanged(bool focus_entered);

  // Handles mouse enter/exit events to either show or hide `close_button_`.
  void HandleOnMouseHovered(bool mouse_entered);

  // Returns true if the nudge is mouse hovered or a child is focused.
  bool IsHoveredOrChildHasFocus();

  // Sets the corner radius for the nudge view, shadow and highlight border.
  void SetNudgeRoundedCornerRadius(const gfx::RoundedCornersF& rounded_corners);

  // Owned by the views hierarchy.
  raw_ptr<views::ImageButton> close_button_ = nullptr;

  std::unique_ptr<views::ViewTracker> anchor_view_tracker_;

  std::unique_ptr<SystemShadow> shadow_;

  // Used to determine if the nudge will draw a pointy corner.
  const bool is_corner_anchored_;

  // Observes focus state changes in the nudge's focusable children.
  std::unique_ptr<FocusableChildrenObserver> focusable_children_observer_;

  // Custom callback triggered whenever the nudge's hover state changes. It may
  // be empty since it's an optional parameter set in `nudge_data`.
  const base::RepeatingCallback<void(/*is_hovered=*/bool)>
      hover_changed_callback_;

  // Callback triggered whenever the hover or focus state changes.
  const base::RepeatingCallback<void(/*is_hovered_or_has_focus=*/bool)>
      hover_or_focus_changed_callback_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_SYSTEM_NUDGE_VIEW_H_
