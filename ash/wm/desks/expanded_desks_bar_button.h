// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_EXPANDED_DESKS_BAR_BUTTON_H_
#define ASH_WM_DESKS_EXPANDED_DESKS_BAR_BUTTON_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/view.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class Label;
}  // namespace views

namespace ash {

class DeskButtonBase;
class DesksBarView;
class InnerExpandedDesksBarButton;

// A desk button view in the expanded desks bar. It includes the
// InnerExpandedDesksBarButton and a name label below, which has the same style
// as a DeskMiniView, but the name label is not changeable and not focusable.
class ASH_EXPORT ExpandedDesksBarButton : public views::View {
 public:
  METADATA_HEADER(ExpandedDesksBarButton);

  ExpandedDesksBarButton(DesksBarView* bar_view,
                         const gfx::VectorIcon* button_icon,
                         const std::u16string& button_label,
                         bool initially_enabled,
                         base::RepeatingClosure callback);
  ExpandedDesksBarButton(const ExpandedDesksBarButton&) = delete;
  ExpandedDesksBarButton& operator=(const ExpandedDesksBarButton&) = delete;
  ~ExpandedDesksBarButton() override = default;

  const gfx::VectorIcon* button_icon() const { return button_icon_; }

  void set_active(bool active) { active_ = active; }

  DeskButtonBase* GetInnerButton();

  // Updates `inner_button_`'s state on current desks state.
  void SetButtonState(bool enabled);

  // Updates the `label_`'s color on DesksController::CanCreateDesks.
  void UpdateLabelColor(bool enabled);

  bool IsPointOnButton(const gfx::Point& screen_location) const;

  // Updates the border color of the ExpandedDesksBarButton based on
  // the dragged item's position and `active_`.
  void UpdateFocusColor() const;

  // views::View:
  void Layout() override;
  void OnThemeChanged() override;

  absl::optional<ui::ColorId> GetFocusColorIdForTesting();

 private:
  DesksBarView* const bar_view_;  // Not owned.
  const gfx::VectorIcon* const button_icon_;
  const std::u16string button_label_;
  InnerExpandedDesksBarButton* inner_button_;
  views::Label* label_;

  // If `active_` is true, then the border of `inner_button_` will be
  // highlighted if it's not already focused.
  bool active_ = false;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_EXPANDED_DESKS_BAR_BUTTON_H_
