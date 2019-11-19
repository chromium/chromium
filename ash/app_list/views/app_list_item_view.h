// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/model/app_list_item_observer.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace views {
class ImageView;
class Label;
class ProgressBar;
}  // namespace views

namespace ash {

class AppListConfig;
class AppListItem;
class AppListMenuModelAdapter;
class AppListViewDelegate;
class AppsGridView;

class APP_LIST_EXPORT AppListItemView : public views::Button,
                                        public views::ContextMenuController,
                                        public AppListItemObserver {
 public:
  // Internal class name.
  static const char kViewClassName[];

  AppListItemView(AppsGridView* apps_grid_view,
                  AppListItem* item,
                  AppListViewDelegate* delegate);
  AppListItemView(AppsGridView* apps_grid_view,
                  AppListItem* item,
                  AppListViewDelegate* delegate,
                  bool is_in_folder);
  ~AppListItemView() override;

  // Sets the icon of this image.
  void SetIcon(const gfx::ImageSkia& icon);

  void SetItemName(const base::string16& display_name,
                   const base::string16& full_name);
  void SetItemIsInstalling(bool is_installing);
  void SetItemPercentDownloaded(int percent_downloaded);

  void CancelContextMenu();

  void OnDragEnded();
  gfx::Point GetDragImageOffset();

  void SetAsAttemptedFolderTarget(bool is_target_folder);

  // Sets focus without a11y announcements or focus ring.
  void SilentlyRequestFocus();

  // Helper for getting current app list config from the parents in the app list
  // view hierarchy.
  const AppListConfig& GetAppListConfig() const;

  AppListItem* item() const { return item_weak_; }

  views::Label* title() { return title_; }

  // In a synchronous drag the item view isn't informed directly of the drag
  // ending, so the runner of the drag should call this.
  void OnSyncDragEnd();

  // Returns the icon bounds relative to AppListItemView.
  gfx::Rect GetIconBounds() const;

  // Returns the icon bounds in screen.
  gfx::Rect GetIconBoundsInScreen() const;

  // Returns the image of icon.
  gfx::ImageSkia GetIconImage() const;

  // Sets the icon's visibility.
  void SetIconVisible(bool visible);

  // Sets UI state to dragging state.
  void SetDragUIState();

  // Returns the icon bounds for with |target_bounds| as the bounds of this view
  // and given |icon_size|.
  static gfx::Rect GetIconBoundsForTargetViewBounds(
      const AppListConfig& config,
      const gfx::Rect& target_bounds,
      const gfx::Size& icon_size);

  // Returns the title bounds for with |target_bounds| as the bounds of this
  // view and given |title_size|.
  static gfx::Rect GetTitleBoundsForTargetViewBounds(
      const AppListConfig& config,
      const gfx::Rect& target_bounds,
      const gfx::Size& title_size);

  // Returns the progress bar bounds for with |target_bounds| as the bounds of
  // this view and given |progress_bar_size|.
  static gfx::Rect GetProgressBarBoundsForTargetViewBounds(
      const gfx::Rect& target_bounds,
      const gfx::Size& progress_bar_size);

  // views::Button overrides:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::View overrides:
  base::string16 GetTooltipText(const gfx::Point& p) const override;

  // When a dragged view enters this view, a preview circle is shown for
  // non-folder item while the icon is enlarged for folder item. When a
  // dragged view exits this view, the reverse animation will be performed.
  void OnDraggedViewEnter();
  void OnDraggedViewExit();

  // Enables background blur for folder icon if |enabled| is true.
  void SetBackgroundBlurEnabled(bool enabled);

  // Ensures this item view has its own layer.
  void EnsureLayer();

  void FireMouseDragTimerForTest();

  bool is_folder() const { return is_folder_; }

 private:
  class IconImageView;

  enum UIState {
    UI_STATE_NORMAL,              // Normal UI (icon + label)
    UI_STATE_DRAGGING,            // Dragging UI (scaled icon only)
    UI_STATE_DROPPING_IN_FOLDER,  // Folder dropping preview UI
  };

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // Callback used when a menu is closed.
  void OnMenuClosed();

  // Get icon from |item_| and schedule background processing.
  void UpdateIcon();

  // Update the tooltip text from |item_|.
  void UpdateTooltip();

  void SetUIState(UIState state);

  // Scales up app icon if |scale_up| is true; otherwise, scale it back to
  // normal size.
  void ScaleAppIcon(bool scale_up);

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
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // views::View overrides:
  const char* GetClassName() const override;
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;

  // AppListItemObserver overrides:
  void ItemIconChanged(ash::AppListConfigType config_type) override;
  void ItemNameChanged() override;
  void ItemIsInstallingChanged() override;
  void ItemPercentDownloadedChanged() override;
  void ItemBeingDestroyed() override;

  // Returns the radius of preview circle.
  int GetPreviewCircleRadius() const;

  // Creates dragged view hover animation if it does not exist.
  void CreateDraggedViewHoverAnimation();

  // Modifies AppListItemView bounds to match the selected highlight bounds.
  void AdaptBoundsForSelectionHighlight(gfx::Rect* rect);

  const bool is_folder_;

  // Whether context menu options have been requested. Prevents multiple
  // requests.
  bool waiting_for_context_menu_options_ = false;

  AppListItem* item_weak_;  // Owned by AppListModel. Can be nullptr.

  AppListViewDelegate* delegate_;            // Unowned.
  AppsGridView* apps_grid_view_;             // Parent view, owns this.
  IconImageView* icon_;                      // Strongly typed child view.
  views::Label* title_;                      // Strongly typed child view.
  views::ProgressBar* progress_bar_;         // Strongly typed child view.
  views::ImageView* icon_shadow_ = nullptr;  // Strongly typed child view.

  std::unique_ptr<AppListMenuModelAdapter> context_menu_;

  UIState ui_state_ = UI_STATE_NORMAL;

  // True if scroll gestures should contribute to dragging.
  bool touch_dragging_ = false;

  // True if the app is enabled for drag/drop operation by mouse.
  bool mouse_dragging_ = false;
  // True if the drag host proxy is crated for mouse dragging.
  bool mouse_drag_proxy_created_ = false;

  // Whether AppsGridView should not be notified of a focus event, triggering
  // A11y alerts and a focus ring.
  bool focus_silently_ = false;

  // The animation that runs when dragged view enters or exits this view.
  std::unique_ptr<gfx::SlideAnimation> dragged_view_hover_animation_;

  // The radius of preview circle for non-folder item.
  int preview_circle_radius_ = 0;

  bool is_installing_ = false;

  // Whether |context_menu_| was cancelled as the result of a continuous drag
  // gesture.
  bool menu_close_initiated_from_drag_ = false;

  // Whether |context_menu_| was shown via key event.
  bool menu_show_initiated_from_key_ = false;

  base::string16 tooltip_text_;

  // A timer to defer showing drag UI when mouse is pressed.
  base::OneShotTimer mouse_drag_timer_;
  // A timer to defer showing drag UI when the app item is touch pressed.
  base::OneShotTimer touch_drag_timer_;

  // The shadow margins added to the app list item title.
  gfx::Insets title_shadow_margins_;

  base::WeakPtrFactory<AppListItemView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppListItemView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_ITEM_VIEW_H_
