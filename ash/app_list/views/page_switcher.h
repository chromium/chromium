// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_PAGE_SWITCHER_H_
#define ASH_APP_LIST_VIEWS_PAGE_SWITCHER_H_

#include "ash/public/cpp/pagination/pagination_model_observer.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/view.h"

namespace views {
class Button;
}

namespace ash {
class PaginationModel;

// PageSwitcher represents its underlying PaginationModel with a button
// strip. Each page in the PageinationModel has a button in the strip and
// when the button is clicked, the corresponding page becomes selected.
class PageSwitcher : public views::View,
                     public PaginationModelObserver {
  METADATA_HEADER(PageSwitcher, views::View)

 public:
  static constexpr int kMaxButtonRadius = 16;

  explicit PageSwitcher(PaginationModel* model);
  PageSwitcher(const PageSwitcher&) = delete;
  PageSwitcher& operator=(const PageSwitcher&) = delete;
  ~PageSwitcher() override;

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;
  void OnThemeChanged() override;

 private:
  // Button pressed callback.
  void HandlePageSwitch(const ui::Event& event);

  // Overridden from PaginationModelObserver:
  void TotalPagesChanged(int previous_page_count, int new_page_count) override;
  void SelectedPageChanged(int old_selected, int new_selected) override;

  raw_ptr<PaginationModel> model_;  // Owned by AppsGridView.
  raw_ptr<views::View> buttons_;    // Owned by views hierarchy.
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_PAGE_SWITCHER_H_
