// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_CROS_NEXT_DESK_BUTTON_H_
#define ASH_WM_DESKS_CROS_NEXT_DESK_BUTTON_H_

#include "ash/wm/desks/cros_next_desk_button_base.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace gfx {
struct VectorIcon;
}

namespace ash {

class DesksBarView;

// A button in zero state bar showing the name of the desk. Zero state is the
// state of the desks bar when there's only a single desk available, in which
// case the bar is shown in a minimized state. Clicking the button will switch
// to the expanded desks bar and focus on the single desk's name view.
// TODO(conniekxu): Remove `ZeroStateDefaultDeskButton`, replace it with this
// class, and rename this class by removing the prefix CrOSNext.
class CrOSNextDefaultDeskButton : public CrOSNextDeskButtonBase {
 public:
  METADATA_HEADER(CrOSNextDefaultDeskButton);

  explicit CrOSNextDefaultDeskButton(DesksBarView* bar_view);
  CrOSNextDefaultDeskButton(const CrOSNextDefaultDeskButton&) = delete;
  CrOSNextDefaultDeskButton& operator=(const CrOSNextDefaultDeskButton&) =
      delete;
  ~CrOSNextDefaultDeskButton() override = default;

  void UpdateLabelText();

  // CrOSNextDeskButtonBase:
  gfx::Size CalculatePreferredSize() const override;

 private:
  void OnButtonPressed();

  DesksBarView* const bar_view_;
};

// A button view in the desks bar with an icon. The button have three different
// states, and the three states are interchangeable.
// TODO(conniekxu): Remove `ZeroStateIconButton` and `ExpandedDesksBarButton`,
// replace them with this class, and rename this class by removing the prefix
// CrOSNext.
class CrOSNextDeskIconButton : public CrOSNextDeskButtonBase {
 public:
  METADATA_HEADER(CrOSNextDeskIconButton);

  // The enum class defines three states for the button. The button at different
  // states has different sizes. Any state could be transformed into another
  // state under certain conditions.
  enum class State {
    // The state of the button when the DesksBarView is in zero state.
    kZero,
    // The state of the button when the DesksBarView is in expanded state.
    kExpanded,
    // The state of when a window is dragged over the new desk button and held
    // for 500 milliseconds, we can create a new desk. The new desk button state
    // will change to reflect that.
    kDragAndDrop,
  };

  CrOSNextDeskIconButton(DesksBarView* bar_view,
                         const gfx::VectorIcon* button_icon,
                         const std::u16string& text,
                         ui::ColorId icon_color_id,
                         ui::ColorId background_color_id,
                         base::RepeatingClosure callback);
  CrOSNextDeskIconButton(const CrOSNextDeskIconButton&) = delete;
  CrOSNextDeskIconButton& operator=(const CrOSNextDeskIconButton&) = delete;
  ~CrOSNextDeskIconButton() override;

  bool IsPointOnButton(const gfx::Point& screen_location) const;

  // CrOSNextDeskButtonBase:
  gfx::Size CalculatePreferredSize() const override;
  // Updates the focus ring based on the dragged item's position and `active_`.
  void UpdateFocusState() override;

 private:
  DesksBarView* const bar_view_;

  State state_;

  // If `active_` is true, then focus ring will be painted with color id
  // `kColorAshCurrentDeskColor` even if it's not already focused.
  bool paint_as_active_ = false;

  absl::optional<ui::ColorId> focus_color_id_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_CROS_NEXT_DESK_BUTTON_H_
