// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_TOAST_CONTAINER_VIEW_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_TOAST_CONTAINER_VIEW_H_

#include <memory>
#include <string>

#include "ash/app_list/views/app_list_nudge_controller.h"
#include "ui/views/view.h"

namespace views {
class Button;
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

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Requests that focus move up and out (usually to the recent apps).
    // `column` is the column of the item (could be from the recent apps or apps
    // grid) that was focused before moving focus on this toast container. The
    // delegate should choose an appropriate item to focus.
    virtual bool MoveFocusUpFromToast(int column) = 0;

    // Requests that focus move down and out (usually to the apps grid).
    // `column` is the column of the item (could be from the recent apps or apps
    // grid) that was focused before moving focus on this toast container. The
    // delegate should choose an appropriate item to focus.
    virtual bool MoveFocusDownFromToast(int column) = 0;

    // Called when the nudge gets removed by the close or dismiss buttons.
    virtual void OnNudgeRemoved() = 0;
  };

  AppListToastContainerView(AppListNudgeController* nudge_controller_,
                            AppListA11yAnnouncer* a11y_announcer,
                            Delegate* delegate,
                            bool tablet_mode);
  AppListToastContainerView(const AppListToastContainerView&) = delete;
  AppListToastContainerView& operator=(const AppListToastContainerView&) =
      delete;
  ~AppListToastContainerView() override;

  // views::View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // Handle focus passed from the app on column `column` in AppsGridView or
  // RecentAppsView.
  bool HandleFocus(int column);

  // Updates the toast container to show/hide the reorder nudge if needed.
  void MaybeUpdateReorderNudgeView();

  // Creates a reorder nudge view in the container.
  void CreateReorderNudgeView();

  // Dismisses the reorder nudge view and ensures it will no longer be shown.
  void DismissReorderNudgeView();

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

  // Fires an accessibility alert to notify the users that the sort is reverted.
  void AnnounceUndoSort();

  // This function expects that `toast_view_` exists.
  views::LabelButton* GetToastButton();

  // Gets the close button if one exists.
  views::Button* GetCloseButton();

  AppListToastView* toast_view() { return toast_view_; }
  bool is_toast_visible() const { return toast_view_; }
  ToastType current_toast() const { return current_toast_; }

  AppListA11yAnnouncer* a11y_announcer_for_test() { return a11y_announcer_; }

 private:
  // Called when the `toast_view_`'s dismiss button is clicked.
  void OnReorderUndoButtonClicked();

  // Called when the `toast_view_`'s close button is clicked.
  void OnReorderCloseButtonClicked();

  // Calculates the toast text based on the temporary sorting order.
  [[nodiscard]] std::u16string CalculateToastTextFromOrder(
      AppListSortOrder order) const;

  std::u16string GetA11yTextOnUndoButtonFromOrder(AppListSortOrder order) const;

  AppListA11yAnnouncer* const a11y_announcer_;

  // The app list toast container is visually part of the apps grid and should
  // provide context menu options generally available in the apps grid.
  std::unique_ptr<AppsGridContextMenu> context_menu_;

  // Whether the toast container is part of the tablet mode app list UI.
  const bool tablet_mode_;

  AppListToastView* toast_view_ = nullptr;

  Delegate* const delegate_;
  AppListNudgeController* const nudge_controller_;

  // Caches the current visibility state which is used to help tracking the
  // status of reorder nudge..
  VisibilityState visibility_state_ = VisibilityState::kHidden;

  // Caches the current toast type.
  ToastType current_toast_ = ToastType::kNone;

  // Caches the column of previously focused app. Used when passing focus
  // between apps grid view and recent apps.
  int focused_app_column_ = 0;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_TOAST_CONTAINER_VIEW_H_
