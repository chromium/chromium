// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_VIEW_H_
#define ASH_SHELF_SHELF_VIEW_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/views/app_list_drag_and_drop_host.h"
#include "ash/public/cpp/shelf_model_observer.h"
#include "ash/public/interfaces/shelf.mojom.h"
#include "ash/shelf/ink_drop_button_listener.h"
#include "ash/shelf/shelf_button_pressed_metric_tracker.h"
#include "ash/shelf/shelf_tooltip_manager.h"
#include "ash/system/model/virtual_keyboard_model.h"
#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace ui {
class SimpleMenuModel;
}

namespace views {
class BoundsAnimator;
class MenuRunner;
class Separator;
}  // namespace views

namespace ash {
class AppListButton;
class BackButton;
class DragImageView;
class OverflowBubble;
class OverflowButton;
class ScopedRootWindowForNewWindows;
class Shelf;
class ShelfButton;
class ShelfModel;
struct ShelfItem;
class ShelfMenuModelAdapter;
class ShelfWidget;

enum ShelfAlignmentUmaEnumValue {
  SHELF_ALIGNMENT_UMA_ENUM_VALUE_BOTTOM,
  SHELF_ALIGNMENT_UMA_ENUM_VALUE_LEFT,
  SHELF_ALIGNMENT_UMA_ENUM_VALUE_RIGHT,
  // Must be last entry in enum.
  SHELF_ALIGNMENT_UMA_ENUM_VALUE_COUNT,
};

// ShelfView contains the shelf items visible within an active user session.
// ShelfView and LoginShelfView should never be shown together.

// In the following example, there are 12 apps to place on the shelf, plus
// the app list and back buttons, which make 14 shelf items in total.
//
// If there is enough screen space, all icons can fit:
//
// -----------------------------------------------------------------
// | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 10 | 11 | 12 | 13 |
// -----------------------------------------------------------------
//   ^                                               ^
//   |                                               |
// first_visible_index = 0                 last_visible_index = 13
// (back button = 0 is hidden)
//
// Where:
//     0 = back button (only shown in tablet mode)
//     1 = app list button
//
// If screen space is more constrained, some icons are placed in an overflow
// menu (which holds its own instance of ShelfView):
//
//            first_visible_index = 10
//               (for the overflow)     last_visible_index = 13 (for overflow)
//                                |               |
//                                v               v
//                              ---------------------
//                              | 10 | 11 | 12 | 13 |
//                              ---------------------
//                                        ^
// -------------------------------------------
// | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | ... |
// -------------------------------------------
//   ^                                    ^
//   |                                    |
// first_visible_index = 0       last_visible_index = 10
//   (for the main shelf)         (the overflow button)
//  (back button = 0
//           is hidden)
//
// Note that last_visible_index is 10 (not 9) even though the overflow button
// doesn't shift the array of indices.

class ASH_EXPORT ShelfView : public views::View,
                             public ShelfModelObserver,
                             public InkDropButtonListener,
                             public views::ContextMenuController,
                             public views::FocusTraversable,
                             public views::BoundsAnimatorObserver,
                             public app_list::ApplicationDragAndDropHost,
                             public ash::TabletModeObserver,
                             public VirtualKeyboardModel::Observer {
 public:
  ShelfView(ShelfModel* model, Shelf* shelf, ShelfWidget* shelf_widget);
  ~ShelfView() override;

  Shelf* shelf() const { return shelf_; }
  ShelfModel* model() const { return model_; }

  void Init();

  void OnShelfAlignmentChanged();

  // Returns the ideal bounds of the specified item, or an empty rect if id
  // isn't know. If the item is in an overflow shelf, the overflow icon location
  // will be returned.
  gfx::Rect GetIdealBoundsOfItemIcon(const ShelfID& id);

  // Returns true if we're showing a menu.
  bool IsShowingMenu() const;

  // Returns true if we're showing a menu for |view|. |view| could be a
  // ShelfButton or the ShelfView.
  bool IsShowingMenuForView(const views::View* view) const;

  // Returns true if overflow bubble is shown.
  bool IsShowingOverflowBubble() const;

  // Sets owner overflow bubble instance from which this shelf view pops
  // out as overflow.
  void set_owner_overflow_bubble(OverflowBubble* owner) {
    owner_overflow_bubble_ = owner;
  }

  AppListButton* GetAppListButton() const;
  BackButton* GetBackButton() const;

  // Returns true if the mouse cursor exits the area for launcher tooltip.
  // There are thin gaps between launcher buttons but the tooltip shouldn't hide
  // in the gaps, but the tooltip should hide if the mouse moved totally outside
  // of the buttons area.
  bool ShouldHideTooltip(const gfx::Point& cursor_location) const;

  // Returns true if a tooltip should be shown for the shelf item |view|.
  bool ShouldShowTooltipForView(const views::View* view) const;

  // Returns the title of the shelf item |view|.
  base::string16 GetTitleForView(const views::View* view) const;

  // Returns rectangle bounding all visible launcher items. Used screen
  // coordinate system.
  gfx::Rect GetVisibleItemsBoundsInScreen();

  // InkDropButtonListener:
  void ButtonPressed(views::Button* sender,
                     const ui::Event& event,
                     views::InkDrop* ink_drop) override;

  // Overridden from FocusTraversable:
  views::FocusSearch* GetFocusSearch() override;
  FocusTraversable* GetFocusTraversableParent() override;
  View* GetFocusTraversableParentView() override;

  // Overridden from app_list::ApplicationDragAndDropHost:
  void CreateDragIconProxy(const gfx::Point& location_in_screen_coordinates,
                           const gfx::ImageSkia& icon,
                           views::View* replaced_view,
                           const gfx::Vector2d& cursor_offset_from_center,
                           float scale_factor) override;

  // Overridden from ash::TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // Overridden from VirtualKeyboardModel::Observer:
  void OnVirtualKeyboardVisibilityChanged() override;

  void CreateDragIconProxyByLocationWithNoAnimation(
      const gfx::Point& origin_in_screen_coordinates,
      const gfx::ImageSkia& icon,
      views::View* replaced_view,
      float scale_factor,
      int blur_radius) override;

  void UpdateDragIconProxy(
      const gfx::Point& location_in_screen_coordinates) override;

  void UpdateDragIconProxyByLocation(
      const gfx::Point& origin_in_screen_coordinates) override;

  void DestroyDragIconProxy() override;
  bool StartDrag(const std::string& app_id,
                 const gfx::Point& location_in_screen_coordinates) override;
  bool Drag(const gfx::Point& location_in_screen_coordinates) override;
  void EndDrag(bool cancel) override;

  // Returns true if |event| on the shelf item is going to activate the item.
  // Used to determine whether a pending ink drop should be shown or not.
  bool ShouldEventActivateButton(views::View* view, const ui::Event& event);

  // The shelf buttons use the Pointer interface to enable item reordering.
  enum Pointer { NONE, DRAG_AND_DROP, MOUSE, TOUCH };
  void PointerPressedOnButton(views::View* view,
                              Pointer pointer,
                              const ui::LocatedEvent& event);
  void PointerDraggedOnButton(views::View* view,
                              Pointer pointer,
                              const ui::LocatedEvent& event);
  void PointerReleasedOnButton(views::View* view,
                               Pointer pointer,
                               bool canceled);

  // Returns whether |item| should belong in the pinned section of the shelf.
  bool IsItemPinned(const ShelfItem& item) const;

  // Enumerates the shelf items that are centered in the new UI and returns
  // the total size they occupy.
  int GetDimensionOfCenteredShelfItems() const;

  // Returns the index of the item after which the separator should be shown,
  // or -1 if no separator is required.
  int GetSeparatorIndex() const;

  // Updates the background for the shelf items.
  void UpdateShelfItemBackground(SkColor color);

  // Update the layout when entering or exiting tablet mode. Have the owning
  // widget call this instead of observing changes ourselves to ensure this
  // happens after the tablet related changes in ShelfController.
  void OnTabletModeChanged();

  // True if the current |drag_view_| is the given |drag_view|.
  bool IsDraggedView(const ShelfButton* drag_view) const;

  // Returns the list of open windows that correspond to the app represented by
  // this shelf view.
  const std::vector<aura::Window*> GetOpenWindowsForShelfView(
      views::View* view);

  // Return the view model for test purposes.
  const views::ViewModel* view_model_for_test() const {
    return view_model_.get();
  }

  // Return the main shelf. This will return nullptr if this is not called on
  // the overflow shelf.
  ShelfView* main_shelf() { return main_shelf_; }

  const ShelfButton* drag_view() const { return drag_view_; }

  // Returns true when this ShelfView is used for Overflow Bubble.
  // In this mode, it does not show app list and overflow button.
  // Note:
  //   * When Shelf can contain only one item (the overflow button) due to very
  //     small resolution screen, the overflow bubble can show the app list
  //     button.
  bool is_overflow_mode() const { return overflow_mode_; }

  int first_visible_index() const { return first_visible_index_; }
  int last_visible_index() const { return last_visible_index_; }
  ShelfWidget* shelf_widget() const { return shelf_widget_; }
  OverflowBubble* overflow_bubble() { return overflow_bubble_.get(); }
  views::ViewModel* view_model() { return view_model_.get(); }

 private:
  friend class ShelfViewTestAPI;

  class FadeOutAnimationDelegate;
  class StartFadeAnimationDelegate;

  enum RemovableState {
    REMOVABLE,      // Item can be removed when dragged away.
    DRAGGABLE,      // Item can be dragged, but will snap always back to origin.
    NOT_REMOVABLE,  // Item is fixed and can never be removed.
  };

  // Minimum distance before drag starts.
  static const int kMinimumDragDistance;

  bool dragging() const { return drag_pointer_ != NONE; }

  // Sets the bounds of each view to its ideal bounds.
  void LayoutToIdealBounds();

  // Update all button's visibility in overflow.
  void UpdateAllButtonsVisibilityInOverflowMode();

  void LayoutAppListAndBackButtonHighlight() const;

  // Calculates the ideal bounds. The bounds of each button corresponding to an
  // item in the model is set in |view_model_|.
  void CalculateIdealBounds(gfx::Rect* overflow_bounds) const;

  // Returns the index of the last view whose max primary axis coordinate is
  // less than |max_value|. Returns -1 if nothing fits, or there are no views.
  int IndexOfLastItemThatFitsSize(int max_value) const;

  // Animates the bounds of each view to its ideal bounds.
  void AnimateToIdealBounds();

  // Creates the view used to represent |item|.
  views::View* CreateViewForItem(const ShelfItem& item);

  // Fades |view| from an opacity of 0 to 1. This is when adding a new item.
  void FadeIn(views::View* view);

  // Invoked when the pointer has moved enough to trigger a drag. Sets
  // internal state in preparation for the drag.
  void PrepareForDrag(Pointer pointer, const ui::LocatedEvent& event);

  // Invoked when the mouse is dragged. Updates the models as appropriate.
  void ContinueDrag(const ui::LocatedEvent& event);

  // Ends the drag on the other shelf. (ie if we are on main shelf, ends drag on
  // the overflow shelf). Invoked when a shelf item is being dragged from one
  // shelf to the other.
  void EndDragOnOtherShelf(bool cancel);

  // Handles ripping off an item from the shelf. Returns true when the item got
  // removed.
  bool HandleRipOffDrag(const ui::LocatedEvent& event);

  // Finalize the rip off dragging by either |cancel| the action or validating.
  void FinalizeRipOffDrag(bool cancel);

  // Check if an item can be ripped off or not.
  RemovableState RemovableByRipOff(int index) const;

  // Returns true if |typea| and |typeb| should be in the same drag range.
  bool SameDragType(ShelfItemType typea, ShelfItemType typeb) const;

  // Returns the range (in the model) the item at the specified index can be
  // dragged to.
  std::pair<int, int> GetDragRange(int index);

  // If there is a drag operation in progress it's canceled. If |modified_index|
  // is valid, the new position of the corresponding item is returned.
  int CancelDrag(int modified_index);

  // Returns rectangle bounds used for drag insertion.
  // Note:
  //  * When overflow button is visible, returns bounds from first item
  //    to overflow button.
  //  * In the overflow mode, returns only bubble's bounds.
  gfx::Rect GetBoundsForDragInsertInScreen();

  // Common setup done for all children.
  void ConfigureChildView(views::View* view);

  // Toggles the overflow menu.
  void ToggleOverflowBubble();

  // Invoked after the fading out animation for item deletion is ended.
  void OnFadeOutAnimationEnded();

  // Fade in last visible item.
  void StartFadeInLastVisibleItem();

  // Updates the visible range of overflow items in |overflow_view|.
  void UpdateOverflowRange(ShelfView* overflow_view) const;

  // Gets the menu anchor rect for menus. |source| is the view that is
  // asking for a menu, |location| is the location of the event, |context_menu|
  // is whether the menu is for a context or application menu.
  gfx::Rect GetMenuAnchorRect(const views::View& source,
                              const gfx::Point& location,
                              bool context_menu) const;

  // Gets the menu anchor position for a menu. |for_item| is true if the menu is
  // for an item on the shelf, or false if the menu is for the shelf view
  // itself, |context_menu| is whether the menu will be an application menu or
  // context menu, and |touch_menu| is whether the menu was initiated by touch.
  views::MenuAnchorPosition GetMenuAnchorPosition(bool for_item,
                                                  bool context_menu) const;

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  FocusTraversable* GetPaneFocusTraversable() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // Overridden from ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;

  // Overridden from ShelfModelObserver:
  void ShelfItemAdded(int model_index) override;
  void ShelfItemRemoved(int model_index, const ShelfItem& old_item) override;
  void ShelfItemChanged(int model_index, const ShelfItem& old_item) override;
  void ShelfItemMoved(int start_index, int target_index) override;
  void ShelfItemDelegateChanged(const ShelfID& id,
                                ShelfItemDelegate* old_delegate,
                                ShelfItemDelegate* delegate) override;
  void ShelfItemStatusChanged(const ShelfID& id) override;

  // Handles the result when querying ShelfItemDelegates for context menu items.
  // Shows a default shelf context menu with optional extra custom |menu_items|.
  void AfterGetContextMenuItems(const ShelfID& shelf_id,
                                const gfx::Point& point,
                                views::View* source,
                                ui::MenuSourceType source_type,
                                std::vector<mojom::MenuItemPtr> menu_items);

  // Handles the result of an item selection, records the |action| taken and
  // optionally shows an application menu with the given |menu_items|.
  void AfterItemSelected(
      const ShelfItem& item,
      views::Button* sender,
      std::unique_ptr<ui::Event> event,
      views::InkDrop* ink_drop,
      ShelfAction action,
      base::Optional<std::vector<mojom::MenuItemPtr>> menu_items);

  // Overridden from views::ContextMenuController:
  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override;

  // Show either a context or normal click menu of given |menu_model|.
  // If |context_menu| is set, the displayed menu is a context menu and not
  // a menu listing one or more running applications.
  // The |click_point| is only used for |context_menu|'s.
  void ShowMenu(std::unique_ptr<ui::SimpleMenuModel> menu_model,
                views::View* source,
                const gfx::Point& click_point,
                bool context_menu,
                ui::MenuSourceType source_type);

  // Callback for MenuRunner.
  void OnMenuClosed(views::View* source);

  // Overridden from views::BoundsAnimatorObserver:
  void OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) override;
  void OnBoundsAnimatorDone(views::BoundsAnimator* animator) override;

