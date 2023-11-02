// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_SWITCH_ACCESS_BACK_BUTTON_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_SWITCH_ACCESS_BACK_BUTTON_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace ash {

class FloatingMenuButton;

// View for the Switch Access Back Button.
class SwitchAccessBackButtonView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(SwitchAccessBackButtonView);

  explicit SwitchAccessBackButtonView(bool for_menu);
  ~SwitchAccessBackButtonView() override = default;

  SwitchAccessBackButtonView(const SwitchAccessBackButtonView&) = delete;
  SwitchAccessBackButtonView& operator=(const SwitchAccessBackButtonView&) =
      delete;

  int GetFocusRingWidthPerSide();
  void SetFocusRing(bool should_show);
  void SetForMenu(bool for_menu);

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  int GetHeightForWidth(int w) const override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  void OnButtonPressed();

  bool show_focus_ring_ = false;

  // Owned by views hierarchy.
  FloatingMenuButton* back_button_;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   SwitchAccessBackButtonView,
                   views::BoxLayoutView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::SwitchAccessBackButtonView)

#endif  // ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_SWITCH_ACCESS_BACK_BUTTON_VIEW_H_
