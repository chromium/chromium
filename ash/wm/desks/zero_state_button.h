// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_ZERO_STATE_BUTTON_H_
#define ASH_WM_DESKS_ZERO_STATE_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/wm/desks/desk_button_base.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace gfx {
struct VectorIcon;
}

namespace ash {

class DeskBarViewBase;

// A button in zero state bar showing "Desk 1". Zero state is the state of the
// desks bar when there's only a single desk available, in which case the bar is
// shown in a minimized state. Clicking the button will switch to the expanded
// desks bar and focus on the single desk's name view. The expanded bar will
// include the single desk and the ExpandedDesksBarButton.
class ASH_EXPORT ZeroStateDefaultDeskButton : public DeskButtonBase {
 public:
  METADATA_HEADER(ZeroStateDefaultDeskButton);

  explicit ZeroStateDefaultDeskButton(DeskBarViewBase* bar_view);
  ZeroStateDefaultDeskButton(const ZeroStateDefaultDeskButton&) = delete;
  ZeroStateDefaultDeskButton& operator=(const ZeroStateDefaultDeskButton&) =
      delete;
  ~ZeroStateDefaultDeskButton() override = default;

  // DeskButtonBase:
  void OnThemeChanged() override;
  gfx::Size CalculatePreferredSize() const override;
  void UpdateLabelText() override;

 private:
  void OnButtonPressed();
};

// A button in the zero state bar with an icon. Zero state is the state of the
// desks bar when there's only a single desk available, in which case the bar is
// shown in a minimized state.
class ASH_EXPORT ZeroStateIconButton : public DeskButtonBase {
 public:
  METADATA_HEADER(ZeroStateIconButton);

  ZeroStateIconButton(DeskBarViewBase* bar_view,
                      const gfx::VectorIcon* button_icon,
                      const std::u16string& text,
                      base::RepeatingClosure callback);
  ZeroStateIconButton(const ZeroStateIconButton&) = delete;
  ZeroStateIconButton& operator=(const ZeroStateIconButton&) = delete;
  ~ZeroStateIconButton() override;

  // DeskButtonBase:
  void OnThemeChanged() override;
  gfx::Size CalculatePreferredSize() const override;

  // views::Button:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

 private:
  const raw_ptr<const gfx::VectorIcon, ExperimentalAsh> button_icon_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_ZERO_STATE_BUTTON_H_
