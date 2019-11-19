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
#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model_observer.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/shelf/overflow_bubble.h"
#include "ash/shelf/overflow_bubble_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ash/shelf/shelf_button_pressed_metric_tracker.h"
#include "ash/shelf/shelf_tooltip_delegate.h"
#include "ash/shell_observer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/focus/focus_manager.h"
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
class DragImageView;
class OverflowBubble;
class OverflowButton;
class ScopedRootWindowForNewWindows;
class ShelfAppButton;
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
// ------------------------------------------------------------
// | o |         | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
// ------------------------------------------------------------
//                 ^                                        ^
//                 |                                        |
//             first_visible_index = 0            last_visible_index = 10
//
// Where "o" is the home button (back button is hidden).
//
// If screen space is more constrained, some icons are placed in an overflow
// menu (which holds its own instance of ShelfView):
//
//                first_visible_index = 8        last_visible_index = 11
//                     (for the overflow)        (for overflow)
//                                     |             |
//                                     v             v
//                                   ---------------------
//                                   | 8 | 9 | 10 | 11 |
//                                   ---------------------
//                                             ^
// --------------------------------------------------
// | o |    | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | ... |
// --------------------------------------------------
//            ^                           ^    ^
//            |                           |    L-- overflow button
//     first_visible_index = 0            |
//      (for the main shelf)        last_visible_index = 7
//

