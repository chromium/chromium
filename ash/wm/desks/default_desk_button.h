// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DEFAULT_DESK_BUTTON_H_
#define ASH_WM_DESKS_DEFAULT_DESK_BUTTON_H_

#include "ash/wm/desks/desk_button_base.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

class DeskBarViewBase;

// A button in zero state bar showing the name of the desk. Zero state is the
// state of the desks bar when there's only a single desk available, in which
// case the bar is shown in a minimized state. Clicking the button will switch
// to the expanded desks bar and focus on the single desk's name view.
class DefaultDeskButton : public DeskButtonBase {
  METADATA_HEADER(DefaultDeskButton, DeskButtonBase)

 public:
  explicit DefaultDeskButton(DeskBarViewBase* bar_view);
  DefaultDeskButton(const DefaultDeskButton&) = delete;
  DefaultDeskButton& operator=(const DefaultDeskButton&) = delete;
  ~DefaultDeskButton() override = default;

  void UpdateLabelText();

  // DeskButtonBase:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  void OnButtonPressed();
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DEFAULT_DESK_BUTTON_H_
