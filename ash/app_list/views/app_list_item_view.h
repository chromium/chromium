// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_H_

#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/grid_index.h"
#include "ash/app_list/model/app_icon_load_helper.h"
#include "ash/app_list/model/app_list_item_observer.h"
#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace ui {
class LocatedEvent;
class SimpleMenuModel;
}  // namespace ui

namespace views {
class Label;
}  // namespace views

namespace ash {

class AppsGridContextMenu;
class AppListConfig;
class AppListItem;
class AppListMenuModelAdapter;
class AppListViewDelegate;
class DotIndicator;

namespace test {
class AppsGridViewTest;
class AppListMainViewTest;
class RecentAppsViewTest;
}  // namespace test

// An application icon and title. Commonly part of the AppsGridView, but may be
// used in other contexts. Supports dragging and keyboard selection via the
// GridDelegate interface.
class ASH_EXPORT AppListItemView : public views::Button,
                                   public views::ContextMenuController,
                                   public AppListItemObserver,
                                   public ui::ImplicitAnimationObserver {
 public:
  METADATA_HEADER(AppListItemView);

  // The types of context where the app list item view is shown.
  enum class Context {
    // The item is shown in an AppsGridView.
    kAppsGridView,

    // The item is shown in the RecentAppsView.
    kRecentAppsView
  };

  // Describes the app list item view drag state.
  enum class DragState {
    // Item is not being dragged.
    kNone,

    // Drag is initialized for the item (the owning apps grid considers the view
    // to be the dragged view), but the item is still not being dragged.
    // Depending on mouse/touch drag timers, UI may be in either normal, or
    // dragging state.
    kInitialized,

    // The item drag is in progress. While in this state, the owning apps grid
    // view will generally hide the item view, and replace it with a drag icon
    // widget. The UI should be in dragging state (scaled up and with title
    // hidden).
    kStarted,
  };

  // The parent apps grid (AppsGridView) or a stub. Not named "Delegate" to
  // differentiate it from AppListViewDelegate.
  class GridDelegate {
   public:
    virtual ~GridDelegate() = default;

    // Whether the parent apps grid (if any) is a folder.
    virtual bool IsInFolder() const = 0;

    // Methods for keyboard selection.
    virtual void SetSelectedView(AppListItemView* view) = 0;
    virtual void ClearSelectedView() = 0;
    virtual bool IsSelectedView(const AppListItemView* view) const = 0;

    // Registers `view` as a dragged item with the apps grid. Called when the
    // user presses the mouse, or starts touch interaction with the view (both
    // of which may transition into a drag operation).
    // `location` - The pointer location in the view's bounds.
    // `root_location` - The pointer location in the root window coordinates.
    // `drag_start_callback` - Callback that gets called when the mouse/touch
    //     interaction transitions into a drag (i.e. when the "drag" item starts
    //     moving.
    //  `drag_end_callback` - Callback that gets called when drag interaction
    //     ends.
    //  Returns whether `view` has been registered as a dragged view. Callbacks
    //  should be ignored if the method returns false. If the method returns
    //  true, it's expected to eventually run `drag_end_callback`.
    virtual bool InitiateDrag(AppListItemView* view,
                              const gfx::Point& location,
                              const gfx::Point& root_location,
                              base::OnceClosure drag_start_callback,
                              base::OnceClosure drag_end_callback) = 0;
    virtual void StartDragAndDropHostDragAfterLongPress() = 0;
    // Called from AppListItemView when it receives a drag event. Returns true
    // if the drag is still happening.
    virtual bool UpdateDragFromItem(bool is_touch,
                                    const ui::LocatedEvent& event) = 0;
    virtual void EndDrag(bool cancel) = 0;

    // Provided as a callback for AppListItemView to notify of activation via
    // press/click/return key.
    virtual void OnAppListItemViewActivated(AppListItemView* pressed_item_view,
                                            const ui::Event& event) = 0;
  };

  AppListItemView(const AppListConfig* app_list_config,
                  GridDelegate* grid_delegate,
                  AppListItem* item,
                  AppListViewDelegate* view_delegate,
                  Context context);
  AppListItemView(const AppListItemView&) = delete;
  AppListItemView& operator=(const AppListItemView&) = delete;
  ~AppListItemView() override;

  // Initializes icon loader. Should be called after the view has been added to
  // the apps grid view model - otherwise, if icon gets updated synchronously,
  // it may update the item metadata before the view gets added to the view
  // model. If the metadata update causes a position change, attempts to move
  // the item in the view model could crash.
  void InitializeIconLoader();

  // Sets the app list config that should be used to size the app list icon, and
  // margins within the app list item view. The owner should ensure the
  // `AppListItemView` does not outlive the object referenced by
  // `app_list_config_`.
  void UpdateAppListConfig(const AppListConfig* app_list_config);

  // Updates the currently dragged AppListItemView to update the `folder_icon_`.
  void UpdateDraggedItem(const AppListItem* dragged_item);

  // Updates and repaints the icon view, which could be either `icon_` or
  // `folder_icon_`.
  // For `icon_`, update the image icon from AppListItem if `update_item_icon`
  // is true.
  void UpdateIconView(bool update_item_icon);

  // Sets the icon of this image.
  void SetIcon(const gfx::ImageSkia& icon);

  void SetItemName(const std::u16string& display_name,
                   const std::u16string& full_name);

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  void CancelContextMenu();

  void SetAsAttemptedFolderTarget(bool is_target_folder);

  // Sets focus without a11y announcements or focus ring.
  void SilentlyRequestFocus();

  // Ensures that the item view is selected by `grid_delegate_`.
  void EnsureSelected();

  AppListItem* item() const { return item_weak_; }

  views::Label* title() { return title_; }

  // In a synchronous drag the item view isn't informed directly of the drag
  // ending, so the runner of the drag should call this.
  void OnSyncDragEnd();

  // Returns the view that draws the item view icon.
  views::View* GetIconView() const;

  // Returns the icon bounds relative to AppListItemView.
  gfx::Rect GetIconBounds() const;

  // Returns the icon bounds in screen.
  gfx::Rect GetIconBoundsInScreen() const;

  // Returns the image of icon.
  gfx::ImageSkia GetIconImage() const;

  // Sets the icon's visibility.
  void SetIconVisible(bool visible);

  // Handles the icon's scaling and animation for a cardified grid.
  void EnterCardifyState();
  void ExitCardifyState();

  // Returns the icon bounds for with |target_bounds| as the bounds of this view
  // and given |icon_size| and the |icon_scale| if the icon was scaled from the
  // original display size.
  static gfx::Rect GetIconBoundsForTargetViewBounds(
      const AppListConfig* config,
      const gfx::Rect& target_bounds,
      const gfx::Size& icon_size,
      float icon_scale);

  // Returns the title bounds for with |target_bounds| as the bounds of this
  // view and given |title_size| and the |icon_scale| if the icon was scaled
  // from the original display size.
  static gfx::Rect GetTitleBoundsForTargetViewBounds(
      const AppListConfig* config,
      const gfx::Rect& target_bounds,
      const gfx::Size& title_size,
      float icon_scale);

  // views::Button overrides:
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnThemeChanged() override;

  // views::View overrides:
  std::u16string GetTooltipText(const gfx::Point& p) const override;

  // When a dragged view enters this view, a preview circle is shown for
  // non-folder item while the icon is enlarged for folder item. When a
  // dragged view exits this view, the reverse animation will be performed.
  void OnDraggedViewEnter();
  void OnDraggedViewExit();

  // Enables background blur for folder icon if |enabled| is true.
  void SetBackgroundBlurEnabled(bool enabled);

  // Ensures this item view has its own layer.
  void EnsureLayer();

  bool HasNotificationBadge();

  bool FireMouseDragTimerForTest();

  bool FireTouchDragTimerForTest();

  // Whether the context menu on a non-folder app item view is showing.
  bool IsShowingAppMenu() const;

  // Whether the item can be dragged within its `context_`.
  bool IsItemDraggable() const;

  bool is_folder() const { return is_folder_; }

  bool IsNotificationIndicatorShownForTest() const;
  GridDelegate* grid_delegate_for_test() { return grid_delegate_; }
  const gfx::ImageSkia& icon_image_for_test() const { return icon_image_; }

  AppListMenuModelAdapter* item_menu_model_adapter() const {
    return item_menu_model_adapter_.get();
  }
  AppsGridContextMenu* context_menu_for_folder() const {
    return context_menu_for_folder_.get();
  }

  // Sets the callback which will run after the context menu is shown.
  void SetContextMenuShownCallbackForTest(base::RepeatingClosure closure);

  // Returns the bounds that would be used for the title if there was no blue
  // dot for new install.
  gfx::Rect GetDefaultTitleBoundsForTest();

  // Sets the most recent grid index for this item view. Also sets
  // `has_pending_row_change_` based on whether the grid index change is
  // considered a row change for the purposes of animating item views between
  // rows.
  void SetMostRecentGridIndex(GridIndex new_grid_index, int columns);

  GridIndex most_recent_grid_index() { return most_recent_grid_index_; }

  bool has_pending_row_change() { return has_pending_row_change_; }
  void reset_has_pending_row_change() { has_pending_row_change_ = false; }

  const ui::Layer* icon_background_layer_for_test() const {
    if (!icon_background_layer_) {
      return nullptr;
    }
    return icon_background_layer_->layer();
  }
  bool is_icon_extended_for_test() const { return is_icon_extended_; }
  absl::optional<size_t> item_counter_count_for_test() const;

 private:
  class FolderIconView;

  friend class AppListItemViewTest;
  friend class AppListMainViewTest;
  friend class test::AppsGridViewTest;
  friend class RecentAppsViewTest;

  enum UIState {
    UI_STATE_NORMAL,              // Normal UI (icon + label)
    UI_STATE_DRAGGING,            // Dragging UI (scaled icon only)
    UI_STATE_DROPPING_IN_FOLDER,  // Folder dropping preview UI
    UI_STATE_TOUCH_DRAGGING,      // Dragging UI for touch drag (non-scaled icon
                                  // only)
  };

  // Callback used when a menu is closed.
  void OnMenuClosed();

  void SetUIState(UIState state);

  // Scales up app icon if |scale_up| is true; otherwise, scale it back to
  // normal size.
  void ScaleAppIcon(bool scale_up);

  // Scales app icon to |scale_factor| without animation.
  void ScaleIconImmediatly(float scale_factor);

  // Updates the bounds of the icon background layer.
  void UpdateBackgroundLayerBounds();

  // Sets |touch_dragging_| flag and updates UI.
  void SetTouchDragging(bool touch_dragging);
  // Sets |mouse_dragging_| flag and updates UI. Only to be called on
  // |mouse_drag_timer_|.
  void SetMouseDragging(bool mouse_dragging);

  // Invoked when |mouse_drag_timer_| fires to show dragging UI.
  void OnMouseDragTimer();

  // Invoked when |touch_drag_timer_| fires to show dragging UI.
  void OnTouchDragTimer(const gfx::Point& tap_down_location,
                        const gfx::Point& tap_down_root_location);

  // Registers this view as a dragged view with the grid delegate.
  bool InitiateDrag(const gfx::Point& location,
                    const gfx::Point& root_location);

  // Callback invoked when a context menu is received after calling
  // |AppListViewDelegate::GetContextMenuModel|.
  void OnContextMenuModelReceived(
      const gfx::Point& point,
      ui::MenuSourceType source_type,
      std::unique_ptr<ui::SimpleMenuModel> menu_model);

  // views::ContextMenuController overrides:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // views::Button overrides:
  bool ShouldEnterPushedState(const ui::Event& event) override;

  // views::View overrides:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;
  int GetDragOperations(const gfx::Point& press_pt) override;
  void WriteDragData(const gfx::Point& press_pt, OSExchangeData* data) override;
  void OnDragDone() override;

  // Called when the drag registered for this view starts moving.
  // `drag_start_callback` passed to `GridDelegate::InitiateDrag()`.
  void OnDragStarted();

  // Called when the drag registered for this view ends.
  // `drag_end_callback` passed to `GridDelegate::InitiateDrag()`.
  void OnDragEnded();

  // AppListItemObserver overrides:
  void ItemIconChanged(AppListConfigType config_type) override;
  void ItemNameChanged() override;
  void ItemBadgeVisibilityChanged() override;
  void ItemBadgeColorChanged() override;
  void ItemIsNewInstallChanged() override;
  void ItemBeingDestroyed() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // Calculates the transform between the icon scaled by |icon_scale| and the
  // normal size icon.
  gfx::Transform GetScaleTransform(float icon_scale);

  // Updates the icon extended state if another app is dragged onto this item
  // view, which could be either an app or a folder. `extend_icon` is true if
  // the icon background is going to extend, shrink the background otherwise.
  // `animate` specifies if the visual update should be animated or not.
  void SetBackgroundExtendedState(bool extend_icon, bool animate);

  // Ensures that the layer where the icon background is painted on is created.
  void EnsureIconBackgroundLayer();

  // Returns the color ID for the app list item background, if the background
  // needs to be shown.
  ui::ColorId GetBackgroundLayerColorId() const;

  void OnExtendingAnimationEnded(bool extend_icon);

  // Returns the layer that paints the icon background.
  ui::Layer* GetIconBackgroundLayer();

  // Initialize the item drag operation if it is available at `location`.
  bool MaybeStartTouchDrag(const gfx::Point& location);

  // The app list config used to layout this view. The initial values is set
  // during view construction, but can be changed by calling
  // `UpdateAppListConfig()`.
  raw_ptr<const AppListConfig, ExperimentalAsh> app_list_config_;

  const bool is_folder_;

  // Whether context menu options have been requested. Prevents multiple
  // requests.
  bool waiting_for_context_menu_options_ = false;

  raw_ptr<AppListItem, ExperimentalAsh>
      item_weak_;  // Owned by AppListModel. Can be nullptr.

  // Handles dragging and item selection. Might be a stub for items that are not
  // part of an apps grid.
  const raw_ptr<GridDelegate, ExperimentalAsh> grid_delegate_;

  // AppListControllerImpl by another name.
  const raw_ptr<AppListViewDelegate, ExperimentalAsh> view_delegate_;

  // Set to true if the ImageSkia icon in AppListItem is drawn. The refreshed
  // folder icons are directly drawn on FolderIconView instead of using the
  // AppListItem icon.
  const bool use_item_icon_;

  // NOTE: Only one of `icon_` and `folder_icon_` is used for an item view.
  // The icon view that uses the ImageSkia in AppListItem to draw the icon.
  raw_ptr<views::ImageView, ExperimentalAsh> icon_ = nullptr;
  // The folder icon view used for refreshed folders.
  raw_ptr<FolderIconView, ExperimentalAsh> folder_icon_ = nullptr;

  raw_ptr<views::Label, ExperimentalAsh> title_ = nullptr;

  // The background layer added under the `icon_` layer to paint the background
  // of the icon.
  std::unique_ptr<ui::LayerOwner> icon_background_layer_;

  // Draws a dot next to the title for newly installed apps.
  raw_ptr<views::View, ExperimentalAsh> new_install_dot_ = nullptr;

  // The context menu model adapter used for app item view.
  std::unique_ptr<AppListMenuModelAdapter> item_menu_model_adapter_;

  // The context menu controller used for folder item view.
  std::unique_ptr<AppsGridContextMenu> context_menu_for_folder_;

  UIState ui_state_ = UI_STATE_NORMAL;

  // True if scroll gestures should contribute to dragging.
  bool touch_dragging_ = false;

  // True if the app is enabled for drag/drop operation by mouse.
  bool mouse_dragging_ = false;

  // Whether AppsGridView should not be notified of a focus event, triggering
  // A11y alerts and a focus ring.
  bool focus_silently_ = false;

  // Whether AppsGridView is in cardified state.
  bool in_cardified_grid_ = false;

  // The radius of preview circle for non-folder item.
  int preview_circle_radius_ = 0;

  // Whether `item_menu_model_adapter_` was cancelled as the result of a
  // continuous drag gesture.
  bool menu_close_initiated_from_drag_ = false;

  // Whether `item_menu_model_adapter_` was shown via key event.
  bool menu_show_initiated_from_key_ = false;

  std::u16string tooltip_text_;

  // A timer to defer showing drag UI when mouse is pressed.
  base::OneShotTimer mouse_drag_timer_;
  // A timer to defer showing drag UI when the app item is touch pressed.
  base::OneShotTimer touch_drag_timer_;

  // The bitmap image for this app list item.
  gfx::ImageSkia icon_image_;

  // The current item's drag state.
  DragState drag_state_ = DragState::kNone;

  // The scaling factor for displaying the app icon.
  float icon_scale_ = 1.0f;

  // Draws an indicator in the top right corner of the image to represent an
  // active notification.
  raw_ptr<DotIndicator, ExperimentalAsh> notification_indicator_ = nullptr;

  // Indicates the context in which this view is shown.
  const Context context_;

  // Helper to trigger icon load.
  absl::optional<AppIconLoadHelper> icon_load_helper_;

  // Called when the context menu is shown.
  base::RepeatingClosure context_menu_shown_callback_;

  // The most recent location of this item within the app grid.
  GridIndex most_recent_grid_index_;

  // Whether the last grid index update was a change in position between rows.
  // Used to determine whether the animation between rows should be used.
  bool has_pending_row_change_ = false;

  // Whether the context menu removed focus on a view when opening. Used to
  // determine if the focus should be restored on context menu close.
  bool focus_removed_by_context_menu_ = false;

  // Whether the `icon_` is in the extended state, where a dragged view entered
  // this item view.
  bool is_icon_extended_ = false;

  // Whether the icon background animation is being setup. Used to prevent the
  // background layer from being deleted during setup.
  bool setting_up_icon_animation_ = false;

  base::WeakPtrFactory<AppListItemView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_H_