  // Returns true if the (press down) |event| is a repost event from an event
  // which just closed the menu of a shelf item. If it occurs on the same shelf
  // item, we should ignore the call.
  bool IsRepostEvent(const ui::Event& event);

  // Returns true if the given |item| is supposed to be shown to the user.
  bool ShouldShowShelfItem(const ShelfItem& item);

  // Convenience accessor to model_->items().
  const ShelfItem* ShelfItemForView(const views::View* view) const;

  // Get the distance from the given |coordinate| to the closest point on this
  // launcher/shelf.
  int CalculateShelfDistance(const gfx::Point& coordinate) const;

  bool CanPrepareForDrag(Pointer pointer, const ui::LocatedEvent& event);

  // Updates the back button opacity and focus behavior based on tablet mode.
  void UpdateBackButton();

  // Set background blur to the dragged image. |size| is the image size.
  void SetDragImageBlur(const gfx::Size& size, int blur_radius);

  // The model; owned by Launcher.
  ShelfModel* model_;

  // The shelf controller; owned by RootWindowController.
  Shelf* shelf_;

  // The shelf widget for this view. For overflow bubbles, this is the widget
  // for the shelf, not for the bubble.
  ShelfWidget* shelf_widget_;

  // Used to manage the set of active launcher buttons. There is a view per
  // item in |model_|.
  std::unique_ptr<views::ViewModel> view_model_;

