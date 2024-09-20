// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_FOLDER_VIEW_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_FOLDER_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item_list_observer.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/app_list_model_observer.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/apps_grid_view_folder_delegate.h"
#include "ash/app_list/views/folder_header_view.h"
#include "ash/app_list/views/folder_header_view_delegate.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {

class AppListA11yAnnouncer;
class AppListFolderController;
class AppListFolderItem;
class AppListItemView;
class AppListModel;
class AppListViewDelegate;
class AppsContainerView;
class AppsGridView;
class FolderHeaderView;
class ScrollViewGradientHelper;
class SystemShadow;

// Displays folder contents via an AppsGridView. App items can be dragged out
// of the folder to the main apps grid.
class ASH_EXPORT AppListFolderView : public views::View,
                                     public FolderHeaderViewDelegate,
                                     public AppListModelProvider::Observer,
                                     public AppListModelObserver,
                                     public views::ViewObserver,
                                     public AppsGridViewFolderDelegate {
  METADATA_HEADER(AppListFolderView, views::View)

 public:
  // The maximum number of columns a folder can have.
  static constexpr int kMaxFolderColumns = 4;

  AppListFolderView(AppListFolderController* folder_controller,
                    AppsGridView* root_apps_grid_view,
                    AppListA11yAnnouncer* a11y_announcer,
                    AppListViewDelegate* view_delegate,
                    bool tablet_mode);
  AppListFolderView(const AppListFolderView&) = delete;
  AppListFolderView& operator=(const AppListFolderView&) = delete;
  ~AppListFolderView() override;

  // An interface for the folder opening and closing animations.
  class Animation {
   public:
    virtual ~Animation() = default;
    // `completion_callback` is an optional callback to be run when the
    // animation completes. Not run if the animation gets reset before
    // completion.
    virtual void ScheduleAnimation(base::OnceClosure completion_callback) = 0;
    virtual bool IsAnimationRunning() = 0;
  };

  // Sets the `AppListConfig` that should be used to configure app list item
  // size within the folder items grid.
  void UpdateAppListConfig(const AppListConfig* config);

  // Configures AppListFolderView to show the contents for the folder item
  // associated with `folder_item_view`. The folder view will be anchored at
  // `folder_item_view`. `hide_callback` gets called when the folder gets
  // hidden (after all hide animations complete).
  void ConfigureForFolderItemView(AppListItemView* folder_item_view,
                                  base::OnceClosure hide_callback);

  // Schedules an animation to show or hide the view.
  // If |show| is false, the view should be set to invisible after the
  // animation is done unless |hide_for_reparent| is true.
  void ScheduleShowHideAnimation(bool show, bool hide_for_reparent);

  // Hides the view immediately without animation.
  void HideViewImmediately();

  // Prepares folder item grid for closing the folder - it ends any in-progress
  // drag, and clears any selected view.
  void ResetItemsGridForClose();

  // Closes the folder page and goes back the top level page.
  void CloseFolderPage();

  // Focuses the name input text-field in the folder header.
  void FocusNameInput();

  // Focuses the first app item. Does not set the selection or perform a11y
  // announce if `silently` is true.
  void FocusFirstItem(bool silently);

  // views::View
  void AddedToWidget() override;
  void Layout(PassKey) override;
  void ChildPreferredSizeChanged(View* child) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // AppListModelProvider::Observer:
  void OnActiveAppListModelsChanged(AppListModel* model,
                                    SearchModel* search_model) override;

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* view) override;

  // AppListModelObserver
  void OnAppListItemWillBeDeleted(AppListItem* item) override;

  // Updates preferred bounds of this view based on the activated folder item
  // icon's bounds.
  void UpdatePreferredBounds();

  // Returns the Y-offset that would move a folder out from under a visible
  // Virtual keyboard
  int GetYOffsetForFolder();

  // Recalculates and updates the bounds of the folder `shadow_`  .
  void UpdateShadowBounds();

  // Called when the  `shadow_` layer gets recreated.
  void OnShadowLayerRecreated(ui::Layer* old_layer, ui::Layer* new_layer);

  // Returns true if this view's child views are in animation for opening or
  // closing the folder.
  bool IsAnimationRunning() const;

  // Sets the bounding box for the folder view bounds. The bounds are expected
  // to be in the parent view's coordinate system.
  void SetBoundingBox(const gfx::Rect& bounding_box);

  // Sets the callback that runs when the folder animation ends.
  void SetAnimationDoneTestCallback(base::OnceClosure animation_done_callback);

  AppsGridView* items_grid_view() { return items_grid_view_; }

  FolderHeaderView* folder_header_view() { return folder_header_view_; }

  views::View* animating_background() { return animating_background_; }

  views::View* contents_container() { return contents_container_; }

  const AppListFolderItem* folder_item() const { return folder_item_; }

  const gfx::Rect& folder_item_icon_bounds() const {
    return folder_item_icon_bounds_;
  }

  const gfx::Rect& preferred_bounds() const { return preferred_bounds_; }

  SystemShadow* shadow() { return shadow_.get(); }

  // Records the smoothness of folder show/hide animations mixed with the
  // BackgroundAnimation, FolderItemTitleAnimation, TopIconAnimation, and
  // ContentsContainerAnimation.
  void RecordAnimationSmoothness();

  // views::View:
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // Overridden from FolderHeaderViewDelegate:
  void SetItemName(AppListFolderItem* item, const std::string& name) override;

  // Overridden from AppsGridViewFolderDelegate:
  void ReparentItem(AppsGridView::Pointer pointer,
                    AppListItemView* original_drag_view,
                    const gfx::Point& drag_point_in_folder_grid) override;
  void DispatchEndDragEventForReparent(bool events_forwarded_to_drag_drop_host,
                                       bool cancel_drag) override;
  void Close() override;
  bool IsDragPointOutsideOfFolder(const gfx::Point& drag_point) override;
  bool IsOEMFolder() const override;
  void HandleKeyboardReparent(AppListItemView* reparented_view,
                              ui::KeyboardCode key_code) override;

  const AppListConfig* GetAppListConfig() const;

  AppListA11yAnnouncer* a11y_announcer_for_test() { return a11y_announcer_; }
  views::ScrollView* scroll_view_for_test() { return scroll_view_; }

 private:
  // Creates a vertically scrollable apps grid view.
  void CreateScrollableAppsGrid(bool tablet_mode);

  // Returns the compositor associated to the widget containing this view.
  // Returns nullptr if there isn't one associated with this widget.
  ui::Compositor* GetCompositor();

  // Called from the root apps grid view to cancel reparent drag from the root
  // apps grid.
  void CancelReparentDragFromRootGrid();

  // Resets the folder view state. Called when the folder view gets hidden (and
  // hide animations finish) to disassociate the folder view with the current
  // folder item (if any).
  // `restore_folder_item_view_state` - whether the folder item view state
  // should be restored to the default state (icon and title shown). Set to
  // false when resetting the folder state due to folder item view deletion.
  void ResetState(bool restore_folder_item_view_state);

  // Called when the animation to show the folder view is completed.
  void OnShowAnimationDone();

  // Called when the animation to hide the folder view is completed.
  // `hide_for_reparent` is true if an item in the folder is being reparented to
  // the root grid view.
  void OnHideAnimationDone(bool hide_for_reparent);

  void UpdateExpandedCollapsedAccessibleState() const;

  // Controller interface implemented by the container for this view.
  const raw_ptr<AppListFolderController> folder_controller_;

  // The root (non-folder) apps grid view.
  const raw_ptr<AppsGridView> root_apps_grid_view_;

  // Used to send accessibility alerts. Owned by the parent apps container.
  const raw_ptr<AppListA11yAnnouncer> a11y_announcer_;

  // The view is used to draw a background with corner radius.
  raw_ptr<views::View> background_view_;
  raw_ptr<views::View> animating_background_;

  // The view is used as a container for all following views.
  raw_ptr<views::View> contents_container_;  // Owned by views hierarchy.

  raw_ptr<FolderHeaderView> folder_header_view_;  // Owned by views hierarchy.
  raw_ptr<AppsGridView> items_grid_view_;         // Owned by views hierarchy.

  // Owned by views hierarchy.
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;

  std::unique_ptr<SystemShadow> shadow_;

  // Adds fade in/out gradients to `scroll_view_`.
  std::unique_ptr<ScrollViewGradientHelper> gradient_helper_;

  const raw_ptr<AppListViewDelegate> view_delegate_;
  raw_ptr<AppListFolderItem> folder_item_ = nullptr;  // Not owned.

  // Whether the folder view is currently shown, or showing.
  bool shown_ = false;

  // If set, the callback that will be called when the folder hides (after hide
  // animations complete).
  base::OnceClosure hide_callback_;

  // The folder item in the root apps grid associated with this folder.
  raw_ptr<AppListItemView> folder_item_view_ = nullptr;

  // The bounds of the activated folder item icon relative to this view.
  gfx::Rect folder_item_icon_bounds_;

  // The preferred bounds of this view relative to AppsContainerView.
  gfx::Rect preferred_bounds_;

  // The bounds of the box within which the folder view can be shown. The bounds
  // are relative the the parent view's coordinate system.
  gfx::Rect bounding_box_;

  std::vector<std::unique_ptr<Animation>> folder_visibility_animations_;

  // Records smoothness of the folder show/hide animation.
  std::optional<ui::ThroughputTracker> show_hide_metrics_tracker_;

  base::ScopedObservation<AppListModel, AppListModelObserver>
      model_observation_{this};

  // Observes `folder_item_view_` deletion, so the folder state can be cleared
  // if the folder item view is destroyed (for example, the view may get deleted
  // during folder hide animation if the backing item gets deleted from the
  // model, and animations depend on the folder item view).
  base::ScopedObservation<views::View, views::ViewObserver>
      folder_item_view_observer_{this};

  // The callback that runs at the end of the folder animation.
  base::OnceClosure animation_done_test_callback_;

  base::WeakPtrFactory<AppListFolderView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_FOLDER_VIEW_H_
