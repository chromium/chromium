// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_SWITCH_ACCESS_MENU_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_SWITCH_ACCESS_MENU_VIEW_H_

#include <vector>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// View for the Switch Access menu.
class SwitchAccessMenuView : public views::View {
  METADATA_HEADER(SwitchAccessMenuView, views::View)

 public:
  SwitchAccessMenuView();
  ~SwitchAccessMenuView() override;

  SwitchAccessMenuView(const SwitchAccessMenuView&) = delete;
  SwitchAccessMenuView& operator=(const SwitchAccessMenuView&) = delete;

  int GetBubbleWidthDip() const;
  void SetActions(std::vector<std::string> actions);

 private:
  friend class SwitchAccessMenuBubbleControllerTest;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_SWITCH_ACCESS_MENU_VIEW_H_
