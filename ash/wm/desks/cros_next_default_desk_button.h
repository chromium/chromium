// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_CROS_NEXT_DEFAULT_DESK_BUTTON_H_
#define ASH_WM_DESKS_CROS_NEXT_DEFAULT_DESK_BUTTON_H_

#include "ash/wm/desks/cros_next_desk_button_base.h"
#include "ui/base/metadata/metadata_impl_macros.h"

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

  // Owned by the views hierarchy.
  DesksBarView* const bar_view_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_CROS_NEXT_DEFAULT_DESK_BUTTON_H_