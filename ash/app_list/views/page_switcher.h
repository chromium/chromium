// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_PAGE_SWITCHER_H_
#define ASH_APP_LIST_VIEWS_PAGE_SWITCHER_H_

#include "ash/public/cpp/pagination/pagination_model_observer.h"
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
 public:
  static constexpr int kMaxButtonRadiusForRootGrid = 16;
  static constexpr int kMaxButtonRadiusForFolderGrid = 10;

  PageSwitcher(PaginationModel* model,
               bool is_root_app_grid_page_switcher,
               bool is_tablet_mode);
  PageSwitcher(const PageSwitcher&) = delete;
  PageSwitcher& operator=(const PageSwitcher&) = delete;
  ~PageSwitcher() override;

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  const char* GetClassName() const override;
  void OnThemeChanged() override;

  void set_is_tablet_mode(bool started) { is_tablet_mode_ = started; }

 private:
  // Button pressed callback.
  void HandlePageSwitch(const ui::Event& event);

  // Overridden from PaginationModelObserver:
  void TotalPagesChanged(int previous_page_count, int new_page_count) override;
  void SelectedPageChanged(int old_selected, int new_selected) override;

  PaginationModel* model_;       // Owned by AppsGridView.
  views::View* buttons_;         // Owned by views hierarchy.

  // True if the page switcher's root view is the AppsGridView.
  const bool is_root_app_grid_page_switcher_;

  // Whether tablet mode is enabled.
  bool is_tablet_mode_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_PAGE_SWITCHER_H_
