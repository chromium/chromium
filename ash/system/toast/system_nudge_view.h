// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_SYSTEM_NUDGE_VIEW_H_
#define ASH_SYSTEM_TOAST_SYSTEM_NUDGE_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class ImageButton;
class Widget;
}  // namespace views

namespace ash {

struct AnchoredNudgeData;
class SystemShadow;

// The System Nudge view. (go/cros-educationalnudge-spec)
// This view supports different configurations depending on the provided
// nudge data parameters. It will always have a body text, and may have a
// leading image view, a title text, and up to two buttons placed on the bottom.
class ASH_EXPORT SystemNudgeView : public views::FlexLayoutView,
                                   public views::WidgetObserver {
 public:
  METADATA_HEADER(SystemNudgeView);

  SystemNudgeView(AnchoredNudgeData& nudge_data);
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
  // Owned by the views hierarchy.
  raw_ptr<views::ImageButton> close_button_ = nullptr;

  std::unique_ptr<SystemShadow> shadow_;

  // Handles mouse enter/exit events to either show or hide `close_button_`.
  void HandleOnMouseHovered(const bool mouse_entered);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_SYSTEM_NUDGE_VIEW_H_
