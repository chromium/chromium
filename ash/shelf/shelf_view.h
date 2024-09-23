// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_VIEW_H_
#define ASH_SHELF_SHELF_VIEW_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/views/app_drag_icon_proxy.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model_observer.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ash/shelf/shelf_button_pressed_metric_tracker.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/shelf/shelf_tooltip_delegate.h"
#include "ash/shell_observer.h"
#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view_model.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ui {
class SimpleMenuModel;
}

namespace display {
class ScopedDisplayForNewWindows;
enum class TabletState;
}  // namespace display

namespace views {
class BoundsAnimator;
class MenuRunner;
class Separator;
}  // namespace views

namespace ash {
class GhostImageView;
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
class ASH_EXPORT ShelfView : public views::AccessiblePaneView,
                             public ShelfButtonDelegate,
                             public ShelfModelObserver,
                             public ShellObserver,
                             public ShelfObserver,
                             public views::ContextMenuController,
                             public views::BoundsAnimatorObserver,
                             public ShelfTooltipDelegate {
  METADATA_HEADER(ShelfView, views::AccessiblePaneView)

 public:
  // Used to communicate with the container class ScrollableShelfView.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called on each shelf item drag update to update the parent view scroll
    // offset if needed.
    virtual void ScheduleScrollForItemDragIfNeeded(
        const gfx::Rect& item_bounds_in_screen) = 0;

    // Called when a shelf item drag ends to cancel any parent view scroll
    // offset updates previously scheduled by
    // `ScheduleDcrollForItemDragIfNeeded()`.
    virtual void CancelScrollForItemDrag() = 0;

    // Returns whether the provided `bounds_in_screen` are within visible
    // portion of the shelf.
    virtual bool AreBoundsWithinVisibleSpace(
        const gfx::Rect& bounds_in_screen) const = 0;
  };

  ShelfView(ShelfModel* model,
            Shelf* shelf,
            Delegate* delegate,
            ShelfButtonDelegate* button_delegate);

  ShelfView(const ShelfView&) = delete;
  ShelfView& operator=(const ShelfView&) = delete;

  ~ShelfView() override;

  Shelf* shelf() const { return shelf_; }
  ShelfModel* model() const { return model_; }

  // Returns the size occupied by |count| app buttons. |button_size| indicates
  // the size of each app button.
  int GetSizeOfAppButtons(int count, int button_size);

  // Initializes shelf view elements.
  void Init(views::FocusSearch* focus_search);

  // Returns true if we're showing a menu. Note the menu could be either the
  // context menu or the application select menu.
  bool IsShowingMenu() const;

  // Returns true if we're showing a menu for |view|. |view| could be a
  // ShelfAppButton or the ShelfView.
  bool IsShowingMenuForView(const views::View* view) const;

  // Updates the union of all the shelf item bounds shown by this shelf view.
  // This is used to determine the common area where the mouse can hover
  // for showing tooltips without stuttering over gaps.
  void UpdateVisibleShelfItemBoundsUnion();

  // Returns true if the given location is within the bounds of all visiable app
  // icons. Used for tool tip visibility and scrolling event propogation.
  bool LocationInsideVisibleShelfItemBounds(const gfx::Point& location) const;

  // ShelfTooltipDelegate:
  bool ShouldShowTooltipForView(const views::View* view) const override;
  bool ShouldHideTooltip(const gfx::Point& cursor_location,
                         views::View* delegate_view) const override;
  const std::vector<aura::Window*> GetOpenWindowsForView(
      views::View* view) override;
  std::u16string GetTitleForView(const views::View* view) const override;
  views::View* GetViewForEvent(const ui::Event& event) override;

  // Returns rectangle bounding all visible launcher items. Used screen
  // coordinate system.
  gfx::Rect GetVisibleItemsBoundsInScreen();

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  gfx::Rect GetAnchorBoundsInScreen() const override;
  void OnThemeChanged() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  FocusTraversable* GetPaneFocusTraversable() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;

  View* GetTooltipHandlerForPoint(const gfx::Point& point) override;

  bool CanDrop(const OSExchangeData& data) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  DropCallback GetDropCallback(const ui::DropTargetEvent& event) override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;

