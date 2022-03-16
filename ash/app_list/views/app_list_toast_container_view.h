// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_TOAST_CONTAINER_VIEW_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_TOAST_CONTAINER_VIEW_H_

#include <memory>

#include "ash/app_list/views/app_list_nudge_controller.h"
#include "ui/views/view.h"

namespace views {
class LabelButton;
}

namespace ash {

class AppListA11yAnnouncer;
class AppListNudgeController;
class AppListToastView;
class AppsGridContextMenu;
enum class AppListSortOrder;

// A container view accommodating a toast view with type `ToastType`. See
// `ToastType` for more detail.
class AppListToastContainerView : public views::View {
 public:
  // The visibility state of the container.
  enum class VisibilityState {
    // The toast container is showing.
    kShown,
    // The toast container is shadowed by other views so it is inactive and
    // showing in the background.
    kShownInBackground,
    // The toast container is hidden.
    kHidden
  };

  // The type of toast that the container is currently showing.
  enum class ToastType {
    // The container is not showing any toast.
    kNone,
    // Shows the nudge to guide the users to use apps reordering using context
    // menu.
    kReorderNudge,
    // Shows the notification that the apps are temporarily sorted and allows
    // users to undo the sorting actions.
    kReorderUndo,
  };

  AppListToastContainerView(AppListNudgeController* nudge_controller_,
                            AppListA11yAnnouncer* a11y_announcer,
                            bool tablet_mode);
  AppListToastContainerView(const AppListToastContainerView&) = delete;
  AppListToastContainerView& operator=(const AppListToastContainerView&) =
      delete;
  ~AppListToastContainerView() override;

  // Updates the toast container to show/hide the reorder nudge if needed.
  void MaybeUpdateReorderNudgeView();

  // Creates a reorder nudge view in the container.
  void CreateReorderNudgeView();

  // Removes the reorder nudge view if the nudge view is showing.
  void RemoveReorderNudgeView();

  // Removes the current toast that is showing.
  void RemoveCurrentView();

  // Updates `visibility_state_` and the states in `nudge_controller_`.
  void UpdateVisibilityState(VisibilityState state);

  // Called when the app list temporary sort order changes. If `new_order` is
  // null, the temporary sort order is cleared.
  void OnTemporarySortOrderChanged(
      const absl::optional<AppListSortOrder>& new_order);

  // Returns the toast's target visibility for the specified sort order. If
  // `order` is null, the temporary sort order is cleared.
  bool GetVisibilityForSortOrder(
      const absl::optional<AppListSortOrder>& order) const;

  // Fires an accessibility alert with the text of the sort order toast.
  void AnnounceSortOrder(AppListSortOrder new_order);

  // This function expects that `toast_view_` exists.
  views::LabelButton* GetToastDismissButtonForTest();

  bool is_toast_visible() const { return toast_view_; }
  ToastType current_toast() const { return current_toast_; }

  AppListA11yAnnouncer* a11y_announcer_for_test() { return a11y_announcer_; }

 private:
  // Called when the `toast_view_`'s dismiss button is clicked.
  void OnReorderUndoButtonClicked();

  // Calculates the toast text based on the temporary sorting order.
  [[nodiscard]] std::u16string CalculateToastTextFromOrder(
      AppListSortOrder order) const;

  AppListA11yAnnouncer* const a11y_announcer_;

  // The app list toast container is visually part of the apps grid and should
  // provide context menu options generally available in the apps grid.
  std::unique_ptr<AppsGridContextMenu> context_menu_;

  // Whether the toast container is part of the tablet mode app list UI.
  const bool tablet_mode_;

  AppListToastView* toast_view_ = nullptr;

  AppListNudgeController* const nudge_controller_;

  // Caches the current visibility state which is used to help tracking the
  // status of reorder nudge..
  VisibilityState visibility_state_ = VisibilityState::kHidden;

  // Caches the current toast type.
  ToastType current_toast_ = ToastType::kNone;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_TOAST_CONTAINER_VIEW_H_