class ASH_EXPORT ShelfView : public views::AccessiblePaneView,
                             public ShelfButtonDelegate,
                             public ShelfModelObserver,
                             public ShellObserver,
                             public views::ContextMenuController,
                             public views::BoundsAnimatorObserver,
                             public ApplicationDragAndDropHost,
                             public ShelfTooltipDelegate,
                             public ash::TabletModeObserver,
                             public ShelfConfig::Observer {
 public:
  ShelfView(ShelfModel* model,
            Shelf* shelf,
            ApplicationDragAndDropHost* drag_and_drop_host,
            ShelfButtonDelegate* delegate);
  ~ShelfView() override;

  Shelf* shelf() const { return shelf_; }
  ShelfModel* model() const { return model_; }

  // Returns the size occupied by |count| app icons. If |with_overflow| is
  // true, returns the size of |count| app icons followed by an overflow
  // button.
  static int GetSizeOfAppIcons(int count, bool with_overflow);

  // Initializes shelf view elements.
  void Init();

  // Returns the ideal bounds of the specified item, or an empty rect if id
  // isn't know. If the item is in an overflow shelf, the overflow icon location
  // will be returned.
  gfx::Rect GetIdealBoundsOfItemIcon(const ShelfID& id);

  // Returns true if we're showing a menu. Note the menu could be either the
  // context menu or the application select menu.
  bool IsShowingMenu() const;

  // Returns true if we're showing a menu for |view|. |view| could be a
  // ShelfAppButton or the ShelfView.
  bool IsShowingMenuForView(const views::View* view) const;

  // Returns true if overflow bubble is shown.
  bool IsShowingOverflowBubble() const;

  // Sets owner overflow bubble instance from which this shelf view pops
  // out as overflow.
  void set_owner_overflow_bubble(OverflowBubble* owner) {
    owner_overflow_bubble_ = owner;
  }

  OverflowButton* GetOverflowButton() const;

  // Updates the union of all the shelf item bounds shown by this shelf view.
  // This is used to determine the common area where the mouse can hover
  // for showing tooltips without stuttering over gaps.
  void UpdateVisibleShelfItemBoundsUnion();

  // ShelfTooltipDelegate:
  bool ShouldShowTooltipForView(const views::View* view) const override;
  bool ShouldHideTooltip(const gfx::Point& cursor_location) const override;
  const std::vector<aura::Window*> GetOpenWindowsForView(
      views::View* view) override;
  base::string16 GetTitleForView(const views::View* view) const override;
  views::View* GetViewForEvent(const ui::Event& event) override;

  // Toggles the overflow menu.
  void ToggleOverflowBubble();

  // Returns rectangle bounding all visible launcher items. Used screen
  // coordinate system.
  gfx::Rect GetVisibleItemsBoundsInScreen();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  FocusTraversable* GetPaneFocusTraversable() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  const char* GetClassName() const override;

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  View* GetTooltipHandlerForPoint(const gfx::Point& point) override;

  // ShelfButtonDelegate:
  void OnShelfButtonAboutToRequestFocusFromTabTraversal(ShelfButton* button,
                                                        bool reverse) override;
  void ButtonPressed(views::Button* sender,
                     const ui::Event& event,
                     views::InkDrop* ink_drop) override;

  // FocusTraversable:
  views::FocusSearch* GetFocusSearch() override;

  // AccessiblePaneView:
  views::View* GetDefaultFocusableChild() override;

  // ApplicationDragAndDropHost:
  void CreateDragIconProxy(const gfx::Point& location_in_screen_coordinates,
                           const gfx::ImageSkia& icon,
                           views::View* replaced_view,
                           const gfx::Vector2d& cursor_offset_from_center,
                           float scale_factor) override;

  // Overridden from views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // ash::TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // ShelfConfig::Observer:
  void OnShelfConfigUpdated() override;

  // Returns true if |event| on the shelf item is going to activate the
  // ShelfItem associated with |view|. Used to determine whether a pending ink
  // drop should be shown or not.
  bool ShouldEventActivateButton(views::View* view, const ui::Event& event);

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

  // Swaps the given button with the next one if |with_next| is true, or with
  // the previous one if |with_next| is false.
  void SwapButtons(views::View* button_to_swap, bool with_next);

  // The ShelfAppButtons use the Pointer interface to enable item reordering.
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

  // Update the layout when entering or exiting tablet mode. Have the owning
  // widget call this instead of observing changes ourselves to ensure this
  // happens after the tablet related changes in ShelfController.
  void OnTabletModeChanged();

  // True if the current |drag_view_| is the given |drag_view|.
  bool IsDraggedView(const views::View* drag_view) const;

  // The three methods below return the first or last focusable child of the
  // set including both the main shelf and the overflow shelf it it's showing.
  // - The first focusable child is either the home button, or the back
  //   button in tablet mode.
  // - The last focusable child can be either 1) the last app icon on the main
  //   shelf if there aren't enough apps to overflow, 2) the overflow button
  //   if it's visible but the overflow bubble isn't showing, or 3) the last
  //   app icon in the overflow bubble if it's showing.
  views::View* FindFirstOrLastFocusableChild(bool last);
  views::View* FindFirstFocusableChild();
  views::View* FindLastFocusableChild();

  // Handles the gesture event. Returns true if |event| has been consumed.
  bool HandleGestureEvent(const ui::GestureEvent* event);

  // Different from ShouldShowTooltipForView, |view| here must be a child view.
  bool ShouldShowTooltipForChildView(const views::View* child_view) const;

  // Returns the ShelfAppButton associated with |id|.
  ShelfAppButton* GetShelfAppButton(const ShelfID& id);

  // Return the view model for test purposes.
  const views::ViewModel* view_model_for_test() const {
    return view_model_.get();
  }

  // Returns the main shelf. This can be called on either the main shelf
  // or the overflow shelf.
  ShelfView* main_shelf() { return main_shelf_ ? main_shelf_ : this; }

  // Returns the overflow shelf. This can be called on either the main shelf
  // or the overflow shelf. Returns nullptr if the overflow shelf isn't visible.
  ShelfView* overflow_shelf() {
    return const_cast<ShelfView*>(
        const_cast<const ShelfView*>(this)->overflow_shelf());
  }

  const ShelfView* overflow_shelf() const {
    if (is_overflow_mode())
      return this;
    return IsShowingOverflowBubble()
               ? overflow_bubble_->bubble_view()->shelf_view()
               : nullptr;
  }

  void set_default_last_focusable_child(bool default_last_focusable_child) {
    default_last_focusable_child_ = default_last_focusable_child;
  }

  void set_app_icons_layout_offset(int app_icons_layout_offset) {
    app_icons_layout_offset_ = app_icons_layout_offset;
  }

  const ShelfAppButton* drag_view() const { return drag_view_; }

  // Returns true when this ShelfView is used for Overflow Bubble.
  // In this mode, it does not show app list and overflow button.
  // Note:
  //   * When Shelf can contain only one item (the overflow button) due to very
  //     small resolution screen, the overflow bubble can show the app list
  //     button.
  bool is_overflow_mode() const { return overflow_mode_; }

  int first_visible_index() const { return first_visible_index_; }
  int last_visible_index() const { return last_visible_index_; }
  int number_of_visible_apps() const {
    if (is_overflow_mode())
      return std::max(0, last_visible_index_ - first_visible_index_ + 1);
    else
      return std::max(0, last_visible_index_ + 1);
  }
  views::View* first_visible_button_for_testing() {
    return view_model_->view_at(first_visible_index());
  }
  ShelfWidget* shelf_widget() const { return shelf_->shelf_widget(); }
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

  struct AppCenteringStrategy {
    bool center_on_screen = false;
    bool overflow = false;
  };

  // Minimum distance before drag starts.
  static const int kMinimumDragDistance;

  // Common setup done for all children views.
  static void ConfigureChildView(views::View* view);

  bool dragging() const { return drag_pointer_ != NONE; }

  // Calculates the ideal bounds of shelf elements.
  // The bounds of each button corresponding to an item in the model is set in
  // |view_model_|.
  void CalculateIdealBounds();

  // Creates the view used to represent given shelf |item|.
  // Returns unowned pointer (view is owned by the view hierarchy).
  views::View* CreateViewForItem(const ShelfItem& item);

  // Updates the visible range of overflow items in |overflow_view|.
  void UpdateOverflowRange(ShelfView* overflow_view) const;

  // Returns the size that's actually available for app icons. Size occupied
  // by the home button and back button plus all appropriate margins is
  // not available for app icons.
  int GetAvailableSpaceForAppIcons() const;

  // Returns the index of the item after which the separator should be shown,
  // or -1 if no separator is required.
  int GetSeparatorIndex() const;

  // This method determines which centering strategy is adequate, returns that,
  // and sets the |first_visible_index_| and |last_visible_index_| fields
  // appropriately.
  AppCenteringStrategy CalculateAppCenteringStrategy();

  // Update all buttons' visibility in overflow.
  void UpdateAllButtonsVisibilityInOverflowMode();

  // Sets the bounds of each view to its ideal bounds.
  void LayoutToIdealBounds();

  void LayoutBackAndHomeButtons();
  void LayoutOverflowButton() const;

  // Returns the index of the last view whose max primary axis coordinate is
  // less than |max_value|. Returns -1 if nothing fits, or there are no views.
  int IndexOfLastItemThatFitsSize(int max_value) const;

  // Animates the bounds of each view to its ideal bounds.
  void AnimateToIdealBounds();

  // Fades |view| from an opacity of 0 to 1. This is when adding a new item.
  void FadeIn(views::View* view);

  // Invoked when the pointer has moved enough to trigger a drag. Sets
  // internal state in preparation for the drag.
  void PrepareForDrag(Pointer pointer, const ui::LocatedEvent& event);

  // Invoked when the mouse is dragged. Updates the models as appropriate.
  void ContinueDrag(const ui::LocatedEvent& event);

  // Scroll the view to show more content in the direction of the user's drag.
  void ScrollForUserDrag(int offset);

  // Increase the speed of an existing scroll.
  void SpeedUpDragScrolling();

  // Reorder |drag_view_| according to the latest dragging coordinate.
  void MoveDragViewTo(int primary_axis_coordinate);

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

  // Returns true if focus should move out of the ShelfView view tree.
  bool ShouldFocusOut(bool reverse, views::View* button);

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

  // Invoked after the fading out animation for item deletion is ended.
  void OnFadeOutAnimationEnded();

  // Fade in last visible item.
  void StartFadeInLastVisibleItem();

  // Gets the menu anchor rect for menus. |source| is the view that is
  // asking for a menu, |location| is the location of the event, |context_menu|
  // is whether the menu is for a context or application menu.
  gfx::Rect GetMenuAnchorRect(const views::View& source,
                              const gfx::Point& location,
                              bool context_menu) const;

  void AnnounceShelfAlignment();
  void AnnounceShelfAutohideBehavior();

  // Overridden from ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Overridden from ShelfModelObserver:
  void ShelfItemAdded(int model_index) override;
  void ShelfItemRemoved(int model_index, const ShelfItem& old_item) override;
  void ShelfItemChanged(int model_index, const ShelfItem& old_item) override;
  void ShelfItemMoved(int start_index, int target_index) override;
  void ShelfItemDelegateChanged(const ShelfID& id,
                                ShelfItemDelegate* old_delegate,
                                ShelfItemDelegate* delegate) override;
  void ShelfItemStatusChanged(const ShelfID& id) override;

  // Overridden from ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window) override;
  void OnShelfAutoHideBehaviorChanged(aura::Window* root_window) override;

  // Shows a shelf context menu with the given |model|, or a default menu.
  void ShowShelfContextMenu(const ShelfID& shelf_id,
                            const gfx::Point& point,
                            views::View* source,
                            ui::MenuSourceType source_type,
                            std::unique_ptr<ui::SimpleMenuModel> model);

  // Handles the result of an item selection, records the |action| taken and
  // optionally shows an application menu with the given |menu_items|.
  void AfterItemSelected(const ShelfItem& item,
                         views::Button* sender,
                         std::unique_ptr<ui::Event> event,
                         views::InkDrop* ink_drop,
                         ShelfAction action,
                         ShelfItemDelegate::AppMenuItems menu_items);

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

  // Set background blur to the dragged image. |size| is the image size.
  void SetDragImageBlur(const gfx::Size& size, int blur_radius);

  bool ShouldHandleGestures(const ui::GestureEvent& event) const;

  // Different from GetTitleForView, |view| here must be a child view.
  base::string16 GetTitleForChildView(const views::View* view) const;

  // Update |first_visible_index_| and |last_visible_index_| when the scrollable
  // shelf is enabled.
  void UpdateVisibleIndice();

  // The model; owned by Launcher.
  ShelfModel* model_;

  // The shelf controller; owned by RootWindowController.
  Shelf* shelf_;

  // Used to manage the set of active launcher buttons. There is a view per
  // item in |model_|.
  std::unique_ptr<views::ViewModel> view_model_;

  // Index of the first visible app item. This is either:
  // * -1 if there are no apps.
  // * 0 if there is at least one app.
  // > 0 when this shelf view is the overflow shelf view and only shows a
  //   subset of items.
  int first_visible_index_ = -1;

  // Last index of an app launcher button that is visible (does not go into
  // overflow), or -1 if there are no apps (or if only the overflow button is
  // visible).
  int last_visible_index_ = -1;

  std::unique_ptr<views::BoundsAnimator> bounds_animator_;

  OverflowButton* overflow_button_ = nullptr;

  std::unique_ptr<OverflowBubble> overflow_bubble_;

  OverflowBubble* owner_overflow_bubble_ = nullptr;

  // Pointer device that initiated the current drag operation. If there is no
  // current dragging operation, this is NONE.
  Pointer drag_pointer_ = NONE;

  // The view being dragged. This is set immediately when the mouse is pressed.
  // |dragging_| is set only if the mouse is dragged far enough.
  ShelfAppButton* drag_view_ = nullptr;

  // The view showing a context menu. This can be either a ShelfView or
  // ShelfAppButton.
  views::View* menu_owner_ = nullptr;

  // A reference to the view used as a separator between pinned and unpinned
  // items.
  views::Separator* separator_ = nullptr;

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

  // The item with an in-flight async request for a context menu or selection
  // (which shows a shelf item application menu if multiple windows are open).
  // Used to avoid multiple concurrent menu requests. The value is null if none.
  ShelfID item_awaiting_response_;

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

  // The union of all visible shelf item bounds. Used for showing tooltips in
  // a continuous manner.
  gfx::Rect visible_shelf_item_bounds_union_;

  // A view used to make accessibility announcements (changes in the shelf's
  // alignment or auto-hide state).
  views::View* announcement_view_ = nullptr;  // Owned by ShelfView

  // For dragging: -1 if scrolling back, 1 if scrolling forward, 0 if neither.
  int drag_scroll_dir_ = 0;

  // Used to periodically call ScrollForUserDrag.
  base::RepeatingTimer scrolling_timer_;

  // Used to call SpeedUpDragScrolling.
  base::OneShotTimer speed_up_drag_scrolling_;

  // The AppListViewState recorded before a button press, used to record app
  // launching metrics. This allows an accurate AppListViewState to be recorded
  // before AppListViewState changes.
  ash::AppListViewState recorded_app_list_view_state_;

  // Whether the applist was shown before a button press, used to record app
  // launching metrics. This is recorded because AppList visibility can change
  // before the metric is recorded.
  bool app_list_visibility_before_app_launch_ = false;

  // Whether this view should focus its last focusable child (instead of its
  // first) when focused.
  bool default_last_focusable_child_ = false;

  // Indicates the starting position of shelf items on the main axis. (Main
  // axis is x-axis when the shelf is horizontally aligned; otherwise, it
  // becomes y-axis)
  int app_icons_layout_offset_ = 0;

  // When the scrollable shelf is enabled, |drag_and_drop_host_| should be
  // ScrollableShelfView.
  ApplicationDragAndDropHost* drag_and_drop_host_ = nullptr;

  // When the scrollable shelf is enabled, |shelf_button_delegate_| should
  // be ScrollableShelfView.
  ShelfButtonDelegate* shelf_button_delegate_ = nullptr;

  base::WeakPtrFactory<ShelfView> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ShelfView);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_VIEW_H_