  void EndDragCallback(
      const ui::DropTargetEvent& event,
      ui::mojom::DragOperation& output_drag_op,
      std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

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

  // Overridden from views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // Called from ScrollableShelfView when shelf config is updated.
  void OnShelfConfigUpdated();

  // Returns true if |event| on the shelf item is going to activate the
  // ShelfItem associated with |view|. Used to determine whether a pending ink
  // drop should be shown or not.
  bool ShouldEventActivateButton(views::View* view, const ui::Event& event);

  bool ShouldHandleDrag(const std::string& app_id,
                        const gfx::Point& location_in_screen) const;
  bool StartDrag(const std::string& app_id,
                 const gfx::Point& location_in_screen,
                 const gfx::Rect& drag_icon_bounds_in_screen);
  bool Drag(const gfx::Point& location_in_screen,
            const gfx::Rect& drag_icon_bounds_in_screen);
  void EndDrag(bool cancel);

  // Swaps the given button with the next one if |with_next| is true, or with
  // the previous one if |with_next| is false.
  void SwapButtons(views::View* button_to_swap, bool with_next);

  // The ShelfAppButtons use the Pointer interface to enable item reordering.
  enum Pointer { NONE, DRAG_AND_DROP, MOUSE, TOUCH };
  void PointerPressedOnButton(views::View* view,
                              Pointer pointer,
                              const ui::LocatedEvent& event);
  void PointerDraggedOnButton(const views::View* view,
                              Pointer pointer,
                              const ui::LocatedEvent& event);
  void PointerReleasedOnButton(const views::View* view,
                               Pointer pointer,
                               bool canceled);

  // Returns whether |item| should belong in the pinned section of the shelf.
  bool IsItemPinned(const ShelfItem& item) const;

  // Returns whether |item| should be visible or hidden.
  bool IsItemVisible(const ShelfItem& item) const;

  // Update the layout when entering or exiting tablet mode. Have the owning
  // widget call this instead of observing changes ourselves to ensure this
  // happens after the tablet related changes in ShelfController.
  void OnTabletModeChanged();

  // True if the current |drag_view_| is the given |drag_view|.
  bool IsDraggedView(const views::View* drag_view) const;

  // These three methods return the first or last focuable child of the whole
  // shelf view.
  views::View* FindFirstOrLastFocusableChild(bool last);
  views::View* FindFirstFocusableChild();
  views::View* FindLastFocusableChild();

  // Handles the gesture event. Returns true if |event| has been consumed.
  bool HandleGestureEvent(const ui::GestureEvent* event);

  // Different from ShouldShowTooltipForView, |view| here must be a child view.
  bool ShouldShowTooltipForChildView(const views::View* child_view) const;

  // Returns the ShelfAppButton associated with |id|.
  ShelfAppButton* GetShelfAppButton(const ShelfID& id);

  // Updates the visibility of the views of the shelf items and the
  // |visible_views_indices_|.
  void UpdateShelfItemViewsVisibility();

  // If there is animation associated with |view| in |bounds_animator_|,
  // stops the animation.
  void StopAnimatingViewIfAny(views::View* view);

  // Returns the the shelf button size.
  int GetButtonSize() const;

  // Returns the size of a shelf button icon.
  int GetButtonIconSize() const;

  // Returns the size of a shelf button shortcut icon.
  int GetShortcutIconSize() const;

  // Returns the size of a shelf button shortcut icon border.
  int GetShelfShortcutIconContainerSize() const;

  // Returns the size of a shelf button shortcut host badge icon.
  int GetShelfShortcutHostBadgeIconSize() const;

  // Returns the size of a shelf button shortcut host badge icon border.
  int GetShelfShortcutHostBadgeContainerSize() const;

  // Returns the size of a shelf button shortcut bottom right corner radius.
  int GetShelfShortcutTeardropCornerRadiusSize() const;

  // Returns the size of the shelf item ripple ring.
  int GetShelfItemRippleSize() const;

  // If |app_icons_layout_offset_| is outdated, re-layout children to ideal
  // bounds.
  void LayoutIfAppIconsOffsetUpdates();

  // Returns the app button whose context menu is shown. Returns nullptr if no
  // app buttons have a context menu showing.
  ShelfAppButton* GetShelfItemViewWithContextMenu();

  // Modifies the announcement view to verbalize that the focused app button has
  // new updates, based on the item having a notification badge.
  void AnnounceShelfItemNotificationBadge(views::View* button);

  // Returns whether `bounds_animator_` is animating any view.
  bool IsAnimating() const;

  // If a shelf icon is being dragged (and drag icon proxy is created), returns
  // the drag icon bounds in screen coordinate. Otherwise, returns empty bounds.
  gfx::Rect GetDragIconBoundsInScreenForTest() const;

  // Return the view model for test purposes.
  views::ViewModel* view_model_for_test() { return view_model_.get(); }

  void set_default_last_focusable_child(bool default_last_focusable_child) {
    default_last_focusable_child_ = default_last_focusable_child;
  }

  ui::LayerTreeOwner* drag_image_layer_for_test() {
    return drag_image_layer_.get();
  }

  ShelfAppButton* drag_view() { return drag_view_; }

  const std::vector<size_t>& visible_views_indices() const {
    return visible_views_indices_;
  }
  size_t number_of_visible_apps() const {
    return visible_views_indices_.size();
  }
  ShelfWidget* shelf_widget() const { return shelf_->shelf_widget(); }
  const views::ViewModel* view_model() const { return view_model_.get(); }
  ShelfID drag_and_drop_shelf_id() const { return drag_and_drop_shelf_id_; }

  views::View* first_visible_button_for_testing() {
    DCHECK(!visible_views_indices_.empty());
    return view_model_->view_at(visible_views_indices_[0]);
  }

  ShelfMenuModelAdapter* shelf_menu_model_adapter_for_testing() {
    return shelf_menu_model_adapter_.get();
  }

  std::optional<size_t> current_ghost_view_index() const {
    return current_ghost_view_index_;
  }

  void AddAnimationObserver(views::BoundsAnimatorObserver* observer);
  void RemoveAnimationObserver(views::BoundsAnimatorObserver* observer);

 private:
  friend class ShelfViewTestAPI;

  class FadeInAnimationDelegate;
  class FadeOutAnimationDelegate;
  class StartFadeAnimationDelegate;
  class ViewOpacityResetter;

  enum RemovableState {
    REMOVABLE,      // Item can be removed when dragged away.
    DRAGGABLE,      // Item can be dragged, but will snap always back to origin.
    NOT_REMOVABLE,  // Item is fixed and can never be removed.
  };

  // Minimum distance before drag starts.
  static const int kMinimumDragDistance;

  // Updates relevant fields of the button and reflects the item status has
  // changed.
  void UpdateButton(ShelfAppButton* button, const ShelfItem& item);

  // Common setup done for all children views. |layer_type| specifies the type
  // of layer for the |view|. Use ui::LAYER_NOT_DRAWN if the content of the view
  // do not have to be painted (e.g. a container for views that have its own
  // texture layer).
  static void ConfigureChildView(views::View* view, ui::LayerType layer_type);

  bool dragging() const { return drag_pointer_ != NONE; }

  // Calculates the ideal bounds of shelf elements.
  // The bounds of each button corresponding to an item in the model is set in
  // |view_model_|.
  void CalculateIdealBounds();

  // Creates the view used to represent given shelf |item|.
  // Returns unowned pointer (view is owned by the view hierarchy).
  views::View* CreateViewForItem(const ShelfItem& item);

  // Returns the size that's actually available for app icons. Size occupied
  // by the home button and back button plus all appropriate margins is
  // not available for app icons.
  int GetAvailableSpaceForAppIcons() const;

  // Updates the index of the separator and save it to |separator_index_|.
  void UpdateSeparatorIndex();

  // Sets the bounds of each view to its ideal bounds.
  void LayoutToIdealBounds();

  // Returns the index of the last view whose max primary axis coordinate is
  // less than |max_value|. Returns -1 if nothing fits, or there are no views.
  int IndexOfLastItemThatFitsSize(int max_value) const;

  // Animates the bounds of each view to its ideal bounds.
  void AnimateToIdealBounds();

  // Animates the separator to its ideal bounds if `animate` is true, or sets
  // the bounds directly otherwise.
  void UpdateSeparatorBounds(bool animate);

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

  // Called when drag icon proxy closure animation completes.
  // Runs `opacity_resetter` which is expected to reset the drag view opacity to
  // visible state (as drag view may get hidden while shelf item drag is in
  // progress).
  void OnDragIconProxyAnimatedOut(
      std::unique_ptr<ViewOpacityResetter> opacity_resetter);

  // Handles ripping off an item from the shelf.
  void HandleRipOffDrag(const ui::LocatedEvent& event);

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
  std::pair<size_t, size_t> GetDragRange(size_t index);

  // Checks if the item at |dragged_item_index| should be pinned on pointer
  // release.
  bool ShouldUpdateDraggedViewPinStatus(size_t dragged_item_index);

  // Checks if |dragged_view| is allowed to be dragged across the separator to
  // perform pinning. Note that this function doesn't check if the separator
  // exists.
  bool CanDragAcrossSeparator(views::View* dragged_view) const;

  // If there is a drag operation in progress it's canceled. If |modified_index|
  // is valid, the new position of the corresponding item is returned.
  std::optional<size_t> CancelDrag(std::optional<size_t> modified_index);

  // Returns rectangle bounds used for drag insertion.
  gfx::Rect GetBoundsForDragInsertInScreen();

  // Invoked after the fading in animation for item addition is ended.
  void OnFadeInAnimationEnded();

  // Invoked after the fading out animation for item deletion is ended.
  void OnFadeOutAnimationEnded();

  // Gets the menu anchor rect for menus. |source| is the view that is
  // asking for a menu, |location| is the location of the event, |context_menu|
  // is whether the menu is for a context or application menu.
  gfx::Rect GetMenuAnchorRect(const views::View& source,
                              const gfx::Point& location,
                              bool context_menu) const;

  void AnnounceShelfAlignment();
  void AnnounceShelfAutohideBehavior();
  void AnnouncePinUnpinEvent(const ShelfItem& item, bool pinned);
  void AnnounceSwapEvent(const ShelfItem& first_item,
                         const ShelfItem& second_item);

  // Overridden from ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Overridden from ShelfModelObserver:
  void ShelfItemAdded(int model_index) override;
  void ShelfItemRemoved(int model_index, const ShelfItem& old_item) override;
  void ShelfItemChanged(int model_index, const ShelfItem& old_item) override;
  void ShelfItemsUpdatedForDeskChange() override;
  void ShelfItemMoved(int start_index, int target_index) override;
  void ShelfItemDelegateChanged(const ShelfID& id,
                                ShelfItemDelegate* old_delegate,
                                ShelfItemDelegate* delegate) override;
  void ShelfItemStatusChanged(const ShelfID& id) override;
  void ShelfItemRippedOff() override;
  void ShelfItemReturnedFromRipOff(int index) override;

  // Overridden from ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;

  // ShelfObserver:
  void OnShelfAutoHideBehaviorChanged() override;

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
  // |source| is either a ShelfView or a ShelfAppButton.
  // If |context_menu| is set, the displayed menu is a context menu and not
  // a menu listing one or more running applications.
  // The |click_point| is only used for |context_menu|'s.
  void ShowMenu(std::unique_ptr<ui::SimpleMenuModel> menu_model,
                views::View* source,
                const ShelfID& shelf_id,
                const gfx::Point& click_point,
                bool context_menu,
                ui::MenuSourceType source_type);

  // Callback for MenuRunner.
  // |source| is either a ShelfView or a ShelfAppButton.
  void OnMenuClosed(MayBeDangling<views::View> source);

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

  bool ShouldHandleGestures(const ui::GestureEvent& event) const;

  void DestroyScopedDisplay();

  // Different from GetTitleForView, |view| here must be a child view.
  std::u16string GetTitleForChildView(const views::View* view) const;

  int CalculateAppIconsLayoutOffset() const;

  // Returns the bounds of the given |child| view taken into account RTL layouts
  // and on-going bounds animations on |child|.
  gfx::Rect GetChildViewTargetMirroredBounds(const views::View* child) const;

  // Removes and reset |current_ghost_view| and |last_ghost_view|.
  void RemoveGhostView();

  // Resets the data members related to the app item context menu model request.
  void ResetActiveMenuModelRequest();

  // Calculate the drop target bounds in the screen for the current `drag_view_`
  // according to the target position in the shelf.
  gfx::Rect CalculateDropTargetBoundsForDragViewInScreen();

  // Runs animation for a `drag_icon_proxy_` to the provided
  // `target_bounds_in_screen`.
  void AnimateDragIconProxy(const gfx::Rect& target_bounds_in_screen);

  // Runs animation for a `drag_image_layer_` to the provided
  // `target_bounds_in_screen`.
  void AnimateDragImageLayer(const gfx::Rect& target_bounds_in_screen);

  using PendingPromiseAppsMap = std::map<std::string, ui::ImageModel>;

  // Register the app as a promise app with pending removal. The promise app
  // item is expected to be imminently replaced by the app item installed from
  // the promised package.
  // `promise_icon` - the icon image from the promise app. It will be passed to
  // the installed app item as a fallback icon, that will be shown while the
  // actual app icon is loading (to prevent a flash from an empty app icon).
  void AddPendingPromiseAppRemoval(const std::string& id,
                                   const ui::ImageModel& promise_icon);

  // Animate the transition of an incoming app if there was previously a promise
  // app in place.
  void AnimateTransitionForPromiseApps(views::View* view,
                                       ui::Layer* promise_app_layer,
                                       base::OnceClosure callback);

  // Called when the transition animation between apps is done.
  void FinishAnimationForPromiseApps(const std::string& pending_app_id);

  // Duplicates the layer for the `promise_app_view` and adds it to
  // `pending_promise_apps_removals_` if an animation would be required for it
  // on the future.
  void MaybeDuplicatePromiseAppForRemoval(ShelfAppButton* promise_app_view,
                                          const ShelfItem& item);

  // The model; owned by Launcher.
  const raw_ptr<ShelfModel> model_;

  // The shelf controller; owned by RootWindowController.
  const raw_ptr<Shelf> shelf_;

  // Used to manage the set of active launcher buttons. There is a view per
  // item in |model_|.
  std::unique_ptr<views::ViewModel> view_model_;

  // The indices of the views in |view_model_| that are visible.
  std::vector<size_t> visible_views_indices_;

  // Pointer device that initiated the current drag operation. If there is no
  // current dragging operation, this is NONE.
  Pointer drag_pointer_ = NONE;

  // The view being dragged. This is set immediately when the mouse is pressed.
  // |dragging_| is set only if the mouse is dragged far enough.
  raw_ptr<ShelfAppButton> drag_view_ = nullptr;

  // A reference to the view used as a separator between pinned and unpinned
  // items.
  raw_ptr<views::Separator> separator_ = nullptr;

  // Index of |separator_|. It is set to nullopt if it is invisible.
  std::optional<size_t> separator_index_ = std::nullopt;

  // Used in |drag_view_relative_to_ideal_bounds_| to represent the relative
  // position between |drag_view_| and its ideal bounds in shelf.
  enum class RelativePosition {
    // Set if |drag_view_| is not available or the relative position is not
    // calculated yet.
    kNotAvailable,
    // Set if |drag_view_| is to the left of its ideal bounds.
    kLeft,
    // Set if |drag_view_| is to the right of its ideal bounds.
    kRight
  };

  // The |drag_view_|'s current position relative to its ideal bounds.
  RelativePosition drag_view_relative_to_ideal_bounds_ =
      RelativePosition::kNotAvailable;

  // Position of the mouse down event in |drag_view_|'s coordinates.
  gfx::Point drag_origin_;

  // Index |drag_view_| was initially at.
  std::optional<size_t> start_drag_index_ = std::nullopt;

  // Used for the context menu of a particular item.
  ShelfID context_menu_id_;

  // Responsible for building and running all menus.
  std::unique_ptr<ShelfMenuModelAdapter> shelf_menu_model_adapter_;

  // Created when a shelf icon is pressed, so that new windows will be on the
  // same display as the press event.
  std::unique_ptr<display::ScopedDisplayForNewWindows>
      scoped_display_for_new_windows_;

  // True when an item being inserted or removed in the model cancels a drag.
  bool cancelling_drag_model_changed_ = false;

  // The item with an in-flight async request for a context menu or selection
  // (which shows a shelf item application menu if multiple windows are open).
  // Used to avoid multiple concurrent menu requests. The value is null if none.
  ShelfID item_awaiting_response_;

  // The callback for in-flight async request for a context menu.
  // Used to cancel the request if context menu should be
  // cancelled, for example if shelf item drag starts.
  base::CancelableOnceCallback<void(std::unique_ptr<ui::SimpleMenuModel> model)>
      context_menu_callback_;

  // The timestamp of the event which closed the last menu - or 0.
  base::TimeTicks closing_event_time_;

  // True if a drag and drop operation created/pinned the item in the launcher
  // and it needs to be deleted/unpinned again if the operation gets cancelled.
  bool drag_and_drop_item_pinned_ = false;

  // The ShelfItem currently used for drag and drop; empty if none.
  ShelfID drag_and_drop_shelf_id_;

  // Last received drag icon bounds during drag and drop received using
  // `ApplicationDragAndDropHost` interface.
  gfx::Rect drag_icon_bounds_in_screen_;

  // The original launcher item's size before the dragging operation.
  gfx::Size pre_drag_and_drop_size_;

  // True when the icon was dragged off the shelf.
  bool dragged_off_shelf_ = false;

  // The rip off view when a snap back operation is underway.
  raw_ptr<ShelfAppButton> snap_back_from_rip_off_view_ = nullptr;

  // True if the event is a repost event from a event which has just closed the
  // menu of the same shelf item.
  bool is_repost_event_on_same_item_ = false;

  // Record the index for the last pressed shelf item. This variable is used to
  // check if a repost event occurs on the same shelf item as previous one. If
  // so, the repost event should be ignored.
  std::optional<size_t> last_pressed_index_ = std::nullopt;

  // Tracks UMA metrics based on shelf button press actions.
  ShelfButtonPressedMetricTracker shelf_button_pressed_metric_tracker_;

  // The union of all visible shelf item bounds. Used for showing tooltips in
  // a continuous manner.
  gfx::Rect visible_shelf_item_bounds_union_;

  // A view used to make accessibility announcements (changes in the shelf's
  // alignment or auto-hide state).
  raw_ptr<views::View> announcement_view_ = nullptr;  // Owned by ShelfView

  // For dragging: -1 if scrolling back, 1 if scrolling forward, 0 if neither.
  int drag_scroll_dir_ = 0;

  // Used to periodically call ScrollForUserDrag.
  base::RepeatingTimer scrolling_timer_;

  // Used to call SpeedUpDragScrolling.
  base::OneShotTimer speed_up_drag_scrolling_;

  // Whether this view should focus its last focusable child (instead of its
  // first) when focused.
  bool default_last_focusable_child_ = false;

  // Indicates the starting position of shelf items on the main axis. (Main
  // axis is x-axis when the shelf is horizontally aligned; otherwise, it
  // becomes y-axis)
  int app_icons_layout_offset_ = 0;

  const raw_ptr<Delegate> delegate_;

  // Whether the shelf view is actively acting as an application drag and drop
  // host. Note that shelf view is not expected to create its own
  // `AppDragIconProxy` for item drag operation, but a `drag_icon_proxy_` may
  // get passed to the shelf view using `ApplicationDragAndDropHost::EndDrag()`
  // interface.
  bool is_active_drag_and_drop_host_ = false;

  // The app item icon proxy created for drag operation.
  std::unique_ptr<AppDragIconProxy> drag_icon_proxy_;

  // Placeholder ghost icon to show where an app will drop on the shelf.
  raw_ptr<GhostImageView> current_ghost_view_ = nullptr;
  // The latest ghost icon shown set to be replaced by |current_ghost_view_|.
  raw_ptr<GhostImageView> last_ghost_view_ = nullptr;

  // The index in the shelf app icons where the |current_ghost_view_| will show.
  std::optional<size_t> current_ghost_view_index_ = std::nullopt;

  std::unique_ptr<views::BoundsAnimator> bounds_animator_;

  // When the scrollable shelf is enabled, |shelf_button_delegate_| should
  // be ScrollableShelfView.
  raw_ptr<ShelfButtonDelegate> shelf_button_delegate_ = nullptr;

  // Owned by ScrollableShelfView.
  raw_ptr<views::FocusSearch, DanglingUntriaged> focus_search_ = nullptr;

  std::unique_ptr<FadeInAnimationDelegate> fade_in_animation_delegate_;

  // Tracks the icon move animation.
  std::optional<ui::ThroughputTracker> move_animation_tracker_;

  // Tracks the icon fade-out animation.
  std::optional<ui::ThroughputTracker> fade_out_animation_tracker_;

  // Called when showing shelf context menu.
  base::RepeatingClosure context_menu_shown_callback_;

  // The layer that contains the icon image for the item under the drag cursor.
  // Assigned before the dropping animation is scheduled.
  std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_;

  // Set of promise app items with pending removal. Maps the promise app ID to
  // the promise app icon image.
  PendingPromiseAppsMap pending_promise_apps_removals_;

  base::WeakPtrFactory<ShelfView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_VIEW_H_
