// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_PAGE_SWITCHER_H_
#define ASH_APP_LIST_VIEWS_PAGE_SWITCHER_H_

#include "ash/app_list/pagination_model_observer.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"

namespace app_list {

class PaginationModel;

// PageSwitcher represents its underlying PaginationModel with a button
// strip. Each page in the PageinationModel has a button in the strip and
// when the button is clicked, the corresponding page becomes selected.
class PageSwitcher : public views::View,
                     public views::ButtonListener,
                     public PaginationModelObserver {
 public:
  PageSwitcher(PaginationModel* model, bool vertical);
  ~PageSwitcher() override;

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;

  void set_ignore_button_press(bool ignore) { ignore_button_press_ = ignore; }

 private:
  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Overridden from PaginationModelObserver:
  void TotalPagesChanged() override;
  void SelectedPageChanged(int old_selected, int new_selected) override;
  void TransitionStarted() override;
  void TransitionChanged() override;
  void TransitionEnded() override;

  PaginationModel* model_;  // Owned by AppsGridView.
  views::View* buttons_;    // Owned by views hierarchy.

  // True if the page switcher button strip should grow vertically.
  const bool vertical_;

  // True if button press should be ignored.
  bool ignore_button_press_ = false;

  DISALLOW_COPY_AND_ASSIGN(PageSwitcher);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_PAGE_SWITCHER_H_
