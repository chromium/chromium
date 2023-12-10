// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_page.h"

#include "ash/app_list/views/contents_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/focus/focus_manager.h"

namespace ash {

AppListPage::AppListPage() : contents_view_(nullptr) {}

AppListPage::~AppListPage() {}

void AppListPage::OnShown() {}

void AppListPage::OnWillBeShown() {}

void AppListPage::OnHidden() {}

void AppListPage::OnWillBeHidden() {}

void AppListPage::OnAnimationUpdated(double progress,
                                     AppListState from_state,
                                     AppListState to_state) {}

gfx::Size AppListPage::GetPreferredSearchBoxSize() const {
  return gfx::Size();
}

void AppListPage::UpdatePageBoundsForState(AppListState state,
                                           const gfx::Rect& contents_bounds,
                                           const gfx::Rect& search_box_bounds) {
  SetBoundsRect(
      GetPageBoundsForState(state, contents_bounds, search_box_bounds));
}

views::View* AppListPage::GetFirstFocusableView() {
  return GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), false /* reverse */, false /* dont_loop */);
}

views::View* AppListPage::GetLastFocusableView() {
  return GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), true /* reverse */, false /* dont_loop */);
}

gfx::Rect AppListPage::GetDefaultContentsBounds() const {
  DCHECK(contents_view_);
  return contents_view_->GetContentsBounds();
}

BEGIN_METADATA(AppListPage)
END_METADATA

}  // namespace ash