  // Index of the first visible launcher item. This is not always zero because
  // the overflow view (also a kind of shelf view) only shows a subset of items.
  int first_visible_index_ = 0;

  // Last index of a launcher button that is visible
  // (does not go into overflow).
  mutable int last_visible_index_ = -1;

  std::unique_ptr<views::BoundsAnimator> bounds_animator_;

  OverflowButton* overflow_button_ = nullptr;

  std::unique_ptr<OverflowBubble> overflow_bubble_;

  OverflowBubble* owner_overflow_bubble_ = nullptr;

  ShelfTooltipManager tooltip_;

  // Pointer device that initiated the current drag operation. If there is no
  // current dragging operation, this is NONE.
  Pointer drag_pointer_ = NONE;

  // The view being dragged. This is set immediately when the mouse is pressed.
  // |dragging_| is set only if the mouse is dragged far enough.
  ShelfButton* drag_view_ = nullptr;

  // The view showing a context menu. This can be either a ShelfView or
  // ShelfButton.
  views::View* menu_owner_ = nullptr;

  // Position of the mouse down event in |drag_view_|'s coordinates.
  gfx::Point drag_origin_;

  // Index |drag_view_| was initially at.
  int start_drag_index_ = -1;

  // Used for the context menu of a particular item.
  ShelfID context_menu_id_;

