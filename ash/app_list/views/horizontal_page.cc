// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/horizontal_page.h"

#include "ui/views/focus/focus_manager.h"

namespace ash {

void HorizontalPage::OnWillBeHidden() {}

views::View* HorizontalPage::GetFirstFocusableView() {
  return GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), false /* reverse */, false /* dont_loop */);
}

views::View* HorizontalPage::GetLastFocusableView() {
  return GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), true /* reverse */, false /* dont_loop */);
}

gfx::Rect HorizontalPage::GetPageBoundsForState(ash::AppListState state) const {
  return gfx::Rect(GetPreferredSize());
}

bool HorizontalPage::ShouldShowSearchBox() const {
  return true;
}

const char* HorizontalPage::GetClassName() const {
  return "HorizontalPage";
}

HorizontalPage::HorizontalPage() = default;

HorizontalPage::~HorizontalPage() = default;

}  // namespace ash
