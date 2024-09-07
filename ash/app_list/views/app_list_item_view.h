// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/app_list/grid_index.h"
#include "ash/app_list/model/app_icon_load_helper.h"
#include "ash/app_list/model/app_list_item_observer.h"
#include "ash/app_list/views/app_list_item_view_grid_delegate.h"
#include "ash/app_list/views/apps_collection_section_view.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace views {
class Label;
}  // namespace views

namespace ash {

class AppsGridContextMenu;
class AppListConfig;
class AppListItem;
class AppListItemViewGridDelegate;
class AppListMenuModelAdapter;
class AppListViewDelegate;
class DotIndicator;
class ProgressIndicator;

namespace test {
class AppsGridViewTest;
class AppListMainViewTest;
class RecentAppsViewTest;
}  // namespace test

// An application icon and title. Commonly part of the AppsGridView, but may be
// used in other contexts. Supports dragging and keyboard selection via the
// AppListItemViewGridDelegate interface.
class ASH_EXPORT AppListItemView : public views::Button,
                                   public views::ContextMenuController,
                                   public AppListItemObserver,
                                   public ui::ImplicitAnimationObserver {
  METADATA_HEADER(AppListItemView, views::Button)

 public:
  // The types of context where the app list item view is shown.
  enum class Context {
    // The item is shown in an AppsGridView.
    kAppsGridView,

    // The item is shown in the RecentAppsView.
    kRecentAppsView,

    // The item is shown in AppsCollectionView.
    kAppsCollection
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

  AppListItemView(const AppListConfig* app_list_config,
                  AppListItemViewGridDelegate* grid_delegate,
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

  // Sets the icon and host badge icon of this image.
  void SetIconAndMaybeHostBadgeIcon(const gfx::ImageSkia& icon,
                                    const gfx::ImageSkia& host_badge_icon);

  // Returns the main app icon size for the associated item. This is the actual
  // size of the main app icon that is painted in the grid.
  gfx::Size GetIconSize() const;

  // Whether the icon used on this item is a placeholder icon for a promise app.
  // This is obtained from the value in the item's metadata.
  bool ItemHasPlaceholderIcon();

  void SetItemName(const std::u16string& display_name,
                   const std::u16string& full_name);

  void SetItemAccessibleName(const std::u16string& name);

  void SetHostBadgeIcon(const gfx::ImageSkia& host_badge_icon);

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
  gfx::ImageSkia GetDragImage() const;

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

  // Returns the host badge icon bounds using the centerpoint of
  // `main_icon_bounds` and given `host_badge_icon_container_size and the
  // `icon_scale` if the icon was scaled from the original display size.
  static gfx::Rect GetHostBadgeIconBoundsForTargetViewBounds(
      const gfx::Rect& main_icon_bounds,
      const gfx::Size& host_badge_icon_container_size,
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
  AppListItemViewGridDelegate* grid_delegate_for_test() {
    return grid_delegate_;
  }
  const ui::ImageModel& icon_image_model() const { return icon_image_model_; }
  const gfx::ImageSkia icon_image_for_test() const {
    return icon_image_model_.GetImage().AsImageSkia();
  }

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

  // Whether the app list items need to keep layers at all times.
  bool AlwaysPaintsToLayer();

  // Initializes the view to simulate a completed promise app state, and runs
  // animation to show the app list item view. Used when showing the app list
  // item view in place of a promise app.
  // `fallback_icon` - the icon that can be used for the app list item view if
  // the actual app icon has not yet been loaded. Using the `fallback_icon`
  // addresses a flash of the app item state with no icon immediately after
  // adding the view to the apps grid.
  void AnimateInFromPromiseApp(const ui::ImageModel& fallback_icon,
                               base::RepeatingClosure callback);

  // Remove all dragging states from the view.
  void ClearItemDraggingState();

  GridIndex most_recent_grid_index() { return most_recent_grid_index_; }

  bool has_pending_row_change() { return has_pending_row_change_; }
  void reset_has_pending_row_change() { has_pending_row_change_ = false; }

  const ui::Layer* icon_background_layer_for_test() const {
    if (!icon_background_) {
      return nullptr;
    }
    return icon_background_->layer();
  }
  bool is_icon_extended_for_test() const { return is_icon_extended_; }
  bool is_promise_app() const { return is_promise_app_; }
  std::optional<size_t> item_counter_count_for_test() const;
  ProgressIndicator* GetProgressIndicatorForTest() const;
  bool has_host_badge_for_test() const { return has_host_badge_; }

 private:
  class FolderIconView;

  friend class AppsCollectionSectionViewTest;
  friend class AppListFolderViewTest;
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
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;
  int GetDragOperations(const gfx::Point& press_pt) override;
  void WriteDragData(const gfx::Point& press_pt, OSExchangeData* data) override;
  void OnDragDone() override;
  void ScrollRectToVisible(const gfx::Rect& rect) override;

  // Called when the drag registered for this view starts moving.
  // `drag_start_callback` passed to
  // `AppListItemViewGridDelegate::InitiateDrag()`.
  void OnDragStarted();

  // Called when the drag registered for this view ends.
  // `drag_end_callback` passed to
  // `AppListItemViewGridDelegate::InitiateDrag()`.
  void OnDragEnded();

  // AppListItemObserver overrides:
  void ItemIconChanged(AppListConfigType config_type) override;
  void ItemNameChanged() override;
  void ItemHostBadgeIconChanged() override;
  void ItemBadgeVisibilityChanged() override;
  void ItemBadgeColorChanged() override;
  void ItemIsNewInstallChanged() override;
  void ItemBeingDestroyed() override;
  void ItemProgressUpdated() override;
  void ItemAppStatusUpdated() override;
  void ItemAppCollectionIdChanged() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // Called upon completion of the AppListItemView's show animation from a
  // promise icon state.
  void OnAnimatedInFromPromiseApp(base::RepeatingClosure callback);

  // Whether the image view should show the icon from
  // `fallback_icon_image_model_` instead of the icon from the app list item.
  // Returns true during show animation from a promise icon state, if the actual
  // app icon has not been loaded yet.
  bool ShouldUseFallbackIconImageModel() const;

  // Whether the image view has a placeholder icon in place. The placeholder
  // icon is represented as a VectorIcon in the ImageModel. Depending on the
  // case, the icon may use the `icon_image_model` or the
  // `fallback_icon_image_model` (ie, when an animation in for the promise app
  // is happening) for this calceulation.
  bool ImageModelHasPlaceholderIcon() const;

  // Calculates the transform between the icon scaled by |icon_scale| and the
  // normal size icon.
  gfx::Transform GetScaleTransform(float icon_scale);

  // Updates the icon extended state if another app is dragged onto this item
  // view, which could be either an app or a folder. `extend_icon` is true if
  // the icon background is going to extend, shrink the background otherwise.
  // `animate` specifies if the visual update should be animated or not.
  void SetBackgroundExtendedState(bool extend_icon, bool animate);

  // Returns the color ID for the app list item background, if the background
  // needs to be shown.
  ui::ColorId GetBackgroundLayerColorId() const;

  void OnExtendingAnimationEnded(bool extend_icon);

  // Returns the layer that paints the icon background.
  ui::Layer* GetIconBackgroundLayer();

  // Initialize the item drag operation if it is available at `location`.
  bool MaybeStartTouchDrag(const gfx::Point& location);

  // Updates the active `progress_indicator_` to reflect the current state of
  // the item associated to this view.
  void UpdateProgressIndicatorState();

  // Updates the layer bounds for the `progress_indicator_` if any is currently
  // active.
  void UpdateProgressRingBounds();

  // Returns the preferred inner icon size for a promise app depending on the
  // current app_state. Different from `GetIconSize()` since
  // `GetPreferredIconSizeForProgressRing()` is used to adjust padding for the
  // promise ring.
  gfx::Size GetPreferredIconSizeForProgressRing() const;

  void UpdateAccessibleDescription();

  // The app list config used to layout this view. The initial values is set
  // during view construction, but can be changed by calling
  // `UpdateAppListConfig()`.
  raw_ptr<const AppListConfig, DanglingUntriaged> app_list_config_;

  const bool is_folder_;

  // Whether context menu options have been requested. Prevents multiple
  // requests.
  bool waiting_for_context_menu_options_ = false;

  raw_ptr<AppListItem> item_weak_;  // Owned by AppListModel. Can be nullptr.

  // Handles dragging and item selection. Might be a stub for items that are not
  // part of an apps grid.
  const raw_ptr<AppListItemViewGridDelegate, DanglingUntriaged> grid_delegate_;

  // AppListControllerImpl by another name.
  const raw_ptr<AppListViewDelegate> view_delegate_;

  // Set to true if the ImageSkia icon in AppListItem is drawn. The refreshed
  // folder icons are directly drawn on FolderIconView instead of using the
  // AppListItem icon.
  const bool use_item_icon_;

  // NOTE: Only one of `icon_` and `folder_icon_` is used for an item view.
  // The icon view that uses the ImageSkia in AppListItem to draw the icon.
  raw_ptr<views::ImageView> icon_ = nullptr;

  // The folder icon view used for refreshed folders.
  raw_ptr<FolderIconView> folder_icon_ = nullptr;

  raw_ptr<views::Label> title_ = nullptr;

  // The background layer added under the `icon_` layer to paint the background
  // of the icon.
  raw_ptr<views::View> icon_background_ = nullptr;

  // Draws a dot next to the title for newly installed apps.
  raw_ptr<views::View> new_install_dot_ = nullptr;

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

  // A timer to defer showing drag UI when mouse is pressed.
  base::OneShotTimer mouse_drag_timer_;
  // A timer to defer showing drag UI when the app item is touch pressed.
  base::OneShotTimer touch_drag_timer_;

  // The bitmap image for this app list item.
  ui::ImageModel icon_image_model_;

  // The bitmap image for this app list item's host badge icon.
  gfx::ImageSkia host_badge_icon_image_;

  // The bitmap image for this app list item's main icon. This is separate from
  // icon_->GetImage(), since the latter might contain the badge image in its
  // imageSkia for shortcuts.
  gfx::ImageSkia icon_image_;

  // If set, the icon that will be used for the AppListItemView until the actual
  // app icon loads. Used when animating an installed app into a place of a
  // promise app, in which case the promise app icon is initially used as the
  // app icon to prevent jankyness due to an empty icon while the app list item
  // is being loaded.
  ui::ImageModel fallback_icon_image_model_;

  // Whether fallback icon should be preferred even if the actual app icon has
  // been loaded - set while the animation from a promise icon state is in
  // progress.
  bool prefer_fallback_icon_ = false;

  // The current item's drag state.
  DragState drag_state_ = DragState::kNone;

  // The scaling factor for displaying the app icon.
  float icon_scale_ = 1.0f;

  // Draws an indicator in the top right corner of the image to represent an
  // active notification.
  raw_ptr<DotIndicator> notification_indicator_ = nullptr;

  // Indicates the context in which this view is shown.
  const Context context_;

  // Helper to trigger icon load.
  std::optional<AppIconLoadHelper> icon_load_helper_;

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

  // Whether the app is a promise app  (i.e. an app with pending or installing
  // app status).
  bool is_promise_app_ = false;

  // Whether the app is a shortcut (i.e. a deeplink created with shortcut via
  // Chrome or other third party installed apps) and should render the host
  // badge icon.
  bool has_host_badge_ = false;

  // An object that draws and updates the progress ring around promise app
  // icons.
  std::unique_ptr<ProgressIndicator> progress_indicator_;

  // If set, the progress indicator will be shown, and indicate the contained
  // progress value. Used when animating the view in from a promise app state to
  // simulate promise icon UI.
  std::optional<float> forced_progress_indicator_value_;

  base::WeakPtrFactory<AppListItemView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_H_