  std::unique_ptr<views::FocusSearch> focus_search_;

  // Responsible for building and running all menus.
  std::unique_ptr<ShelfMenuModelAdapter> shelf_menu_model_adapter_;
  std::unique_ptr<ScopedRootWindowForNewWindows>
      scoped_root_window_for_new_windows_;

  // True when an item being inserted or removed in the model cancels a drag.
  bool cancelling_drag_model_changed_ = false;

  // The timestamp of the event which closed the last menu - or 0.
  base::TimeTicks closing_event_time_;

  // True if a drag and drop operation created/pinned the item in the launcher
  // and it needs to be deleted/unpinned again if the operation gets cancelled.
  bool drag_and_drop_item_pinned_ = false;

  // The ShelfItem currently used for drag and drop; empty if none.
  ShelfID drag_and_drop_shelf_id_;

  // The original launcher item's size before the dragging operation.
  gfx::Size pre_drag_and_drop_size_;

  // The image proxy for drag operations when a drag and drop host exists and
  // the item can be dragged outside the app grid.
  std::unique_ptr<ash::DragImageView> drag_image_;

  // The owner of a mask layer used to clip the background blur.
  std::unique_ptr<ui::LayerOwner> drag_image_mask_;

  // The cursor offset to the middle of the dragged item.
  gfx::Vector2d drag_image_offset_;

