// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_REORDER_UNDO_CONTAINER_VIEW_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_REORDER_UNDO_CONTAINER_VIEW_H_

#include "ui/views/view.h"

namespace views {
class LabelButton;
}

namespace ash {

class SystemToastStyle;
enum class AppListSortOrder;

// A view accommodating a toast view that reverts the app list temporary
// sorting order when the toast dismiss button is clicked.
class AppListReorderUndoContainerView : public views::View {
 public:
  AppListReorderUndoContainerView();
  AppListReorderUndoContainerView(const AppListReorderUndoContainerView&) =
      delete;
  AppListReorderUndoContainerView& operator=(
      const AppListReorderUndoContainerView&) = delete;
  ~AppListReorderUndoContainerView() override;

  // Called when the app list temporary sort order changes. If `new_order` is
  // null, the temporary sort order is cleared.
  void OnTemporarySortOrderChanged(
      const absl::optional<AppListSortOrder>& new_order);

  // This function expects that `toast_view_` exists.
  views::LabelButton* GetToastDismissButtonForTest();

  bool is_toast_visible_for_test() const { return toast_view_; }

 private:
  // Called when the `toast_view_`'s dismiss button is clicked.
  void OnReorderUndoButtonClicked();

  // Calculates the toast text based on the temporary sorting order.
  std::u16string CalculateToastTextFromOrder(AppListSortOrder order) const
      WARN_UNUSED_RESULT;

  SystemToastStyle* toast_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_REORDER_UNDO_CONTAINER_VIEW_H_
