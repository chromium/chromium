// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_PAGE_INDICATOR_VIEW_H_
#define ASH_SYSTEM_UNIFIED_PAGE_INDICATOR_VIEW_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/pagination/pagination_model_observer.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

namespace ash {

class PaginationModel;
class UnifiedSystemTrayController;

// PageIndicatorView represents its underlying PaginationModel with a button
// strip. Each page in the PaginationModel has a button in the strip and
// when the button is clicked, the corresponding page becomes selected.
class ASH_EXPORT PageIndicatorView : public views::View,
                                     public PaginationModelObserver {
 public:
  PageIndicatorView(UnifiedSystemTrayController* controller,
                    bool initially_expanded);

  PageIndicatorView(const PageIndicatorView&) = delete;
  PageIndicatorView& operator=(const PageIndicatorView&) = delete;

  ~PageIndicatorView() override;

  // Change the expanded state. 0.0 if collapsed, and 1.0 if expanded.
  // Otherwise, it shows an intermediate state while animating.
  void SetExpandedAmount(double expanded_amount);

  // Returns the height of this view when the tray is fully expanded.
  int GetExpandedHeight();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  const char* GetClassName() const override;

 private:
  friend class PageIndicatorViewTest;

  class PageIndicatorButton;

  // PaginationModelObserver:
  void TotalPagesChanged(int previous_page_count, int new_page_count) override;
  void SelectedPageChanged(int old_selected, int new_selected) override;

  bool IsPageSelectedForTesting(int index);
  views::View* buttons_container() { return buttons_container_; }

  PageIndicatorButton* GetButtonByIndex(int index);

  const raw_ptr<UnifiedSystemTrayController,
                DanglingUntriaged | ExperimentalAsh>
      controller_;

  // Owned by UnifiedSystemTrayModel.
  const raw_ptr<PaginationModel, ExperimentalAsh> model_;

  double expanded_amount_;

  // Owned by views hierarchy.
  raw_ptr<views::View, ExperimentalAsh> buttons_container_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_PAGE_INDICATOR_VIEW_H_