  // The view which gets replaced by our drag icon proxy.
  views::View* drag_replaced_view_ = nullptr;

  // True when the icon was dragged off the shelf.
  bool dragged_off_shelf_ = false;

  // True when an item is dragged from one shelf to another (eg. overflow).
  bool dragged_to_another_shelf_ = false;

  // The rip off view when a snap back operation is underway.
  views::View* snap_back_from_rip_off_view_ = nullptr;

  // True when this ShelfView is used for Overflow Bubble.
  bool overflow_mode_ = false;

  // Holds a pointer to main ShelfView when a ShelfView is in overflow mode.
  ShelfView* main_shelf_ = nullptr;

  // True when ripped item from overflow bubble is entered into Shelf.
  bool dragged_off_from_overflow_to_shelf_ = false;

  // True if the event is a repost event from a event which has just closed the
  // menu of the same shelf item.
  bool is_repost_event_on_same_item_ = false;

  // Record the index for the last pressed shelf item. This variable is used to
  // check if a repost event occurs on the same shelf item as previous one. If
  // so, the repost event should be ignored.
  int last_pressed_index_ = -1;

  // Tracks UMA metrics based on shelf button press actions.
  ShelfButtonPressedMetricTracker shelf_button_pressed_metric_tracker_;

  // Color used to paint the background behind the app list button and back
  // button.
  SkColor shelf_item_background_color_;

  // A reference to the view used as a separator between pinned and unpinned
  // items.
  views::Separator* separator_ = nullptr;

  // A view to draw a background behind the app list and back buttons.
  // Owned by the view hierarchy.
  views::View* back_and_app_list_background_ = nullptr;

  base::WeakPtrFactory<ShelfView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShelfView);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_VIEW_H_
