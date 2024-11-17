// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/apps_grid_view.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_item_util.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/apps_grid_row_change_animator.h"
#include "ash/app_list/grid_index.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/views/app_list_a11y_announcer.h"
#include "ash/app_list/views/app_list_folder_controller.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_keyboard_controller.h"
#include "ash/app_list/views/app_list_view_util.h"
#include "ash/app_list/views/apps_grid_context_menu.h"
#include "ash/app_list/views/apps_grid_view_folder_delegate.h"
#include "ash/app_list/views/ghost_image_view.h"
#include "ash/app_list/views/pulsing_block_view.h"
#include "ash/constants/ash_features.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/animation_sequence_block.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// Distance a drag needs to be from the app grid to be considered 'outside', at
// which point we rearrange the apps to their pre-drag configuration, as a drop
// then would be canceled. We have a buffer to make it easier to drag apps to
// other pages.
constexpr int kDragBufferPx = 20;

// Delays in milliseconds to show re-order preview.
constexpr int kReorderDelay = 120;

// Delays in milliseconds to show folder item reparent UI.
constexpr int kFolderItemReparentDelay = 50;

// Maximum vertical and horizontal spacing between tiles.
constexpr int kMaximumTileSpacing = 96;

// Maximum horizontal spacing between tiles.
constexpr int kMaximumHorizontalTileSpacing = 128;

// The ratio of the slide offset to the tile size.
constexpr float kFadeAnimationOffsetRatio = 0.25f;

// The time duration of the fade in animation used for apps grid reorder.
constexpr base::TimeDelta kFadeInAnimationDuration = base::Milliseconds(400);

// The time duration of the fade out animation used for apps grid reorder.
constexpr base::TimeDelta kFadeOutAnimationDuration = base::Milliseconds(100);

// Constants for folder item view relocation animation - the animation runs
// after closing a folder view if the shown folder item view location within the
// apps grid changed while the folder view was open.
// The folder view animates in the old folder item location, then the folder
// item view animates out at the old location, other items move into their
// correct spot, and after a delay, the folder item view animates into its new
// location.
//
// The duration of the folder item view fade out animation.
constexpr base::TimeDelta kFolderItemFadeOutDuration = base::Milliseconds(100);

// The duraction of the folder item view fade in animation.
constexpr base::TimeDelta kFolderItemFadeInDuration = base::Milliseconds(300);

// The delay for starting the folder item view fade in after the item view was
// faded out.
constexpr base::TimeDelta kFolderItemFadeInDelay = base::Milliseconds(300);

// The base time duration for item bounds animations.
constexpr base::TimeDelta kItemBoundsBaseAnimationDuration =
    base::Milliseconds(300);

// The additional time duration for each subsequent row/slot used for creating a
// cascading item bounds animation.
constexpr base::TimeDelta kItemBoundsAnimationOffsetDuration =
    base::Milliseconds(50);

bool IsOEMFolderItem(AppListItem* item) {
  return IsFolderItem(item) && item->AsFolderItem()->folder_type() ==
                                   AppListFolderItem::FOLDER_TYPE_OEM;
}

// Apply `transform` to `bounds` at an origin of (0,0) so that the scaling
// part of the transform does not modify the position or size.
gfx::Rect ApplyTransformAtOrigin(const gfx::Rect& in_bounds,
                                 const gfx::Transform& transform) {
  gfx::Rect out_bounds;
  out_bounds = transform.MapRect(out_bounds);
  out_bounds.Offset(in_bounds.OffsetFromOrigin());
  out_bounds.set_size(in_bounds.size());
  return out_bounds;
}

// Return the pointer that was used for generating the event from the event
// flags.
AppsGridView::Pointer GetPointerTypeForDragAndDrop() {
  if (Shell::Get()->drag_drop_controller()->event_source() ==
      ui::mojom::DragEventSource::kMouse) {
    return AppsGridView::MOUSE;
  }

  return AppsGridView::TOUCH;
}

}  // namespace

// static
constexpr int AppsGridView::kDefaultAnimationDuration;

// AppsGridView::VisibleItemIndexRange -----------------------------------------

AppsGridView::VisibleItemIndexRange::VisibleItemIndexRange() = default;

AppsGridView::VisibleItemIndexRange::VisibleItemIndexRange(size_t first_index,
                                                           size_t last_index)
    : first_index(first_index), last_index(last_index) {}

AppsGridView::VisibleItemIndexRange::~VisibleItemIndexRange() = default;

// AppsGridView::FolderIconItemHider -------------------------------------------

// Class used to hide an icon depicting an app list item from an folder item
// icon image (which contains images of top app items in the folder).
// Used during drag icon drop animation to hide the dragged item from the folder
// icon (if the item is being dropped into a folder) while the drag icon is
// still visible.
// It gracefully handles the folder item getting deleted before the
// `FolderIconItemHider` instance gets reset, so it should be safe to use in
// asynchronous manner without extra folder item existence checks.
class AppsGridView::FolderIconItemHider : public AppListItemObserver,
                                          public views::ViewObserver {
 public:
  FolderIconItemHider(AppListItemView* folder_item_view,
                      AppListItem* item_icon_to_hide)
      : item_view_(folder_item_view),
        folder_item_(folder_item_view->item()->AsFolderItem()) {
    item_view_->AddObserver(this);

    // Notify the folder item that `item_icon_to_hide` is being dragged, so the
    // dragged item is ignored while generating the folder icon image. This
    // effectively hides the drag item image from the overall folder icon.
    item_view_->UpdateDraggedItem(item_icon_to_hide);
    folder_item_->NotifyOfDraggedItem(item_icon_to_hide);
    folder_item_observer_.Observe(folder_item_.get());
  }

  ~FolderIconItemHider() override {
    if (item_view_) {
      item_view_->RemoveObserver(this);
      item_view_->UpdateDraggedItem(nullptr);
    }
    if (folder_item_) {
      folder_item_->NotifyOfDraggedItem(nullptr);
    }
  }

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override {
    DCHECK_EQ(item_view_, observed_view);
    item_view_ = nullptr;
    folder_item_ = nullptr;
    folder_item_observer_.Reset();
  }

  // AppListItemObserver:
  void ItemBeingDestroyed() override {
    item_view_->RemoveObserver(this);
    item_view_ = nullptr;
    folder_item_ = nullptr;
    folder_item_observer_.Reset();
  }

 private:
  // The item view of `folder_item_`;
  raw_ptr<AppListItemView> item_view_;
  raw_ptr<AppListFolderItem> folder_item_;

  base::ScopedObservation<AppListItem, AppListItemObserver>
      folder_item_observer_{this};
};

// Class that while in scope hides a drag view in such way that the drag view
// keeps receiving mouse/gesture events. Used to hide the dragged view while a
// drag icon proxy for the drag item is shown. It gracefully handles the case
// where it outlives the hidden dragged view, so it should be safe to be used
// asynchronously without extra view existence checks.
class AppsGridView::DragViewHider : public views::ViewObserver {
 public:
  explicit DragViewHider(AppListItemView* drag_view) : drag_view_(drag_view) {
    DCHECK(drag_view_->layer());
    drag_view_->layer()->SetOpacity(0.0f);
    view_observer_.Observe(drag_view_.get());
  }

  ~DragViewHider() override {
    if (drag_view_ && drag_view_->layer())
      drag_view_->layer()->SetOpacity(1.0f);
  }

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* view) override {
    drag_view_ = nullptr;
    view_observer_.Reset();
  }

  const views::View* drag_view() const { return drag_view_; }

 private:
  raw_ptr<AppListItemView> drag_view_;

  base::ScopedObservation<views::View, views::ViewObserver> view_observer_{
      this};
};

// Class used by AppsGridView to track whether app list model is being updated
// by the AppsGridView (by setting `updating_model_`). While this is in scope:
// (1) Do not cancel in progress drag due to app list model changes, and
// (2) Delay `view_structure_` sanitization until the app list model update
// finishes, and
// (3) Ignore apps grid layout
class AppsGridView::ScopedModelUpdate {
 public:
  explicit ScopedModelUpdate(AppsGridView* apps_grid_view)
      : apps_grid_view_(apps_grid_view),
        initial_grid_size_(apps_grid_view_->GetTileGridSize()) {
    DCHECK(!apps_grid_view_->updating_model_);
    apps_grid_view_->updating_model_ = true;

    // One model update may elicit multiple changes on apps grid layout. For
    // example, moving one item out of a folder may empty the parent folder then
    // have the folder deleted. Therefore ignore layout when `ScopedModelUpdate`
    // is in the scope to avoid handling temporary layout.
    DCHECK(!apps_grid_view_->ignore_layout_);
    apps_grid_view_->ignore_layout_ = true;
  }
  ScopedModelUpdate(const ScopedModelUpdate&) = delete;
  ScopedModelUpdate& operator=(const ScopedModelUpdate&) = delete;
  ~ScopedModelUpdate() {
    DCHECK(apps_grid_view_->updating_model_);
    apps_grid_view_->updating_model_ = false;

    DCHECK(apps_grid_view_->ignore_layout_);
    apps_grid_view_->ignore_layout_ = false;

    // Perform update for the final layout.
    apps_grid_view_->ScheduleLayout(initial_grid_size_);
  }

 private:
  const raw_ptr<AppsGridView> apps_grid_view_;
  const gfx::Size initial_grid_size_;
};

// An implicit animation observer that runs a callback to restore the grid after
// the animation is done.
class AnimationObserverToRestoreGrid : public ui::ImplicitAnimationObserver {
 public:
  explicit AnimationObserverToRestoreGrid(base::OnceClosure cb)
      : animation_completion_callback_(std::move(cb)) {}
  ~AnimationObserverToRestoreGrid() override {
    // Required due to RequiresNotificationWhenAnimatorDestroyed() returning
    // true.
    StopObservingImplicitAnimations();
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    if (animation_completion_callback_) {
      std::move(animation_completion_callback_).Run();
    }
    delete this;
  }

  bool RequiresNotificationWhenAnimatorDestroyed() const override {
    return true;
  }

 private:
  base::OnceClosure animation_completion_callback_;
};

AppsGridView::AppsGridView(AppListA11yAnnouncer* a11y_announcer,
                           AppListViewDelegate* app_list_view_delegate,
                           AppsGridViewFolderDelegate* folder_delegate,
                           AppListFolderController* folder_controller,
                           AppListKeyboardController* keyboard_controller)
    : folder_delegate_(folder_delegate),
      folder_controller_(folder_controller),
      a11y_announcer_(a11y_announcer),
      app_list_view_delegate_(app_list_view_delegate),
      keyboard_controller_(keyboard_controller) {
  DCHECK(a11y_announcer_);
  DCHECK(app_list_view_delegate_);
  // Top-level grids must have a folder controller.
  if (!folder_delegate_) {
    DCHECK(folder_controller_);
  }

  SetPaintToLayer(ui::LAYER_NOT_DRAWN);

  items_container_ = AddChildView(std::make_unique<views::View>());
  items_container_->SetPaintToLayer();
  items_container_->layer()->SetFillsBoundsOpaquely(false);

  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);

  // Override the a11y name of top level apps grid.
  if (!folder_delegate) {
    GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_ASH_LAUNCHER_APPS_GRID_A11Y_NAME),
        ax::mojom::NameFrom::kAttribute);
  }

  if (!IsTabletMode()) {
    // `context_menu_` is only set in clamshell mode. The sort options in tablet
    // mode are handled in RootWindowController with ShelfContextMenuModel.
    context_menu_ = std::make_unique<AppsGridContextMenu>(
        AppsGridContextMenu::GridType::kAppsGrid);
    set_context_menu_controller(context_menu_.get());
  }
  row_change_animator_ = std::make_unique<AppsGridRowChangeAnimator>(this);
}

AppsGridView::~AppsGridView() {
  // Coming here |drag_view_| should already be canceled since otherwise the
  // drag would disappear after the app list got animated away and closed,
  // which would look odd.
  DCHECK(!drag_item_);

  if (model_) {
    model_->RemoveObserver(this);
  }

  if (item_list_) {
    item_list_->RemoveObserver(this);
  }

  set_context_menu_controller(nullptr);

  // Abort reorder animation before `view_model_` is cleared.
  MaybeAbortWholeGridAnimation();

  // Reset `folder_icon_item_hider_` before clearing the view model to prevent
  // accessing the AppListItemView after it is deleted.
  folder_icon_item_hider_.reset();
  view_model_.Clear();
  pulsing_blocks_model_.Clear();
  RemoveAllChildViews();

  folder_to_open_after_drag_icon_animation_.clear();
  // To prevent a call to |OnDragIconDropDone()| from an existing drag image
  // animation.
  weak_factory_.InvalidateWeakPtrs();
  drag_image_layer_.reset();
}

void AppsGridView::UpdateAppListConfig(const AppListConfig* app_list_config) {
  app_list_config_ = app_list_config;

  // The app list item view icon sizes depend on the app list config, so they
  // have to be refreshed.
  for (size_t i = 0; i < view_model_.view_size(); ++i)
    view_model_.view_at(i)->UpdateAppListConfig(app_list_config);

  if (current_ghost_view_) {
    CreateGhostImageView();
  }
}

void AppsGridView::SetFixedTilePadding(int horizontal_padding,
                                       int vertical_padding) {
  has_fixed_tile_padding_ = true;
  horizontal_tile_padding_ = horizontal_padding;
  vertical_tile_padding_ = vertical_padding;
}

gfx::Size AppsGridView::GetTotalTileSize(int page) const {
  gfx::Rect rect(GetTileViewSize());
  rect.Inset(GetTilePadding(page));
  return rect.size();
}

gfx::Size AppsGridView::GetMinimumTileGridSize(int cols,
                                               int rows_per_page) const {
  const gfx::Size tile_size = GetTileViewSize();
  return gfx::Size(tile_size.width() * cols,
                   tile_size.height() * rows_per_page);
}

gfx::Size AppsGridView::GetMaximumTileGridSize(int cols,
                                               int rows_per_page) const {
  const gfx::Size tile_size = GetTileViewSize();
  return gfx::Size(
      tile_size.width() * cols + kMaximumHorizontalTileSpacing * (cols - 1),
      tile_size.height() * rows_per_page +
          kMaximumTileSpacing * (rows_per_page - 1));
}

void AppsGridView::ResetForShowApps() {
  CancelDragWithNoDropAnimation();

  layer()->SetOpacity(1.0f);
  SetVisible(true);

  // The number of model items should be the same as item views.
  if (item_list_) {
    CHECK_EQ(item_list_->item_count(), view_model_.view_size());
  }
}

void AppsGridView::EndDragCallback(
    const ui::DropTargetEvent& event,
    ui::mojom::DragOperation& output_drag_op,
    std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
  output_drag_op = ui::mojom::DragOperation::kMove;

  if (drag_item_) {
    drag_image_layer_ = std::move(drag_image_layer_owner);
    EndDrag(/*cancel=*/false);
  }
}

void AppsGridView::CancelDragWithNoDropAnimation() {
  EndDrag(/*cancel=*/true);
  drag_view_hider_.reset();
  folder_icon_item_hider_.reset();
  if (!folder_to_open_after_drag_icon_animation_.empty()) {
    open_folder_info_.reset();
  }
  folder_to_open_after_drag_icon_animation_.clear();
  drag_image_layer_.reset();
}

void AppsGridView::DisableFocusForShowingActiveFolder(bool disabled) {
  for (const auto& entry : view_model_.entries())
    entry.view->SetEnabled(!disabled);

  // Ignore the grid view in accessibility tree so that items inside it will not
  // be accessed by ChromeVox.
  SetViewIgnoredForAccessibility(this, disabled);
}

void AppsGridView::SetModel(AppListModel* model) {
  if (model_) {
    model_->RemoveObserver(this);
  }

  model_ = model;
  if (model_) {
    model_->AddObserver(this);
  }

  Update();
}

void AppsGridView::SetItemList(AppListItemList* item_list) {
  DCHECK_GT(cols_, 0);
  DCHECK(app_list_config_);

  if (item_list_) {
    item_list_->RemoveObserver(this);
  }
  item_list_ = item_list;
  if (item_list_) {
    item_list_->AddObserver(this);
  }
  Update();
}

bool AppsGridView::IsInFolder() const {
  return !!folder_delegate_;
}

void AppsGridView::SetSelectedView(AppListItemView* view) {
  if (IsSelectedView(view) || IsDraggedView(view)) {
    return;
  }

  GridIndex index = GetIndexOfView(view);
  if (IsValidIndex(index)) {
    SetSelectedItemByIndex(index);
  }
}

void AppsGridView::ClearSelectedView() {
  selected_view_ = nullptr;
}

bool AppsGridView::IsSelectedView(const AppListItemView* view) const {
  return selected_view_ == view;
}

void AppsGridView::UpdateDrag(Pointer pointer, const gfx::Point& point) {
  if (folder_delegate_) {
    UpdateDragStateInsideFolder(pointer, point);
  }

  if (!drag_item_) {
    return;  // Drag canceled.
  }

  // If folder is currently open from the grid, delay drag updates until the
  // folder finishes closing.
  if (open_folder_info_) {
    // Only handle pointers that initiated the drag - e.g. ignore drag events
    // that come from touch if a mouse drag is currently in progress.
    if (drag_pointer_ == pointer) {
      last_drag_point_ = point;
    }
    return;
  }
  MaybeStartCardifiedView();

  last_drag_point_ = point;
  const GridIndex last_drop_target = drop_target_;
  DropTargetRegion last_drop_target_region = drop_target_region_;
  UpdateDropTargetRegion();

  const bool has_page_flip = MaybeStartPageFlip();
  const bool is_scrolling = MaybeAutoScroll();

  if (is_scrolling || has_page_flip) {
    // Don't do reordering while auto-scrolling, or flipping page, otherwise
    // there is too much motion during the drag.
    reorder_timer_.Stop();
    // Reset the previous drop target.
    if (last_drop_target_region == ON_ITEM)
      SetAsFolderDroppingTarget(last_drop_target, false);
    return;
  }

  if (last_drop_target != drop_target_ ||
      last_drop_target_region != drop_target_region_) {
    if (last_drop_target_region == ON_ITEM)
      SetAsFolderDroppingTarget(last_drop_target, false);
    if (drop_target_region_ == ON_ITEM && DraggedItemCanEnterFolder() &&
        DropTargetIsValidFolder()) {
      reorder_timer_.Stop();
      MaybeCreateFolderDroppingAccessibilityEvent();
      SetAsFolderDroppingTarget(drop_target_, true);
      BeginHideCurrentGhostImageView();
    } else if ((drop_target_region_ == ON_ITEM ||
                drop_target_region_ == NEAR_ITEM) &&
               !folder_delegate_) {
      // If the drag changes regions from |BETWEEN_ITEMS| to |NEAR_ITEM| the
      // timer should reset, so that we gain the extra time from hovering near
      // the item
      if (last_drop_target_region == BETWEEN_ITEMS) {
        reorder_timer_.Stop();
      }
      reorder_timer_.Start(FROM_HERE, base::Milliseconds(kReorderDelay * 5),
                           this, &AppsGridView::OnReorderTimer);
    } else if (drop_target_region_ != NO_TARGET) {
      // If none of the above cases evaluated true, then all of the possible
      // drop regions should result in a fast reorder.
      reorder_timer_.Start(FROM_HERE, base::Milliseconds(kReorderDelay), this,
                           &AppsGridView::OnReorderTimer);
    }
  }
}

void AppsGridView::EndDrag(bool cancel) {
  DVLOG(1) << "EndDrag cancel=" << cancel;

  // EndDrag was called before if |drag_view_| is nullptr.
  if (!drag_item_) {
    return;
  }

  AppListItem* drag_item = drag_item_;

  // Whether an icon was actually dragged (and not just clicked).
  const bool was_dragging = IsDragging();

  // The ID of the folder to which the item gets dropped. It will get set when
  // the item is moved to a folder.
  std::string target_folder_id;

  // The animation direction used for the ideal bounds animation.
  bool top_to_bottom_animation = reorder_placeholder_ < drop_target_;

  if (IsDraggingForReparentInHiddenGridView()) {
    EndDragForReparentInHiddenFolderGridView();
    // Forward the EndDrag event to the root level grid view.
    folder_delegate_->DispatchEndDragEventForReparent(
        false /* events_forwarded_to_drag_drop_host */,
        cancel /* cancel_drag */);
    return;
  }

  if (IsDraggingForReparentInRootLevelGridView()) {
    // An EndDrag can be received during a reparent via a model change. This
    // is always a cancel and needs to be forwarded to the folder.
    if (!cancel) {
      UpdateDropTargetRegion();
      EndDragFromReparentItemInRootLevel(nullptr, false, false);
      return;
    }
    DCHECK(!reparent_drag_cancellation_);
  }

  if (!cancel && was_dragging) {
    // Regular drag ending path, ie, not for reparenting.
    UpdateDropTargetRegion();
    if (drop_target_region_ == ON_ITEM && DraggedItemCanEnterFolder() &&
        DropTargetIsValidFolder()) {
      // Adding an item to a folder moves items similarly to moving it to the
      // end of the list, so set as a top_to_bottom animation direction.
      top_to_bottom_animation = true;
      bool is_new_folder = false;
      if (MoveItemToFolder(drag_item_, drop_target_, kMoveByDragIntoFolder,
                           &target_folder_id, &is_new_folder)) {
        MaybeCreateFolderDroppingAccessibilityEvent();
        if (is_new_folder) {
          folder_to_open_after_drag_icon_animation_ = target_folder_id;
          SetOpenFolderInfo(target_folder_id, drop_target_,
                            reorder_placeholder_);
        }

        // If item drag created a folder, layout the grid to ensure the
        // created folder's bounds are correct. Note that `open_folder_info_`
        // affects ideal item bounds, so `DeprecatedLayoutImmediately()` needs
        // to be called after `SetOpenFolderInfo()`.
        DeprecatedLayoutImmediately();
      }
    } else if (IsValidIndex(drop_target_)) {
      // Ensure reorder event has already been announced by the end of drag.
      MaybeCreateDragReorderAccessibilityEvent();
      MoveItemInModel(drag_item_, drop_target_);
      RecordAppMovingTypeMetrics(folder_delegate_ ? kReorderByDragInFolder
                                                  : kReorderByDragInTopLevel);
    }
  }

  SetAsFolderDroppingTarget(drop_target_, false);

  ClearDragState();
  UpdatePaging();

  if (GetWidget()) {
    // Normally layout cancels any animations. At this point there may be a
    // pending layout, force it now so that one isn't triggered part way through
    // the animation. Further, ignore this layout so that the position isn't
    // reset.
    DCHECK(!ignore_layout_);
    base::AutoReset<bool> auto_reset(&ignore_layout_, true);
    GetWidget()->LayoutRootViewIfNecessary();
  }
  if (cardified_state_)
    MaybeEndCardifiedView();
  else
    AnimateToIdealBounds(top_to_bottom_animation);

  if (!cancel) {
    // Select the page where dragged item is dropped. Avoid doing so when the
    // dragged item ends up in a folder.
    const size_t model_index = GetModelIndexOfItem(drag_item);
    if (model_index < view_model_.view_size())
      EnsureViewVisible(GetGridIndexFromIndexInViewModel(model_index));
  }

  // Hide the |current_ghost_view_| for item drag that started
  // within |apps_grid_view_|.
  BeginHideCurrentGhostImageView();
  if (was_dragging)
    SetFocusAfterEndDrag(drag_item);  // Maybe focus the search box.

  AnimateDragIconToTargetPosition(drag_item, target_folder_id);
}

AppListItemView* AppsGridView::GetItemViewForItem(const std::string& item_id) {
  const AppListItem* const item = item_list_->FindItem(item_id);
  if (!item) {
    return nullptr;
  }

  return GetItemViewAt(GetModelIndexOfItem(item));
}

AppListItemView* AppsGridView::GetItemViewAt(size_t index) const {
  return (index < view_model_.view_size()) ? view_model_.view_at(index)
                                           : nullptr;
}

void AppsGridView::UpdateDragFromReparentItem(Pointer pointer,
                                              const gfx::Point& drag_point) {
  // Note that if a cancel ocurrs while reparenting, the |drag_view_| in both
  // root and folder grid views is cleared, so the check in UpdateDragFromItem()
  // for |drag_view_| being nullptr (in the folder grid) is sufficient.
  DCHECK(drag_item_);
  DCHECK(IsDraggingForReparentInRootLevelGridView());

  UpdateDrag(pointer, drag_point);
}

void AppsGridView::SetOpenFolderInfo(const std::string& folder_id,
                                     const GridIndex& target_folder_position,
                                     const GridIndex& position_to_skip) {
  GridIndex expected_folder_position = target_folder_position;
  // If the target view is positioned after `position_to_skip`, move the
  // target one slot earlier, as `position_to_skip` is assumed about to be
  // emptied.
  if (position_to_skip.IsValid() &&
      position_to_skip < expected_folder_position &&
      expected_folder_position.slot > 0) {
    --expected_folder_position.slot;
  }

  open_folder_info_ = {.item_id = folder_id,
                       .grid_index = expected_folder_position};
}

void AppsGridView::ShowFolderForView(AppListItemView* folder_view,
                                     bool new_folder) {
  DCHECK(open_folder_info_);

  // Guard against invalid folder view.
  if (!folder_view || !folder_view->is_folder()) {
    open_folder_info_.reset();
    return;
  }

  folder_controller_->ShowFolderForItemView(
      folder_view,
      /*focus_name_input=*/new_folder,
      base::BindOnce(&AppsGridView::FolderHidden, weak_factory_.GetWeakPtr(),
                     folder_view->item()->id()));
}

void AppsGridView::FolderHidden(const std::string& item_id) {
  if (!open_folder_info_ || open_folder_info_->item_id != item_id) {
    return;
  }

  // Find the folder item location in the app list model to determine whether
  // the item view location changed while the folder was closed (in which case
  // the folder location change should be animated).
  AppListItemView* item_view = nullptr;
  int model_index = -1;
  for (size_t i = 0; i < view_model_.view_size(); ++i) {
    AppListItemView* view = view_model_.view_at(i);
    if (view == drag_view_) {
      continue;
    }

    ++model_index;
    if (view->item()->id() == item_id) {
      item_view = view;
      break;
    }
  }

  // If the item view is gone, or the location in the grid did not change,
  // the folder item should not be animated - immediately update apps grid state
  // for folder hide.
  if (!item_view || GetGridIndexFromIndexInViewModel(model_index) ==
                        open_folder_info_->grid_index) {
    open_folder_info_.reset();
    OnFolderHideAnimationDone();
    return;
  }

  // When folder animates out, remaining items will animate to their ideal
  // bounds - ensure their layers are created (and marked not to fill bounds
  // opaquely).
  PrepareItemsForBoundsAnimation();

  // Animate the folder item view out from its original location.
  reordering_folder_view_ = item_view;
  views::AnimationBuilder animation;
  animation.OnEnded(base::BindOnce(&AppsGridView::AnimateFolderItemViewIn,
                                   weak_factory_.GetWeakPtr()));
  animation.OnAborted(base::BindOnce(&AppsGridView::AnimateFolderItemViewIn,
                                     weak_factory_.GetWeakPtr()));

  gfx::Transform scale;
  scale.Scale(0.5, 0.5);
  scale = gfx::TransformAboutPivot(
      gfx::RectF(item_view->GetLocalBounds()).CenterPoint(), scale);
  animation.Once()
      .SetDuration(kFolderItemFadeOutDuration)
      .SetTransform(item_view->layer(), scale, gfx::Tween::FAST_OUT_LINEAR_IN)
      .SetOpacity(item_view->layer(), 0.0f, gfx::Tween::FAST_OUT_LINEAR_IN);
}

void AppsGridView::AnimateFolderItemViewIn() {
  const GridIndex before_index =
      open_folder_info_ ? open_folder_info_->grid_index : GridIndex();
  const GridIndex after_index =
      GetIndexOfView(reordering_folder_view_.value_or(nullptr));
  const bool top_to_bottom_animation = before_index < after_index;

  // Once folder item view fades out, animate remaining items into their target
  // location, and schedule the folder item view fade-in (note that
  // `AnimateToIdealBounds()` updates `reordering_folder_view_` bounds without
  // animation).
  open_folder_info_.reset();
  AnimateToIdealBounds(top_to_bottom_animation);

  if (!reordering_folder_view_) {
    return;
  }

  views::AnimationBuilder()
      .OnEnded(base::BindOnce(&AppsGridView::OnFolderHideAnimationDone,
                              weak_factory_.GetWeakPtr()))
      .OnAborted(base::BindOnce(&AppsGridView::OnFolderHideAnimationDone,
                                weak_factory_.GetWeakPtr()))
      .Once()
      .At(kFolderItemFadeInDelay)
      .SetDuration(kFolderItemFadeInDuration)
      .SetTransform(reordering_folder_view_.value()->layer(), gfx::Transform(),
                    gfx::Tween::ACCEL_LIN_DECEL_100_3)
      .SetOpacity(reordering_folder_view_.value()->layer(), 1.0f,
                  gfx::Tween::ACCEL_LIN_DECEL_100_3);
}

void AppsGridView::OnFolderHideAnimationDone() {
  reordering_folder_view_.reset();
  DestroyLayerItemsIfNotNeeded();
  if (IsDraggingForReparentInRootLevelGridView()) {
    MaybeStartCardifiedView();
    UpdateDrag(drag_pointer_, last_drag_point_);
  }
}

bool AppsGridView::IsDragging() const {
  return drag_pointer_ != NONE;
}

bool AppsGridView::IsDraggedView(const AppListItemView* view) const {
  return drag_item_ == view->item();
}

void AppsGridView::ClearDragState() {
  current_ghost_location_ = GridIndex();
  last_folder_dropping_a11y_event_location_ = GridIndex();
  last_reorder_a11y_event_location_ = GridIndex();
  drop_target_region_ = NO_TARGET;
  drag_pointer_ = NONE;
  drop_target_ = GridIndex();
  reorder_placeholder_ = GridIndex();
  drag_start_grid_view_ = gfx::Point();

  if (folder_item_reparent_timer_.IsRunning())
    folder_item_reparent_timer_.Stop();

  MaybeStopPageFlip();
  StopAutoScroll();

  if (drag_item_) {
    drag_item_->RemoveObserver(this);
  }

  drag_view_ = nullptr;
  drag_item_ = nullptr;
  drag_out_of_folder_container_ = false;
  dragging_for_reparent_item_ = false;
  extra_page_opened_ = false;
  reparent_drag_cancellation_.Reset();
}

bool AppsGridView::IsAnimatingView(AppListItemView* view) const {
  return view->layer() && view->layer()->GetAnimator()->is_animating();
}

gfx::Size AppsGridView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return GetTileGridSize();
}

bool AppsGridView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  format_types->insert(GetAppItemFormatType());
  return true;
}

bool AppsGridView::CanDrop(const OSExchangeData& data) {
  if (ShouldContainerHandleDragEvents()) {
    return false;
  }

  return WillAcceptDropEvent(data);
}

bool AppsGridView::WillAcceptDropEvent(const OSExchangeData& data) {
  // Ignore drop events if the app list is syncing.
  if (pulsing_blocks_model_.view_size()) {
    return false;
  }

  auto app_id = GetAppInfoFromDropDataForAppType(data);
  if (!app_id.has_value() || app_id->IsValid()) {
    return false;
  }
  std::set<ui::ClipboardFormatType> format_types;
  GetDropFormats(nullptr, &format_types);

  return data.HasAnyFormat(0, format_types);
}

void AppsGridView::OnDragExited() {
  // When the drag and drop host is a folder apps grid, close the folder when
  // drag exits folder grid bounds.
  // TODO(b/261985897): Add timer to close folder bounds.
  if (folder_delegate_) {
    if (drag_view_) {
      folder_delegate_->ReparentItem(drag_pointer_, drag_view_,
                                     last_drag_point_);
    }

    if (item_list_) {
      // Do not observe any data change since it is going to be hidden.
      item_list_->RemoveObserver(this);
    }
    item_list_ = nullptr;
    dragging_for_reparent_item_ = true;
    folder_delegate_->Close();
  }
  if (drag_view_) {
    drag_view_->ClearItemDraggingState();
  }
  CancelDragWithNoDropAnimation();
}

void AppsGridView::ItemBeingDestroyed() {
  DCHECK(drag_item_);
  EndDrag(/*cancel=*/true);
  DCHECK(!drag_item_);
}

void AppsGridView::OnDragEntered(const ui::DropTargetEvent& event) {
  // Ignore drag events if the app list is syncing.
  if (pulsing_blocks_model_.view_size()) {
    return;
  }

  auto app_info = GetAppInfoFromDropDataForAppType(event.data());
  if (!app_info || app_info->IsValid()) {
    return;
  }

  drag_item_ = AppListModelProvider::Get()->model()->FindItem(app_info->app_id);
  if (!drag_item_) {
    return;
  }
  drag_item_->AddObserver(this);

  // Finalize previous drag icon animation if it's still in progress.
  drag_view_hider_.reset();
  folder_icon_item_hider_.reset();
  folder_to_open_after_drag_icon_animation_.clear();

  PrepareItemsForBoundsAnimation();
  drag_pointer_ = GetPointerTypeForDragAndDrop();
  drag_view_ = GetItemViewAt(GetModelIndexOfItem(drag_item_));
  if (drag_view_) {
    drag_view_hider_ = std::make_unique<DragViewHider>(drag_view_);
    // Dragged view should have focus. This also fixed the issue
    // https://crbug.com/834682.
    drag_view_->RequestFocus();
    drag_view_init_index_ = GetIndexOfView(drag_view_);
  } else {
    dragging_for_reparent_item_ = true;
  }

  const gfx::Size initial_grid_size = GetTileGridSize();
  reorder_placeholder_ =
      drag_view_ ? drag_view_init_index_
                 : GetGridIndexFromIndexInViewModel(view_model()->view_size());
  UpdatePaging();

  // When reparenting drag, the preferred grid size may change if there are no
  // extra slots on the grid for the placeholder item.
  if (GetTileGridSize() != initial_grid_size) {
    PreferredSizeChanged();
  }
  ExtractDragLocation(event.root_location(), &drag_start_grid_view_);
}

int AppsGridView::OnDragUpdated(const ui::DropTargetEvent& event) {
  UpdateDrag(drag_pointer_, event.location());
  return ui::DragDropTypes::DRAG_MOVE;
}

void AppsGridView::UpdateControlVisibility(AppListViewState app_list_state) {
  SetVisible(app_list_state == AppListViewState::kFullscreenAllApps ||
             app_list_state == AppListViewState::kFullscreenSearch);
}

views::View::DropCallback AppsGridView::GetDropCallback(
    const ui::DropTargetEvent& event) {
  return base::BindOnce(&AppsGridView::EndDragCallback, base::Unretained(this));
}

bool AppsGridView::OnKeyPressed(const ui::KeyEvent& event) {
  // The user may press VKEY_CONTROL before an arrow key when intending to do an
  // app move with control+arrow.
  if (event.key_code() == ui::VKEY_CONTROL) {
    return true;
  }

  if (selected_view_ && IsArrowKeyEvent(event) && event.IsControlDown()) {
    HandleKeyboardAppOperations(event.key_code(), event.IsShiftDown());
    return true;
  }

  // Let the FocusManager handle Left/Right keys.
  if (!IsUnhandledUpDownKeyEvent(event)) {
    return false;
  }

  const bool arrow_up = event.key_code() == ui::VKEY_UP;
  return HandleVerticalFocusMovement(arrow_up);
}

bool AppsGridView::OnKeyReleased(const ui::KeyEvent& event) {
  if (event.IsControlDown() || !handling_keyboard_move_) {
    return false;
  }

  handling_keyboard_move_ = false;
  RecordAppMovingTypeMetrics(folder_delegate_ ? kReorderByKeyboardInFolder
                                              : kReorderByKeyboardInTopLevel);
  return false;
}

void AppsGridView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (!details.is_add && details.parent == items_container_.get()) {
    // The view being delete should not have reference in |view_model_|.
    CHECK(!view_model_.GetIndexOfView(details.child).has_value());

    if (selected_view_.get() == details.child) {
      selected_view_ = nullptr;
    }

    if (drag_view_.get() == details.child) {
      drag_view_ = nullptr;
    }

    if (current_ghost_view_.get() == details.child) {
      current_ghost_view_ = nullptr;
    }
    if (last_ghost_view_.get() == details.child) {
      last_ghost_view_ = nullptr;
    }

    if (reordering_folder_view_ && *reordering_folder_view_ == details.child)
      reordering_folder_view_.reset();

    row_change_animator_->CancelAnimation(details.child);
  }
}

void AppsGridView::Update() {
  // Abort reorder animation before `view_model_` is cleared.
  MaybeAbortWholeGridAnimation();

  view_model_.Clear();
  pulsing_blocks_model_.Clear();
  items_container_->RemoveAllChildViews();

  DCHECK(!selected_view_);
  DCHECK(!drag_view_);

  std::vector<AppListItemView*> item_views;
  if (item_list_ && item_list_->item_count()) {
    for (size_t i = 0; i < item_list_->item_count(); ++i) {
      std::unique_ptr<AppListItemView> view = CreateViewForItemAtIndex(i);
      view_model_.Add(view.get(), view_model_.view_size());
      item_views.push_back(items_container_->AddChildView(std::move(view)));
    }
  }
  UpdateColsAndRowsForFolder();
  UpdatePaging();
  UpdatePulsingBlockViews();
  PreferredSizeChanged();

  // Icon load can change the item position in the view model, so don't iterate
  // over view model to get items to update.
  for (auto* item_view : item_views) {
    item_view->InitializeIconLoader();
  }

  if (!folder_delegate_) {
    RecordPageMetrics();
  }
}

base::TimeDelta AppsGridView::GetPulsingBlockAnimationDelayForIndex(
    int block_index) {
  // The column in which the last AppListItemViews is located.
  // |view_model_| only contains synced AppListItemViews and not
  // PulsingBlockViews.
  const int last_non_block_view_column = view_model_.view_size() % cols_;
  // The index of the pulsing block view related to the |view_model_|.
  const int block_index_in_view_model = view_model_.view_size() + block_index;
  const base::TimeDelta staging_step_delay = base::Milliseconds(100);

  // Depending of the row and column for the pulsing block, we stage the pulsing
  // animation so it sweeps at a 45 degree angle from the upper left to the
  // lower right.
  return staging_step_delay *
             ((last_non_block_view_column + block_index) / cols_) +
         staging_step_delay * (block_index_in_view_model % cols_);
}

void AppsGridView::OnSwapAnimationDone(views::View* placeholder,
                                       AppListItemView* app_view) {
  delete placeholder;

  if (view_model_.GetIndexOfView(app_view).has_value() &&
      !ItemViewsRequireLayers())
    app_view->DestroyLayer();

  UpdatePulsingBlockViews();
}

AppListItemView* AppsGridView::MaybeSwapPlaceholderAsset(size_t index) {
  AppListItemView* view =
      items_container_->AddChildViewAt(CreateViewForItemAtIndex(index), index);
  view_model_.Add(view, index);

  const bool placeholder_in_view_index = index == (view_model_.view_size() - 1);
  const bool is_syncing =
      model_ && model_->status() == AppListModelStatus::kStatusSyncing;
  const bool should_animate_placeholder_swap =
      pulsing_blocks_model_.view_size() > 0 && is_syncing &&
      placeholder_in_view_index;

  if (should_animate_placeholder_swap) {
    PulsingBlockView* placeholder =
        items_container_->AddChildView(std::make_unique<PulsingBlockView>(
            app_list_config_->grid_icon_size(), base::TimeDelta(),
            app_list_config_->grid_icon_dimension() / 2.f));
    placeholder->SetBoundsRect(view->bounds());
    placeholder->SetPaintToLayer();
    view->EnsureLayer();
    view->layer()->SetOpacity(0);
    views::AnimationBuilder()
        .SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .OnEnded(base::BindOnce(&AppsGridView::OnSwapAnimationDone,
                                weak_factory_.GetWeakPtr(), placeholder, view))
        .OnAborted(base::BindOnce(&AppsGridView::OnSwapAnimationDone,
                                  weak_factory_.GetWeakPtr(), placeholder,
                                  view))
        .Once()
        .SetDuration(base::Milliseconds(200))
        .SetOpacity(placeholder->layer(), 0.0f, gfx::Tween::LINEAR)
        .SetOpacity(view->layer(), 1.0f, gfx::Tween::LINEAR);
  } else {
    UpdatePulsingBlockViews();
  }
  return view;
}

void AppsGridView::UpdatePulsingBlockViews() {
  if (!model_ || model_->status() != AppListModelStatus::kStatusSyncing) {
    pulsing_blocks_model_.Clear();
    return;
  }

  const size_t desired_count =
      GetNumberOfPulsingBlocksToShow(item_list_ ? item_list_->item_count() : 0);
  if (pulsing_blocks_model_.view_size() == desired_count) {
    return;
  }

  pulsing_blocks_model_.Clear();

  while (pulsing_blocks_model_.view_size() < desired_count) {
    base::TimeDelta time = GetPulsingBlockAnimationDelayForIndex(
        pulsing_blocks_model_.view_size());
    auto view = std::make_unique<PulsingBlockView>(
        app_list_config_->grid_icon_size(), time,
        app_list_config_->grid_icon_dimension() / 2.f);
    pulsing_blocks_model_.Add(view.get(), pulsing_blocks_model_.view_size());
    items_container_->AddChildView(std::move(view));
  }
}

std::unique_ptr<AppListItemView> AppsGridView::CreateViewForItemAtIndex(
    size_t index) {
  // The |drag_view_| might be pending for deletion, therefore |view_model_|
  // may have one more item than |item_list_|.
  DCHECK_LE(index, item_list_->item_count());
  auto view = std::make_unique<AppListItemView>(
      app_list_config_, this, item_list_->item_at(index),
      app_list_view_delegate_, AppListItemView::Context::kAppsGridView);
  if (ItemViewsRequireLayers()) {
    view->EnsureLayer();
  }
  if (cardified_state_) {
    view->EnterCardifyState();
  }
  return view;
}

void AppsGridView::SetSelectedItemByIndex(const GridIndex& index) {
  if (GetIndexOfView(selected_view_) == index) {
    return;
  }

  AppListItemView* new_selection = GetViewAtIndex(index);
  if (!new_selection) {
    return;  // Keep current selection.
  }

  if (selected_view_) {
    selected_view_->SchedulePaint();
  }

  EnsureViewVisible(index);
  selected_view_ = new_selection;
  selected_view_->SchedulePaint();
  selected_view_->NotifyAccessibilityEvent(ax::mojom::Event::kFocus, true);
  if (selected_view_->HasNotificationBadge()) {
    a11y_announcer_->AnnounceItemNotificationBadge(
        selected_view_->title()->GetText());
  }
}

int AppsGridView::GetIndexInViewModel(const GridIndex& index) const {
  if (index.page == 0) {
    return index.slot;
  }

  // NOTE: Non-zero page implies that the grid supports paging, so
  // `TilesPerPage()` should return non-null optional.
  const int first_page_size = *TilesPerPage(0);
  const int default_page_size = *TilesPerPage(1);
  return first_page_size + (index.page - 1) * default_page_size + index.slot;
}

GridIndex AppsGridView::GetIndexOfView(const AppListItemView* view) const {
  const auto model_index = view_model_.GetIndexOfView(view);
  if (!model_index.has_value()) {
    return GridIndex();
  }

  return GetGridIndexFromIndexInViewModel(model_index.value());
}

AppListItemView* AppsGridView::GetViewAtIndex(const GridIndex& index) const {
  if (!IsValidIndex(index)) {
    return nullptr;
  }

  const size_t model_index = GetIndexInViewModel(index);
  return GetItemViewAt(model_index);
}

std::optional<int> AppsGridView::TilesPerPage(int page) const {
  const std::optional<int> max_rows = GetMaxRowsInPage(page);
  if (!max_rows.has_value()) {
    return std::nullopt;
  }
  return *max_rows * cols();
}

bool AppsGridView::IsAnimatingCardifiedState() const {
  return false;
}

bool AppsGridView::MaybeStartPageFlip() {
  return false;
}

void AppsGridView::SetMaxColumnsInternal(int max_cols) {
  if (max_cols_ == max_cols) {
    return;
  }

  max_cols_ = max_cols;

  if (IsInFolder()) {
    UpdateColsAndRowsForFolder();
  } else {
    cols_ = max_cols_;
  }
}

void AppsGridView::SetIdealBoundsForViewToGridIndex(
    size_t view_index_in_model,
    const GridIndex& view_grid_index) {
  gfx::Rect tile_bounds = GetExpectedTileBounds(view_grid_index);
  tile_bounds.Offset(CalculateTransitionOffset(view_grid_index.page));
  if (view_index_in_model < view_model_.view_size()) {
    view_model_.set_ideal_bounds(view_index_in_model, tile_bounds);
  } else {
    pulsing_blocks_model_.set_ideal_bounds(
        view_index_in_model - view_model_.view_size(), tile_bounds);
  }
}

void AppsGridView::CalculateIdealBounds() {
  AppListItemView* view_with_locked_position = nullptr;
  if (open_folder_info_)
    view_with_locked_position = GetItemViewForItem(open_folder_info_->item_id);

  std::set<GridIndex> reserved_slots;
  reserved_slots.insert(reorder_placeholder_);
  if (open_folder_info_) {
    reserved_slots.insert(open_folder_info_->grid_index);
  }

  const size_t total_views =
      view_model_.view_size() + pulsing_blocks_model_.view_size();
  int slot_index = 0;
  for (size_t i = 0; i < total_views; ++i) {
    // NOTE: Because of pulsing blocks, `i` can count up to a value higher than
    // the view model size. So verify that `i` is less than the view model size
    // before fetching at index `i` from the view model.
    if (i < view_model_.view_size() && view_model_.view_at(i) == drag_view_) {
      continue;
    }

    if (i < view_model_.view_size() &&
        view_model_.view_at(i) == view_with_locked_position) {
      SetIdealBoundsForViewToGridIndex(i, open_folder_info_->grid_index);
      continue;
    }

    GridIndex view_index = GetGridIndexFromIndexInViewModel(slot_index);

    // Leaves a blank space in the grid for the current reorder placeholder.
    while (reserved_slots.count(view_index)) {
      ++slot_index;
      view_index = GetGridIndexFromIndexInViewModel(slot_index);
    }

    if (i < view_model_.view_size())
      view_model_.view_at(i)->SetMostRecentGridIndex(view_index, cols_);
    SetIdealBoundsForViewToGridIndex(i, view_index);
    ++slot_index;
  }
}

void AppsGridView::AnimateToIdealBounds(bool is_animating_top_to_bottom) {
  if (layer()->GetCompositor()) {
    item_reorder_animation_tracker_ =
        layer()->GetCompositor()->RequestNewThroughputTracker();
    item_reorder_animation_tracker_->Start(
        metrics_util::ForSmoothnessV3(base::BindRepeating(
            &ReportItemDragReorderAnimationSmoothness, IsTabletMode())));
  }

  gfx::Rect visible_bounds(GetVisibleBounds());
  gfx::Point visible_origin = visible_bounds.origin();
  ConvertPointToTarget(this, items_container_, &visible_origin);
  visible_bounds.set_origin(visible_origin);

  CalculateIdealBounds();

  std::unique_ptr<views::AnimationBuilder> animation;
  auto init_animation = [&]() -> std::unique_ptr<views::AnimationBuilder> {
    std::unique_ptr<views::AnimationBuilder> animation =
        std::make_unique<views::AnimationBuilder>();
    animation
        ->SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .OnEnded(base::BindOnce(&AppsGridView::OnIdealBoundsAnimationDone,
                                weak_factory_.GetWeakPtr()))
        .OnAborted(base::BindOnce(&AppsGridView::OnIdealBoundsAnimationDone,
                                  weak_factory_.GetWeakPtr()))
        .Once()
        .SetDuration(kItemBoundsBaseAnimationDuration);
    return animation;
  };

  base::AutoReset<bool> auto_reset(&setting_up_ideal_bounds_animation_, true);
  const bool is_animating_multiple_rows = WillAnimateMultipleRows();

  // A duration which is incremented for cascading item animations.
  base::TimeDelta animation_duration = kItemBoundsBaseAnimationDuration;

  // Keeps track of the current slot/row for the current `animation_duration`.
  int animation_duration_index = -1;

  for (size_t i = 0; i < view_model_.view_size(); ++i) {
    // When not animating top to bottom, reverse the direction of iteration, so
    // bottom animating items have the shortest `animation_duration`.
    const size_t current_view_index =
        is_animating_top_to_bottom ? i : view_model_.view_size() - 1 - i;

    AppListItemView* view = GetItemViewAt(current_view_index);
    const gfx::Rect& target_bounds =
        view_model_.ideal_bounds(current_view_index);
    gfx::Rect current_bounds = view->GetMirroredBounds();

    if (view->bounds() == target_bounds) {
      continue;
    }

    const bool current_visible = visible_bounds.Intersects(current_bounds);
    const bool target_visible = visible_bounds.Intersects(target_bounds);
    const bool visible =
        !IsViewExplicitlyHidden(view) && (current_visible || target_visible);

    if (!visible) {
      view->SetBoundsRect(target_bounds);
      continue;
    }

    const int view_row = view->most_recent_grid_index().slot / cols_;
    const int view_slot = view->most_recent_grid_index().slot;
    // When animating multiple rows, each row of items will have an animation
    // duration that is increased at each new row. When animating items within a
    // single row, the duration will be increased at each new item slot.
    const int current_animation_duration_index =
        is_animating_multiple_rows ? view_row : view_slot;
    // Increment the `animation_duration` when the `animation_duration_index`
    // has been initialized and the current index has changed.
    if (animation_duration_index != -1 &&
        animation_duration_index != current_animation_duration_index) {
      animation_duration += kItemBoundsAnimationOffsetDuration;
    }
    animation_duration_index = current_animation_duration_index;

    if (view->has_pending_row_change()) {
      view->EnsureLayer();
      view->reset_has_pending_row_change();
      if (!animation) {
        animation = init_animation();
      }
      animation->GetCurrentSequence()
          .At(base::TimeDelta())
          .SetDuration(animation_duration);
      row_change_animator_->AnimateBetweenRows(
          view, current_bounds, target_bounds,
          &animation->GetCurrentSequence());
    } else {
      view->EnsureLayer();

      // Update `current_bounds` to include the current layer transform of
      // `view`.
      if (IsAnimatingView(view)) {
        current_bounds =
            ApplyTransformAtOrigin(current_bounds, view->layer()->transform());
      }

      gfx::Transform transform =
          gfx::TransformBetweenRects(gfx::RectF(GetMirroredRect(target_bounds)),
                                     gfx::RectF(current_bounds));
      view->layer()->SetTransform(transform);
      view->SetBoundsRect(target_bounds);

      if (!animation) {
        animation = init_animation();
      }
      animation->GetCurrentSequence()
          .At(base::TimeDelta())
          .SetDuration(animation_duration)
          .SetTransform(view->layer(), gfx::Transform(),
                        gfx::Tween::ACCEL_40_DECEL_100_3);
    }
  }
}

bool AppsGridView::WillAnimateMultipleRows() {
  for (size_t i = 0; i < view_model_.view_size(); ++i) {
    // Return true if an item will animate to a new row.
    if (GetItemViewAt(i)->has_pending_row_change()) {
      return true;
    }
  }
  return false;
}

void AppsGridView::ExtractDragLocation(const gfx::Point& root_location,
                                       gfx::Point* drag_point) {
  // Use root location of |event| instead of location in |drag_view_|'s
  // coordinates because |drag_view_| has a scale transform and location
  // could have integer round error and causes jitter.
  *drag_point = root_location;

  DCHECK(GetWidget());
  aura::Window::ConvertPointToTarget(
      GetWidget()->GetNativeWindow()->GetRootWindow(),
      GetWidget()->GetNativeWindow(), drag_point);
  views::View::ConvertPointFromWidget(this, drag_point);
}

void AppsGridView::UpdateDropTargetRegion() {
  DCHECK(drag_item_);

  gfx::Point point = last_drag_point_;
  point.set_x(GetMirroredXInView(point.x()));

  if (IsPointWithinDragBuffer(point)) {
    if (DragPointIsOverItem(point)) {
      drop_target_region_ = ON_ITEM;
      drop_target_ = GetNearestTileIndexForPoint(point);
      return;
    }

    UpdateDropTargetForReorder(point);
    drop_target_region_ = DragIsCloseToItem(point) ? NEAR_ITEM : BETWEEN_ITEMS;
    return;
  }

  // Reset the reorder target to the original position if the cursor is outside
  // the drag buffer or an item is dragged to a full page either from a folder
  // or another page.
  if (IsDraggingForReparentInRootLevelGridView()) {
    drop_target_region_ = NO_TARGET;
    return;
  }

  drop_target_ = drag_view_init_index_;
  drop_target_region_ = DragIsCloseToItem(point) ? NEAR_ITEM : BETWEEN_ITEMS;
}

bool AppsGridView::DropTargetIsValidFolder() {
  AppListItemView* target_view =
      GetViewDisplayedAtSlotOnCurrentPage(drop_target_.slot);
  if (!target_view) {
    return false;
  }

  AppListItem* target_item = target_view->item();

  // Items can only be dropped into non-folders (which have no children) or
  // folders that have fewer than the max allowed items.
  // The OEM folder does not allow drag/drop of other items into it.
  if (target_item->IsFolderFull() || IsOEMFolderItem(target_item)) {
    return false;
  }

  if (!IsValidIndex(drop_target_)) {
    return false;
  }

  return true;
}

bool AppsGridView::DragPointIsOverItem(const gfx::Point& point) {
  // The reorder placeholder shouldn't count as a unique item
  GridIndex nearest_tile_index(GetNearestTileIndexForPoint(point));
  if (!IsValidIndex(nearest_tile_index) ||
      nearest_tile_index == reorder_placeholder_) {
    return false;
  }

  int distance_to_tile_center =
      (point - GetExpectedTileBounds(nearest_tile_index).CenterPoint())
          .Length();
  if (distance_to_tile_center >
      (app_list_config_->folder_bubble_radius() *
       (cardified_state_ ? GetAppsGridCardifiedScale() : 1.0f))) {
    return false;
  }

  return true;
}

void AppsGridView::AnimateDragIconToTargetPosition(
    AppListItem* drag_item,
    const std::string& target_folder_id) {
  if (!drag_image_layer_) {
    OnDragIconDropDone();
    return;
  }

  AppListItemView* target_folder_view =
      !target_folder_id.empty() ? GetItemViewForItem(target_folder_id)
                                : nullptr;

  // Calculate target item bounds.
  gfx::Rect drag_icon_drop_bounds;
  if (target_folder_id.empty()) {
    // Find the view for drag item, and use its ideal bounds to calculate target
    // drop bounds.
    for (size_t i = 0; i < view_model_.view_size(); ++i) {
      if (view_model_.view_at(i)->item() != drag_item) {
        continue;
      }

      // Get the expected drag item view location.
      const gfx::Rect drag_view_ideal_bounds = view_model_.ideal_bounds(i);
      // Get icon bounds in the drag view coordinates.
      drag_icon_drop_bounds = AppListItemView::GetIconBoundsForTargetViewBounds(
          app_list_config_, drag_view_ideal_bounds,
          drag_item->is_folder() ? app_list_config_->folder_icon_size()
                                 : app_list_config_->grid_icon_size(),
          1.0f);

      break;
    }
  } else if (target_folder_view) {
    // Calculate target bounds of dragged item.
    drag_icon_drop_bounds =
        GetTargetIconRectInFolder(drag_item, target_folder_view);
  }

  // Unable to calculate target bounds - bail out and reshow the drag view.
  if (drag_icon_drop_bounds.IsEmpty()) {
    OnDragIconDropDone();
    return;
  }

  if (target_folder_view) {
    DCHECK(target_folder_view->is_folder());
    folder_icon_item_hider_ =
        std::make_unique<FolderIconItemHider>(target_folder_view, drag_item);
  }

  drag_icon_drop_bounds =
      items_container_->GetMirroredRect(drag_icon_drop_bounds);

  // Convert target bounds to in screen coordinates expected by drag icon proxy.
  views::View::ConvertRectToScreen(items_container_, &drag_icon_drop_bounds);

  // Ensure target bounds are in the same coordinates as the drag image layer.
  wm::ConvertRectFromScreen(
      items_container_->GetWidget()->GetNativeWindow()->GetRootWindow(),
      &drag_icon_drop_bounds);

  ui::Layer* target_layer = drag_image_layer_->root();
  if (target_layer) {
    target_layer->GetAnimator()->AbortAllAnimations();

    gfx::Rect current_bounds = target_layer->bounds();
    if (current_bounds.IsEmpty()) {
      OnDragIconDropDone();
      return;
    }

    ui::ScopedLayerAnimationSettings animation_settings(
        target_layer->GetAnimator());
    animation_settings.SetTweenType(gfx::Tween::FAST_OUT_LINEAR_IN);
    animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
    animation_settings.AddObserver(
        new AnimationObserverToRestoreGrid(base::BindOnce(
            &AppsGridView::OnDragIconDropDone, weak_factory_.GetWeakPtr())));

    target_layer->SetTransform(gfx::TransformBetweenRects(
        gfx::RectF(current_bounds), gfx::RectF(drag_icon_drop_bounds)));
  }
}

void AppsGridView::OnDragIconDropDone() {
  drag_view_hider_.reset();
  folder_icon_item_hider_.reset();
  drag_image_layer_.reset();
  DestroyLayerItemsIfNotNeeded();

  if (!folder_to_open_after_drag_icon_animation_.empty()) {
    AppListItemView* folder_view =
        GetItemViewForItem(folder_to_open_after_drag_icon_animation_);
    folder_to_open_after_drag_icon_animation_.clear();
    ShowFolderForView(folder_view, /*new_folder=*/true);
  }
}

bool AppsGridView::DraggedItemCanEnterFolder() {
  if (!IsFolderItem(drag_item_) && !folder_delegate_) {
    return true;
  }
  return false;
}

void AppsGridView::UpdateDropTargetForReorder(const gfx::Point& point) {
  gfx::Rect bounds = GetContentsBounds();
  bounds.Inset(GetTilePadding(GetSelectedPage()));
  GridIndex nearest_tile_index = GetNearestTileIndexForPoint(point);
  gfx::Point reorder_placeholder_center =
      GetExpectedTileBounds(reorder_placeholder_).CenterPoint();

  int x_offset_direction = 0;
  if (nearest_tile_index == reorder_placeholder_) {
    x_offset_direction = reorder_placeholder_center.x() <= point.x() ? -1 : 1;
  } else {
    x_offset_direction = reorder_placeholder_ < nearest_tile_index ? -1 : 1;
  }

  const gfx::Size total_tile_size = GetTotalTileSize(GetSelectedPage());
  int row = nearest_tile_index.slot / cols_;

  // Offset the target column based on the direction of the target. This will
  // result in earlier targets getting their reorder zone shifted backwards
  // and later targets getting their reorder zones shifted forwards.
  //
  // This makes reordering feel like the user is slotting items into the spaces
  // between apps.
  int x_offset = x_offset_direction *
                 (total_tile_size.width() / 2 -
                  app_list_config_->folder_bubble_radius() *
                      (cardified_state_ ? GetAppsGridCardifiedScale() : 1.0f));
  const int selected_page = GetSelectedPage();
  int col = (point.x() - bounds.x() + x_offset -
             GetGridCenteringOffset(selected_page).x()) /
            total_tile_size.width();
  col = std::clamp(col, 0, cols_ - 1);

  GridIndex max_target_index;
  if (selected_page == GetTotalPages() - 1) {
    // On the last page, cap the target index at the view model size.
    max_target_index = GetGridIndexFromIndexInViewModel(
        view_model()->view_size() -
        (HasExtraSlotForReorderPlaceholder() ? 0 : 1));
  } else {
    max_target_index =
        GridIndex(selected_page, *TilesPerPage(selected_page) - 1);
  }

  drop_target_ =
      std::min(GridIndex(selected_page, row * cols_ + col), max_target_index);

  DCHECK(IsValidIndex(drop_target_))
      << drop_target_.ToString() << " selected page " << selected_page
      << " row " << row << " col " << col << " " << max_target_index.ToString();
}

bool AppsGridView::DragIsCloseToItem(const gfx::Point& point) {
  DCHECK(drag_item_);

  GridIndex nearest_tile_index = GetNearestTileIndexForPoint(point);
  if (nearest_tile_index == reorder_placeholder_) {
    return false;
  }

  const int distance_to_tile_center =
      (point - GetExpectedTileBounds(nearest_tile_index).CenterPoint())
          .Length();

  // The minimum of |forty_percent_icon_spacing| and |double_icon_radius| is
  // chosen to give an acceptable spacing on displays of any resolution: when
  // items are very close together, using |forty_percent_icon_spacing| will
  // prevent overlap and leave a reasonable gap, whereas when icons are very far
  // apart, using |double_icon_radius| will prevent us from juding an overly
  // large region as 'nearby'
  const int forty_percent_icon_spacing =
      (app_list_config_->grid_tile_width() + horizontal_tile_padding_ * 2) *
      0.4 * (cardified_state_ ? GetAppsGridCardifiedScale() : 1.0f);
  const int double_icon_radius =
      app_list_config_->folder_bubble_radius() * 2 *
      (cardified_state_ ? GetAppsGridCardifiedScale() : 1.0f);
  const int minimum_drag_distance_for_reorder =
      std::min(forty_percent_icon_spacing, double_icon_radius);

  if (distance_to_tile_center < minimum_drag_distance_for_reorder) {
    return true;
  }
  return false;
}

void AppsGridView::OnReorderTimer() {
  const GridIndex before_index = reorder_placeholder_;
  reorder_placeholder_ = drop_target_;
  const GridIndex after_index = reorder_placeholder_;
  MaybeCreateDragReorderAccessibilityEvent();
  AnimateToIdealBounds(/*top to bottom animation=*/before_index < after_index);
  CreateGhostImageView();
}

void AppsGridView::OnFolderItemReparentTimer(Pointer pointer) {
  DCHECK(folder_delegate_);
  if (drag_out_of_folder_container_ && drag_view_) {
    // Set the flag in the folder's grid view.
    dragging_for_reparent_item_ = true;

    // Do not observe any data change since it is going to be hidden.
    item_list_->RemoveObserver(this);
    item_list_ = nullptr;
  }
}

void AppsGridView::UpdateDragStateInsideFolder(Pointer pointer,
                                               const gfx::Point& drag_point) {
  if (IsUnderOEMFolder()) {
    return;
  }

  // Calculate if the drag_view_ is dragged out of the folder's container
  // ink bubble.
  bool is_item_dragged_out_of_folder =
      folder_delegate_->IsDragPointOutsideOfFolder(drag_point);
  if (is_item_dragged_out_of_folder) {
    if (!drag_out_of_folder_container_) {
      folder_item_reparent_timer_.Start(
          FROM_HERE, base::Milliseconds(kFolderItemReparentDelay),
          base::BindOnce(&AppsGridView::OnFolderItemReparentTimer,
                         base::Unretained(this), pointer));
      drag_out_of_folder_container_ = true;
    }
  } else {
    folder_item_reparent_timer_.Stop();
    drag_out_of_folder_container_ = false;
  }
}

bool AppsGridView::IsDraggingForReparentInRootLevelGridView() const {
  return (!folder_delegate_ && dragging_for_reparent_item_);
}

bool AppsGridView::IsDraggingForReparentInHiddenGridView() const {
  return (folder_delegate_ && dragging_for_reparent_item_);
}

gfx::Rect AppsGridView::GetTargetIconRectInFolder(
    AppListItem* drag_item,
    AppListItemView* folder_item_view) {
  const gfx::Rect view_ideal_bounds = view_model_.ideal_bounds(
      view_model_.GetIndexOfView(folder_item_view).value());
  const gfx::Rect icon_ideal_bounds =
      folder_item_view->GetIconBoundsForTargetViewBounds(
          app_list_config_, view_ideal_bounds,
          folder_item_view->GetDragImage().size(), /*icon_scale=*/1.0f);
  AppListFolderItem* folder_item = folder_item_view->item()->AsFolderItem();
  return folder_item->GetTargetIconRectInFolderForItem(
      *app_list_config_, drag_item, icon_ideal_bounds);
}

bool AppsGridView::IsUnderOEMFolder() {
  if (!folder_delegate_) {
    return false;
  }

  return folder_delegate_->IsOEMFolder();
}

void AppsGridView::HandleKeyboardAppOperations(ui::KeyboardCode key_code,
                                               bool folder) {
  DCHECK(selected_view_);

  // Do not allow keyboard operations during drag.
  if (drag_view_) {
    return;
  }

  if (folder) {
    if (folder_delegate_)
      folder_delegate_->HandleKeyboardReparent(selected_view_, key_code);
    else
      HandleKeyboardFoldering(key_code);
  } else {
    HandleKeyboardMove(key_code);
  }
}

void AppsGridView::HandleKeyboardFoldering(ui::KeyboardCode key_code) {
  const GridIndex source_index = GetIndexOfView(selected_view_);
  const GridIndex target_index = GetTargetGridIndexForKeyboardMove(key_code);
  if (!CanMoveSelectedToTargetForKeyboardFoldering(target_index)) {
    return;
  }

  const std::u16string moving_view_title = selected_view_->title()->GetText();
  AppListItemView* target_view =
      GetViewDisplayedAtSlotOnCurrentPage(target_index.slot);
  const std::u16string target_view_title = target_view->title()->GetText();
  const bool target_view_is_folder = target_view->is_folder();

  std::string folder_id;
  bool is_new_folder = false;
  if (MoveItemToFolder(selected_view_->item(), target_index,
                       kMoveByKeyboardIntoFolder, &folder_id, &is_new_folder)) {
    a11y_announcer_->AnnounceKeyboardFoldering(
        moving_view_title, target_view_title, target_view_is_folder);
    AppListItemView* folder_view = GetItemViewForItem(folder_id);
    if (folder_view) {
      if (is_new_folder) {
        SetOpenFolderInfo(folder_id, target_index, source_index);
        ShowFolderForView(folder_view, /*new_folder=*/true);
      } else {
        folder_view->RequestFocus();
      }
    }

    // Layout the grid to ensure the created folder's bounds are correct.
    // Note that `open_folder_info_` affects ideal item bounds, so
    // `DeprecatedLayoutImmediately()` needs to be called after
    // `SetOpenFolderInfo()`.
    DeprecatedLayoutImmediately();
    UpdatePaging();
  }
}

bool AppsGridView::CanMoveSelectedToTargetForKeyboardFoldering(
    const GridIndex& target_index) const {
  DCHECK(selected_view_);

  // To folder an item, the item must be moved into the folder, not the folder
  // moved over the item.
  const AppListItem* selected_item = selected_view_->item();
  if (selected_item->is_folder()) {
    return false;
  }

  // Do not allow foldering across pages because the destination folder cannot
  // be seen.
  if (target_index.page != GetIndexOfView(selected_view_).page) {
    return false;
  }

  return true;
}

bool AppsGridView::HandleVerticalFocusMovement(bool arrow_up) {
  views::View* focused = GetFocusManager()->GetFocusedView();
  if (!views::IsViewClass<AppListItemView>(focused)) {
    return false;
  }

  const GridIndex source_index =
      GetIndexOfView(static_cast<const AppListItemView*>(focused));
  int target_page = source_index.page;
  int target_row = source_index.slot / cols_ + (arrow_up ? -1 : 1);
  int target_col = source_index.slot % cols_;

  if (target_row < 0) {
    // Move focus to the last row of previous page if target row is negative.
    --target_page;

    // |target_page| may be invalid which makes |target_row| invalid, but
    // |target_row| will not be used if |target_page| is invalid.
    target_row = (GetNumberOfItemsOnPage(target_page) - 1) / cols_;
  } else if (target_row > (GetNumberOfItemsOnPage(target_page) - 1) / cols_) {
    // Move focus to the first row of next page if target row is beyond range.
    ++target_page;
    target_row = 0;
  }

  if (target_page < 0) {
    // Move focus up outside the apps grid if target page is negative.
    if (keyboard_controller_ &&
        keyboard_controller_->MoveFocusUpFromAppsGrid(target_col)) {
      // The delegate handled the focus move.
      return true;
    }
    // Move focus backwards from the first item in the grid.
    views::View* v = GetFocusManager()->GetNextFocusableView(
        view_model_.view_at(0), /*starting_widget=*/nullptr, /*reverse=*/true,
        /*dont_loop=*/false);
    DCHECK(v);
    v->RequestFocus();
    return true;
  }

  if (target_page >= GetTotalPages()) {
    // Move focus down outside the apps grid if target page is beyond range.
    views::View* v = GetFocusManager()->GetNextFocusableView(
        view_model_.view_at(view_model_.view_size() - 1),
        /*starting_widget=*/nullptr, /*reverse=*/false,
        /*dont_loop=*/false);
    DCHECK(v);
    v->RequestFocus();
    return true;
  }

  GridIndex target_index(target_page, target_row * cols_ + target_col);

  // Ensure the focus is within the range of the target page.
  target_index.slot =
      std::min(GetNumberOfItemsOnPage(target_page) - 1, target_index.slot);
  if (IsValidIndex(target_index)) {
    GetViewAtIndex(target_index)->RequestFocus();
    return true;
  }
  return false;
}

void AppsGridView::UpdateColsAndRowsForFolder() {
  if (!folder_delegate_) {
    return;
  }

  const int item_count = item_list_ ? item_list_->item_count() : 0;

  // Ensure that there is always at least one column.
  if (item_count == 0) {
    cols_ = 1;
  } else {
    int preferred_cols = std::sqrt(item_list_->item_count() - 1) + 1;
    cols_ = std::clamp(preferred_cols, 1, max_cols_);
  }

  PreferredSizeChanged();
}

void AppsGridView::EndDragFromReparentItemInRootLevel(
    AppListItemView* original_parent_item_view,
    bool events_forwarded_to_drag_drop_host,
    bool cancel_drag) {
  DCHECK(!IsInFolder());

  // EndDrag was called before if |drag_view_| is nullptr.
  if (!drag_item_) {
    return;
  }

  AppListItem* drag_item = drag_item_;

  DCHECK(IsDraggingForReparentInRootLevelGridView());
  bool cancel_reparent = cancel_drag || drop_target_region_ == NO_TARGET;

  // The ID of the folder to which the item gets dropped. It will get set when
  // the item is moved to a folder. It will be set the to original folder ID if
  // reparent is canceled.
  std::string target_folder_id;

  // Cache the original item folder id, as model updates may destroy the
  // original folder item.
  const std::string original_folder_id = drag_item_->folder_id();

  if (!events_forwarded_to_drag_drop_host && !cancel_reparent) {
    UpdateDropTargetRegion();
    if (drop_target_region_ == ON_ITEM && DropTargetIsValidFolder() &&
        DraggedItemCanEnterFolder()) {
      bool is_new_folder = false;
      if (MoveItemToFolder(drag_item, drop_target_, kMoveIntoAnotherFolder,
                           &target_folder_id, &is_new_folder)) {
        // Announce folder dropping event before end of drag of reparented item.
        MaybeCreateFolderDroppingAccessibilityEvent();
        // If move to folder created a folder, layout the grid to ensure the
        // created folder's bounds are correct.
        DeprecatedLayoutImmediately();
        if (is_new_folder) {
          folder_to_open_after_drag_icon_animation_ = target_folder_id;
          SetOpenFolderInfo(target_folder_id, drop_target_,
                            reorder_placeholder_);
        }
      } else {
        cancel_reparent = true;
      }
    } else if (drop_target_region_ != NO_TARGET && IsValidIndex(drop_target_)) {
      ReparentItemForReorder(drag_item, drop_target_);
      RecordAppMovingTypeMetrics(kMoveByDragOutOfFolder);
      // Announce accessibility event before the end of drag for reparented
      // item.
      MaybeCreateDragReorderAccessibilityEvent();
    } else {
      NOTREACHED();
    }
  }

  if (cancel_reparent) {
    target_folder_id = original_folder_id;
  }

  SetAsFolderDroppingTarget(drop_target_, false);

  const GridIndex before_index = reorder_placeholder_;
  const GridIndex after_index = drop_target_;
  const bool top_to_bottom_animation = before_index < after_index;

  ClearDragState();
  UpdatePaging();
  if (GetWidget()) {
    // Normally layout cancels any animations. At this point there may be a
    // pending layout, force it now so that one isn't triggered part way through
    // the animation. Further, ignore this layout so that the position isn't
    // reset.
    DCHECK(!ignore_layout_);
    base::AutoReset<bool> auto_reset(&ignore_layout_, true);
    GetWidget()->LayoutRootViewIfNecessary();
  }

  if (cardified_state_)
    MaybeEndCardifiedView();
  else
    AnimateToIdealBounds(top_to_bottom_animation);

  // Hide the |current_ghost_view_| after completed drag from within
  // folder to |apps_grid_view_|.
  BeginHideCurrentGhostImageView();
  SetFocusAfterEndDrag(drag_item);  // Maybe focus the search box.

  AnimateDragIconToTargetPosition(drag_item, target_folder_id);
}

void AppsGridView::EndDragForReparentInHiddenFolderGridView() {
  SetAsFolderDroppingTarget(drop_target_, false);
  ClearDragState();

  // Hide |current_ghost_view_| in the hidden folder grid view.
  BeginHideCurrentGhostImageView();
}

void AppsGridView::HandleKeyboardReparent(
    AppListItemView* reparented_view,
    AppListItemView* original_parent_item_view,
    ui::KeyboardCode key_code) {
  DCHECK(key_code == ui::VKEY_LEFT || key_code == ui::VKEY_RIGHT ||
         key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN);
  DCHECK(!folder_delegate_);
  DCHECK(view_model_.GetIndexOfView(original_parent_item_view).has_value());

  const std::string reparented_item_id = reparented_view->item()->id();

  // Set |original_parent_item_view| selected so |target_index| will be
  // computed relative to the open folder.
  SetSelectedView(original_parent_item_view);
  const GridIndex target_index = GetTargetGridIndexForKeyboardReparent(
      GetIndexOfView(original_parent_item_view), key_code);
  ReparentItemForReorder(reparented_view->item(), target_index);

  // `target_index` could point to an invalid/wrong position after reparenting.
  // This happens after trying to move the last item from the folder
  // to the right (`target_index` is "folder index + 1", but after reparenting
  // it actually moves one position back).
  const AppListItem* const item_after_reparent =
      item_list_->FindItem(reparented_item_id);
  DCHECK(item_after_reparent);
  const int final_model_index = GetModelIndexOfItem(item_after_reparent);
  const GridIndex final_grid_index =
      GetGridIndexFromIndexInViewModel(final_model_index);

  // Update paging because the move could have resulted in a
  // page getting created.
  UpdatePaging();

  DeprecatedLayoutImmediately();
  EnsureViewVisible(final_grid_index);
  GetViewAtIndex(final_grid_index)->RequestFocus();
  AnnounceReorder(final_grid_index);

  RecordAppMovingTypeMetrics(kMoveByKeyboardOutOfFolder);
}

bool AppsGridView::IsTabletMode() const {
  return app_list_view_delegate_->IsInTabletMode();
}

views::AnimationBuilder AppsGridView::FadeOutVisibleItemsForReorder(
    ReorderAnimationCallback done_callback) {
  // The caller of this function is responsible for aborting the old reorder
  // process before starting a new one.
  DCHECK(!IsUnderWholeGridAnimation());

  // Cancel the active bounds animations on item views if any.
  CancelAllItemAnimations();

  grid_animation_status_ = AppListGridAnimationStatus::kReorderFadeOut;
  reorder_animation_tracker_.emplace(
      layer()->GetCompositor()->RequestNewThroughputTracker());
  reorder_animation_tracker_->Start(metrics_util::ForSmoothnessV3(
      base::BindRepeating(&ReportReorderAnimationSmoothness, IsTabletMode())));

  views::AnimationBuilder animation_builder;
  grid_animation_abort_handle_ = animation_builder.GetAbortHandle();

  if (fade_out_start_closure_for_test_)
    animation_builder.OnStarted(std::move(fade_out_start_closure_for_test_));

  // Set the preemption strategy to be `IMMEDIATELY_ANIMATE_TO_NEW_TARGET` so
  // that if there is an existing apps grid animation, fade out animation for
  // reorder is still going to run.
  animation_builder
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(&AppsGridView::OnFadeOutAnimationEnded,
                              weak_factory_.GetWeakPtr(), done_callback,
                              /*abort=*/false))
      .OnAborted(base::BindOnce(&AppsGridView::OnFadeOutAnimationEnded,
                                weak_factory_.GetWeakPtr(), done_callback,
                                /*abort=*/true))
      .Once()
      .SetDuration(kFadeOutAnimationDuration)
      .SetOpacity(layer(), 0.f, gfx::Tween::LINEAR);
  return animation_builder;
}

views::AnimationBuilder AppsGridView::FadeInVisibleItemsForReorder(
    ReorderAnimationCallback done_callback) {
  DCHECK_EQ(AppListGridAnimationStatus::kReorderIntermediaryState,
            grid_animation_status_);
  DCHECK(!IsItemAnimationRunning());

  // When `AppsGridView::OnListItemMoved()` is called due to item reorder,
  // the layout updates asynchronously. Meanwhile, calculating the visible item
  // range needs the up-to-date layout. Therefore update the layout explicitly
  // before calculating `range`.
  if (needs_layout()) {
    DeprecatedLayoutImmediately();
  }

  grid_animation_status_ = AppListGridAnimationStatus::kReorderFadeIn;
  const std::optional<VisibleItemIndexRange> range = GetVisibleItemIndexRange();

  views::AnimationBuilder animation_builder;

  // No items to be sorted are visible - return an empty animation builder that
  // ends immediately.
  if (!range) {
    animation_builder
        .OnEnded(base::BindOnce(&AppsGridView::OnFadeInAnimationEnded,
                                weak_factory_.GetWeakPtr(), done_callback,
                                /*abort=*/true))
        .OnAborted(base::BindOnce(&AppsGridView::OnFadeInAnimationEnded,
                                  weak_factory_.GetWeakPtr(), done_callback,
                                  /*abort=*/true))
        .Once()
        .SetDuration(base::TimeDelta());
    return animation_builder;
  }

  // Only show the visible items during animation to reduce the cost of painting
  // that is triggered by view bounds changes due to reorder.
  for (size_t visible_view_index = range->first_index;
       visible_view_index <= range->last_index; ++visible_view_index) {
    view_model_.view_at(visible_view_index)->SetVisible(true);
  }

  grid_animation_abort_handle_ = animation_builder.GetAbortHandle();
  animation_builder
      .OnEnded(base::BindOnce(&AppsGridView::OnFadeInAnimationEnded,
                              weak_factory_.GetWeakPtr(), done_callback,
                              /*abort=*/false))
      .OnAborted(base::BindOnce(&AppsGridView::OnFadeInAnimationEnded,
                                weak_factory_.GetWeakPtr(), done_callback,
                                /*abort=*/true))
      .Once()
      .SetDuration(kFadeInAnimationDuration)
      .SetOpacity(layer(), 1.f, gfx::Tween::ACCEL_5_70_DECEL_90);

  // Assume all the items matched by the indices in `range` are
  // placed on the same page.
  const int page_index =
      GetGridIndexFromIndexInViewModel(range->first_index).page;
  const int base_offset =
      kFadeAnimationOffsetRatio * GetTotalTileSize(page_index).height();

  // The row of the first visible item.
  const int base_row = range->first_index / cols_;

  for (size_t visible_view_index = range->first_index;
       visible_view_index <= range->last_index; ++visible_view_index) {
    // Calculate translate offset for each view. NOTE: The items on the
    // different rows have different fade in offsets. The ratio between the
    // offset and `base_offset` is (relative_row_index + 2).
    const int relative_row_index = visible_view_index / cols_ - base_row;
    const int offset = (relative_row_index + 2) * base_offset;

    views::View* animated_view = GetItemViewAt(visible_view_index);
    PrepareForLayerAnimation(animated_view);

    // Create a slide animation on `animted_view` using `sequence_block`'s
    // existing time duration.
    SlideViewIntoPositionWithSequenceBlock(
        animated_view, offset,
        /*time_delta=*/std::nullopt, gfx::Tween::ACCEL_5_70_DECEL_90,
        &animation_builder.GetCurrentSequence());
  }

  return animation_builder;
}

void AppsGridView::SlideVisibleItemsForHideContinueSection(int base_offset) {
  DCHECK(IsTabletMode());  // This animation is only used in tablet mode.

  if (needs_layout()) {
    DeprecatedLayoutImmediately();
  }

  const std::optional<VisibleItemIndexRange> range = GetVisibleItemIndexRange();

  // Safety check, unlikely in production.
  if (!range) {
    return;
  }

  // The continue section is on the 0th page. Don't animate if a different page
  // is selected.
  if (GetGridIndexFromIndexInViewModel(range->first_index).page != 0) {
    return;
  }

  grid_animation_status_ = AppListGridAnimationStatus::kHideContinueSection;

  views::AnimationBuilder animation_builder;
  grid_animation_abort_handle_ = animation_builder.GetAbortHandle();
  animation_builder
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(
          base::BindOnce(&AppsGridView::OnHideContinueSectionAnimationEnded,
                         weak_factory_.GetWeakPtr()))
      .OnAborted(
          base::BindOnce(&AppsGridView::OnHideContinueSectionAnimationEnded,
                         weak_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(base::Milliseconds(300));

  // Animate each row of app icons with a different offset.
  for (size_t item_index = range->first_index; item_index <= range->last_index;
       ++item_index) {
    const int row_index = item_index / cols_;

    // The 0th row animates base_offset * 3 / 4
    // The 1st row animates base_offset * 2 / 4
    // The 2nd row animates base_offset * 1 / 4
    const int vertical_offset = std::max(0, base_offset * (3 - row_index) / 4);

    // Ensure each icon view has a layer. These are cleaned up on animation end.
    views::View* icon = GetItemViewAt(item_index);
    PrepareForLayerAnimation(icon);

    // Slide each icon into position.
    SlideViewIntoPositionWithSequenceBlock(
        icon, vertical_offset, /*time_delta=*/std::nullopt,
        gfx::Tween::ACCEL_LIN_DECEL_100_3,
        &animation_builder.GetCurrentSequence());
  }
}

void AppsGridView::OnHideContinueSectionAnimationEnded() {
  grid_animation_status_ = AppListGridAnimationStatus::kEmpty;

  // Clean up the layers created for the app icon views.
  DestroyLayerItemsIfNotNeeded();
}

bool AppsGridView::IsItemAnimationRunning() const {
  for (size_t i = 0; i < view_model_.view_size(); ++i) {
    AppListItemView* view = GetItemViewAt(i);
    if (IsAnimatingView(view)) {
      return true;
    }
  }
  return false;
}

void AppsGridView::CancelAllItemAnimations() {
  // Collect layers and stop animating in another pass to reduce risk of view
  // changes to `view_model_` during iteration.
  std::vector<ui::Layer*> item_layers;
  for (size_t i = 0; i < view_model_.view_size(); ++i) {
    AppListItemView* view = GetItemViewAt(i);
    if (IsAnimatingView(view)) {
      item_layers.push_back(view->layer());
    }
  }
  for (auto* layer : item_layers) {
    layer->GetAnimator()->StopAnimating();
  }
}

void AppsGridView::AddReorderCallbackForTest(
    TestReorderDoneCallbackType done_callback) {
  DCHECK(done_callback);

  reorder_animation_callback_queue_for_test_.push(std::move(done_callback));
}

void AppsGridView::AddFadeOutAnimationStartClosureForTest(
    base::OnceClosure start_closure) {
  DCHECK(start_closure);
  DCHECK(!fade_out_done_closure_for_test_);

  fade_out_start_closure_for_test_ = std::move(start_closure);
}

void AppsGridView::AddFadeOutAnimationDoneClosureForTest(
    base::OnceClosure done_closure) {
  DCHECK(done_closure);
  DCHECK(!fade_out_done_closure_for_test_);

  fade_out_done_closure_for_test_ = std::move(done_closure);
}

bool AppsGridView::HasAnyWaitingReorderDoneCallbackForTest() const {
  return !reorder_animation_callback_queue_for_test_.empty();
}

void AppsGridView::MoveItemInModel(AppListItem* item, const GridIndex& target) {
  const std::string item_id = item->id();

  size_t current_item_list_index = 0;
  bool found = item_list_->FindItemIndex(item_id, &current_item_list_index);
  CHECK(found);

  size_t target_item_list_index = GetIndexInViewModel(target);
  {
    ScopedModelUpdate update(this);
    item_list_->MoveItem(current_item_list_index, target_item_list_index);
  }
}

bool AppsGridView::MoveItemToFolder(AppListItem* item,
                                    const GridIndex& target,
                                    AppListAppMovingType move_type,
                                    std::string* folder_id,
                                    bool* is_new_folder) {
  const std::string source_item_id = item->id();
  const std::string source_folder_id = item->folder_id();

  AppListItemView* target_view =
      GetViewDisplayedAtSlotOnCurrentPage(target.slot);
  DCHECK(target_view);
  const std::string target_view_item_id = target_view->item()->id();

  // An app is being reparented to its original folder. Just cancel the
  // reparent.
  if (target_view_item_id == source_folder_id) {
    return false;
  }

  *is_new_folder = !target_view->is_folder();

  {
    ScopedModelUpdate update(this);
    *folder_id = model_->MergeItems(target_view_item_id, source_item_id);
  }

  if (folder_id->empty()) {
    LOG(ERROR) << "Unable to merge into item id: " << target_view_item_id;
    return false;
  }

  if (*is_new_folder)
    base::RecordAction(base::UserMetricsAction("AppList_CreateFolder"));

  MaybeRecordFolderDeleteUserAction(source_folder_id);
  RecordAppMovingTypeMetrics(move_type);
  return true;
}

void AppsGridView::ReparentItemForReorder(AppListItem* item,
                                          const GridIndex& target) {
  DCHECK(item->IsInFolder());

  const std::string item_id = item->id();
  const std::string source_folder_id = item->folder_id();
  int target_item_index = GetIndexInViewModel(target);

  // Move the item from its parent folder to top level item list. Calculate the
  // target position in the top level list.
  syncer::StringOrdinal target_position;
  if (target_item_index < static_cast<int>(item_list_->item_count()))
    target_position = item_list_->item_at(target_item_index)->position();

  {
    ScopedModelUpdate update(this);
    model_->MoveItemToRootAt(item, target_position);
  }

  MaybeRecordFolderDeleteUserAction(source_folder_id);
}

void AppsGridView::MaybeRecordFolderDeleteUserAction(
    const std::string& folder_id) {
  // Ignore the top-level grid (which isn't a folder and can't be deleted).
  if (folder_id.empty()) {
    return;
  }

  // If the folder disappeared from the model, record a user action.
  if (!model_->FindFolderItem(folder_id))
    base::RecordAction(base::UserMetricsAction("AppList_DeleteFolder"));
}

void AppsGridView::CancelContextMenusOnCurrentPage() {
  GridIndex start_index(GetSelectedPage(), 0);
  if (!IsValidIndex(start_index)) {
    return;
  }
  const size_t start = GetIndexInViewModel(start_index);
  const std::optional<int> tiles_per_page = TilesPerPage(start_index.page);
  const size_t end = tiles_per_page ? std::min(view_model_.view_size(),
                                               start + *tiles_per_page)
                                    : view_model_.view_size();
  for (size_t i = start; i < end; ++i) {
    GetItemViewAt(i)->CancelContextMenu();
  }
}

void AppsGridView::DeleteItemViewAtIndex(size_t index) {
  AppListItemView* item_view = GetItemViewAt(index);
  view_model_.Remove(index);
  if (item_view == drag_view_) {
    drag_view_ = nullptr;
  }
  if (open_folder_info_ &&
      open_folder_info_->item_id == item_view->item()->id()) {
    open_folder_info_.reset();
  }
  delete item_view;
}

bool AppsGridView::IsPointWithinDragBuffer(const gfx::Point& point) const {
  gfx::Rect rect(GetLocalBounds());
  rect.Inset(-kDragBufferPx);
  return rect.Contains(point);
}

void AppsGridView::ScheduleLayout(const gfx::Size& previous_grid_size) {
  if (GetTileGridSize() != previous_grid_size) {
    PreferredSizeChanged();  // Calls InvalidateLayout() internally.
  } else {
    InvalidateLayout();
  }
  DCHECK(needs_layout());
}

void AppsGridView::OnListItemAdded(size_t index, AppListItem* item) {
  const gfx::Size initial_grid_size = GetTileGridSize();

  if (!updating_model_) {
    EndDrag(true);
  }

  // Abort reorder animation before a view is added to `view_model_`.
  MaybeAbortWholeGridAnimation();

  AppListItemView* view = MaybeSwapPlaceholderAsset(index);

  if (item == drag_item_) {
    drag_view_ = view;
    drag_view_hider_ = std::make_unique<DragViewHider>(drag_view_);
  }
  view->InitializeIconLoader();

  // If model update is in progress, paging should be updated when the operation
  // that caused the model update completes.
  if (!updating_model_) {
    UpdatePaging();
    UpdateColsAndRowsForFolder();
    UpdatePulsingBlockViews();
  }

  // Schedule a layout, since the grid items may need their bounds updated.
  ScheduleLayout(initial_grid_size);

  items_container_->NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                                             /*send_native_event=*/true);

  // Attempt to animate the transition from a promise app into an actual app
  if (item->GetMetadata()->app_status == AppStatus::kReady) {
    std::string package_name = view->item()->GetMetadata()->promise_package_id;
    auto found = pending_promise_apps_removals_.find(package_name);
    if (found != pending_promise_apps_removals_.end()) {
      view->AnimateInFromPromiseApp(
          found->second,
          base::BindRepeating(&AppsGridView::FinishAnimationForPromiseApps,
                              weak_factory_.GetWeakPtr(),
                              std::move(package_name)));
    }
  }
}

void AppsGridView::FinishAnimationForPromiseApps(
    const std::string& pending_app_id) {
  PendingAppsMap::iterator pending_app_found =
      pending_promise_apps_removals_.find(pending_app_id);

  // Discard the pending promise app layer.
  if (pending_app_found != pending_promise_apps_removals_.end()) {
    auto pending_app_scope(std::move(pending_app_found->second));
    pending_promise_apps_removals_.erase(pending_app_found);
  }

  DestroyLayerItemsIfNotNeeded();
}

void AppsGridView::OnListItemRemoved(size_t index, AppListItem* item) {
  const gfx::Size initial_grid_size = GetTileGridSize();

  if (!updating_model_) {
    EndDrag(true);
  }

  MaybeDuplicatePromiseAppForRemoval(GetItemViewAt(index));

  // Abort reorder animation before a view is deleted from `view_model_`.
  MaybeAbortWholeGridAnimation();

  DeleteItemViewAtIndex(GetModelIndexOfItem(item));

  // If model update is in progress, paging should be updated when the operation
  // that caused the model update completes.
  if (!updating_model_) {
    UpdatePaging();
    UpdateColsAndRowsForFolder();
    UpdatePulsingBlockViews();
  }

  // Schedule a layout, since the grid items may need their bounds updated.
  ScheduleLayout(initial_grid_size);

  items_container_->NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                                             /*send_native_event=*/true);
}

void AppsGridView::MaybeDuplicatePromiseAppForRemoval(
    AppListItemView* promise_app_view) {
  if (!ash::features::ArePromiseIconsEnabled()) {
    return;
  }

  if (!promise_app_view || !promise_app_view->is_promise_app()) {
    return;
  }

  AppListItem* item = promise_app_view->item();

  if (item->app_status() != AppStatus::kInstallSuccess ||
      !promise_app_view->IsDrawn()) {
    return;
  }

  bool existing_app_in_grid = false;
  // Search along the `view_model_` for an existing app with the same
  // package id as the promise app to be removed.
  for (const auto& entry : view_model_.entries()) {
    AppListItemView* view = views::AsViewClass<AppListItemView>(entry.view);
    if (view == promise_app_view) {
      continue;
    }

    if (view->item()->GetMetadata()->promise_package_id == item->id()) {
      existing_app_in_grid = true;
      break;
    }
  }

  // PromiseApps don't get animation for removal if an app already existst in
  // the grid.
  if (!existing_app_in_grid) {
    AddPendingPromiseAppRemoval(item->id(),
                                promise_app_view->icon_image_model());
  }
}

void AppsGridView::OnListItemMoved(size_t from_index,
                                   size_t to_index,
                                   AppListItem* item) {
  // Abort reorder animation if the apps grid is updated by the user.
  if (!updating_model_) {
    MaybeAbortWholeGridAnimation();

    EndDrag(true);
  }

  // The item is updated in the item list but the view_model is not updated,
  // so get current model index by looking up view_model and predict the
  // target model index based on its current item index.
  size_t from_model_index = GetModelIndexOfItem(item);
  view_model_.Move(from_model_index, to_index);
  items_container_->ReorderChildView(view_model_.view_at(to_index), to_index);
  items_container_->NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                                             true /* send_native_event */);

  // If model update is in progress, paging should be updated when the operation
  // that caused the model update completes.
  if (!updating_model_) {
    UpdatePaging();
    UpdateColsAndRowsForFolder();
    UpdatePulsingBlockViews();
  }

  if (!updating_model_ && GetWidget() && GetWidget()->IsVisible() &&
      enable_item_move_animation_) {
    AnimateToIdealBounds(/*top to bottom animation=*/from_index < to_index);
  } else if (IsUnderWholeGridAnimation()) {
    // During reorder animation, multiple items could be moved subsequently so
    // use the asynchronous layout to reduce painting cost.
    InvalidateLayout();
  } else {
    DeprecatedLayoutImmediately();
  }
}

void AppsGridView::AddPendingPromiseAppRemoval(
    const std::string& id,
    const ui::ImageModel& default_image) {
  auto found = pending_promise_apps_removals_.find(id);
  if (found != pending_promise_apps_removals_.end()) {
    // A promise app might share app id with other apps in the same package.
    // If a promise app removal is already scheduled to be removed for this
    // package, just return normally.
    return;
  }

  pending_promise_apps_removals_.emplace(id, default_image);
}

void AppsGridView::OnAppListModelStatusChanged() {
  UpdatePulsingBlockViews();
  InvalidateLayout();
}

void AppsGridView::DestroyLayerItemsIfNotNeeded() {
  if (ItemViewsRequireLayers()) {
    return;
  }

  for (const auto& entry : view_model_.entries()) {
    AppListItemView* view = views::AsViewClass<AppListItemView>(entry.view);
    // When the item view has finished animating, then also delete the row
    // change layer if possible.
    row_change_animator_->CancelAnimation(view);
    if (!view->AlwaysPaintsToLayer()) {
      view->DestroyLayer();
    }
  }
}

bool AppsGridView::ItemViewsRequireLayers() const {
  // Layers required for app list item move animations during drag (to make room
  // for the current placeholder).
  if (drag_item_ || drag_image_layer_) {
    return true;
  }

  // Bounds animations are in progress, which use layers to animate transforms.
  if (IsItemAnimationRunning()) {
    return true;
  }

  // Reorder animation animate app list item layers.
  if (IsUnderWholeGridAnimation()) {
    return true;
  }

  // Folder position is changing after folder closure - this involves animating
  // folder item view layer out and in, and changing other view's bounds.
  if (reordering_folder_view_) {
    return true;
  }

  if (setting_up_ideal_bounds_animation_) {
    return true;
  }

  if (IsAnimatingCardifiedState()) {
    return true;
  }

  return false;
}

GridIndex AppsGridView::GetNearestTileIndexForPoint(
    const gfx::Point& point) const {
  gfx::Rect bounds = GetContentsBounds();
  const int current_page = GetSelectedPage();
  bounds.Inset(GetTilePadding(current_page));
  const gfx::Size total_tile_size = GetTotalTileSize(current_page);
  const gfx::Vector2d grid_offset = GetGridCenteringOffset(current_page);

  DCHECK_GT(total_tile_size.width(), 0);
  int col = std::clamp(
      (point.x() - bounds.x() - grid_offset.x()) / total_tile_size.width(), 0,
      cols_ - 1);

  DCHECK_GT(total_tile_size.height(), 0);
  const int ideal_row =
      (point.y() - bounds.y() - grid_offset.y()) / total_tile_size.height();
  const std::optional<int> tiles_per_page = TilesPerPage(current_page);
  const int row = tiles_per_page
                      ? std::clamp(ideal_row, 0, *tiles_per_page / cols_ - 1)
                      : std::max(ideal_row, 0);
  return GridIndex(current_page, row * cols_ + col);
}

gfx::Rect AppsGridView::GetExpectedTileBounds(const GridIndex& index) const {
  if (!cols_) {
    return gfx::Rect();
  }

  gfx::Rect bounds(GetContentsBounds());
  bounds.Inset(GetTilePadding(index.page));
  int row = index.slot / cols_;
  int col = index.slot % cols_;
  const gfx::Size total_tile_size = GetTotalTileSize(index.page);
  gfx::Rect tile_bounds(gfx::Point(bounds.x() + col * total_tile_size.width(),
                                   bounds.y() + row * total_tile_size.height()),
                        total_tile_size);

  tile_bounds.Offset(GetGridCenteringOffset(index.page));
  tile_bounds.Inset(-GetTilePadding(index.page));
  return tile_bounds;
}

bool AppsGridView::IsViewHiddenForDrag(const views::View* view) const {
  return drag_view_hider_ && drag_view_hider_->drag_view() == view;
}

bool AppsGridView::IsViewHiddenForFolderReorder(const views::View* view) const {
  return reordering_folder_view_ && *reordering_folder_view_ == view;
}

bool AppsGridView::IsUnderWholeGridAnimation() const {
  return grid_animation_status_ != AppListGridAnimationStatus::kEmpty;
}

bool AppsGridView::IsViewExplicitlyHidden(const views::View* view) const {
  return IsViewHiddenForDrag(view) || IsViewHiddenForFolderReorder(view) ||
         hidden_view_for_test_ == view;
}

void AppsGridView::MaybeAbortWholeGridAnimation() {
  switch (grid_animation_status_) {
    case AppListGridAnimationStatus::kEmpty:
    case AppListGridAnimationStatus::kReorderIntermediaryState:
      // No active whole-grid animation so nothing to do.
      break;
    case AppListGridAnimationStatus::kReorderFadeOut:
    case AppListGridAnimationStatus::kReorderFadeIn:
    case AppListGridAnimationStatus::kHideContinueSection:
      DCHECK(grid_animation_abort_handle_);
      grid_animation_abort_handle_.reset();
      break;
  }
}

AppListItemView* AppsGridView::GetViewDisplayedAtSlotOnCurrentPage(
    int slot) const {
  if (slot < 0) {
    return nullptr;
  }

  // Calculate the original bound of the tile at |index|.
  gfx::Rect tile_rect =
      GetExpectedTileBounds(GridIndex(GetSelectedPage(), slot));
  tile_rect.Offset(CalculateTransitionOffset(GetSelectedPage()));

  const auto& entries = view_model_.entries();
  const auto iter = base::ranges::find_if(entries, [&](const auto& entry) {
    return entry.view->bounds() == tile_rect && entry.view.get() != drag_view_;
  });
  return iter == entries.end() ? nullptr
                               : static_cast<AppListItemView*>(iter->view);
}

void AppsGridView::SetAsFolderDroppingTarget(const GridIndex& target_index,
                                             bool is_target_folder) {
  AppListItemView* target_view =
      GetViewDisplayedAtSlotOnCurrentPage(target_index.slot);
  if (target_view) {
    target_view->SetAsAttemptedFolderTarget(is_target_folder);
    if (is_target_folder)
      target_view->OnDraggedViewEnter();
    else
      target_view->OnDraggedViewExit();
  }
}

GridIndex AppsGridView::GetTargetGridIndexForKeyboardMove(
    ui::KeyboardCode key_code) const {
  DCHECK(key_code == ui::VKEY_LEFT || key_code == ui::VKEY_RIGHT ||
         key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN);
  DCHECK(selected_view_);

  const GridIndex source_index = GetIndexOfView(selected_view_);
  if (key_code == ui::VKEY_LEFT || key_code == ui::VKEY_RIGHT) {
    // Define backward key for traversal based on RTL.
    const ui::KeyboardCode backward =
        base::i18n::IsRTL() ? ui::VKEY_RIGHT : ui::VKEY_LEFT;

    size_t target_model_index =
        view_model_.GetIndexOfView(selected_view_).value();
    if (target_model_index > 0 || key_code != backward)
      target_model_index += (key_code == backward) ? -1 : 1;

    // A forward move on the last item in |view_model_| should do nothing.
    if (target_model_index == view_model_.view_size()) {
      return source_index;
    }
    return GetIndexOfView(
        static_cast<const AppListItemView*>(GetItemViewAt(target_model_index)));
  }

  // Handle the vertical move. Attempt to place the app in the same column.
  int target_page = source_index.page;
  int target_row =
      source_index.slot / cols_ + (key_code == ui::VKEY_UP ? -1 : 1);

  if (target_row < 0) {
    // The app will move to the last row of the previous page.
    --target_page;
    if (target_page < 0) {
      return source_index;
    }

    // When moving up, place the app in the last row.
    target_row = (GetNumberOfItemsOnPage(target_page) - 1) / cols_;
  } else if (target_row > (GetNumberOfItemsOnPage(target_page) - 1) / cols_) {
    // The app will move to the first row of the next page.
    ++target_page;
    if (target_page >= GetTotalPages()) {
      return source_index;
    }
    target_row = 0;
  }

  // The ideal slot shares a column with |source_index|.
  const int ideal_slot = target_row * cols_ + source_index.slot % cols_;
  return GridIndex(
      target_page,
      std::min(GetNumberOfItemsOnPage(target_page) - 1, ideal_slot));
}

GridIndex AppsGridView::GetTargetGridIndexForKeyboardReparent(
    const GridIndex& folder_index,
    ui::KeyboardCode key_code) const {
  DCHECK(!folder_delegate_) << "Reparenting target calculations occur from the "
                               "root AppsGridView, not the folder AppsGridView";

  // A backward move means the item will be placed previous to the folder. To do
  // this without displacing other items, place the item in the folders slot.
  // The folder will then shift forward.
  const ui::KeyboardCode backward =
      base::i18n::IsRTL() ? ui::VKEY_RIGHT : ui::VKEY_LEFT;
  if (key_code == backward) {
    return folder_index;
  }

  GridIndex target_index = GetTargetGridIndexForKeyboardMove(key_code);

  // If the item is expected to be positioned after the parent view,
  // `GetTargetGridIndexForKeyboardMove()` may return folder index to indicate
  // no-op operation for move (e.g. if the folder is the last item), assuming
  // that there are no slots available. Reparent is an insertion operation, so
  // creating an extra trailing slot is allowed.
  if (target_index == folder_index &&
      (key_code != ui::VKEY_UP && key_code != backward)) {
    if (IsPageFull(target_index.page))
      return GridIndex(target_index.page + 1, 0);
    return GridIndex(target_index.page, target_index.slot + 1);
  }

  // Ensure the item is placed on the same page as the folder when possible.
  if (target_index.page < folder_index.page) {
    return folder_index;
  }

  if (target_index.page > folder_index.page) {
    const std::optional<int> folder_page_size = TilesPerPage(folder_index.page);
    // Target index page being at least 1 indicates paged apps grid, so number
    // of tiles per page should be bounded.
    DCHECK(folder_page_size);
    if (folder_index.slot + 1 < *folder_page_size)
      return GridIndex(folder_index.page, folder_index.slot + 1);
  }

  return target_index;
}

void AppsGridView::HandleKeyboardMove(ui::KeyboardCode key_code) {
  DCHECK(selected_view_);
  const GridIndex target_index = GetTargetGridIndexForKeyboardMove(key_code);
  const GridIndex starting_index = GetIndexOfView(selected_view_);
  if (target_index == starting_index || !IsValidIndex(target_index)) {
    return;
  }

  handling_keyboard_move_ = true;

  AppListItemView* original_selected_view = selected_view_;
  const GridIndex original_selected_view_index =
      GetIndexOfView(original_selected_view);
  // Moving an AppListItemView is either a swap within the origin page, a swap
  // to a full page, or a dump to a page with room. A move within a folder is
  // always a swap because there are no gaps.
  const bool swap_items =
      folder_delegate_ || IsPageFull(target_index.page) ||
      target_index.page == original_selected_view_index.page;

  AppListItemView* target_view = GetViewAtIndex(target_index);
  MoveItemInModel(selected_view_->item(), target_index);
  if (swap_items) {
    DCHECK(target_view);
    MoveItemInModel(target_view->item(), original_selected_view_index);
  }

  int target_page = target_index.page;
  if (!folder_delegate_) {
    // Update paging because the move could have resulted in a
    // page getting collapsed or created.
    UpdatePaging();

    // |target_page| may change due to a page collapsing.
    target_page = std::min(GetTotalPages() - 1, target_index.page);
  }
  DeprecatedLayoutImmediately();
  EnsureViewVisible(GridIndex(target_page, target_index.slot));
  SetSelectedView(original_selected_view);
  AnnounceReorder(target_index);

  if (target_index.page != original_selected_view_index.page &&
      !folder_delegate_) {
    RecordPageSwitcherSource(kMoveAppWithKeyboard);
  }
}

bool AppsGridView::IsValidIndex(const GridIndex& index) const {
  const std::optional<int> tiles_per_page = TilesPerPage(index.page);
  const int extra_valid_slots = HasExtraSlotForReorderPlaceholder() ? 1 : 0;
  return index.page >= 0 && index.page < GetTotalPages() && index.slot >= 0 &&
         (!tiles_per_page || index.slot < *tiles_per_page) &&
         static_cast<size_t>(GetIndexInViewModel(index)) <
             view_model_.view_size() + extra_valid_slots;
}

size_t AppsGridView::GetModelIndexOfItem(const AppListItem* item) const {
  const auto& entries = view_model_.entries();
  const auto iter = base::ranges::find(entries, item, [](const auto& entry) {
    return static_cast<AppListItemView*>(entry.view)->item();
  });
  return static_cast<size_t>(std::distance(entries.begin(), iter));
}

int AppsGridView::GetNumberOfItemsOnPage(int page) const {
  if (page < 0 || page >= GetTotalPages()) {
    return 0;
  }

  // We are guaranteed not on the last page, so the page must be full.
  if (page < GetTotalPages() - 1) {
    return *TilesPerPage(page);
  }

  // We are on the last page, so calculate the number of items on the page.
  size_t item_count = view_model_.view_size();
  int current_page = 0;
  while (current_page < GetTotalPages() - 1) {
    std::optional<int> tiles_per_page = TilesPerPage(current_page);
    // `current_page` not being the last page implies a paged apps grid view,
    // as the grid has more than one page. For paged apps grid view,
    // `TilesPerPage()` should be defined.
    DCHECK(tiles_per_page);
    item_count -= *tiles_per_page;
    ++current_page;
  }
  return item_count;
}

void AppsGridView::MaybeCreateFolderDroppingAccessibilityEvent() {
  if (!drag_item_ || !drag_view_) {
    return;
  }
  if (drop_target_region_ != ON_ITEM || !DropTargetIsValidFolder() ||
      IsFolderItem(drag_item_) || folder_delegate_ ||
      drop_target_ == last_folder_dropping_a11y_event_location_) {
    return;
  }

  last_folder_dropping_a11y_event_location_ = drop_target_;
  last_reorder_a11y_event_location_ = GridIndex();

  AppListItemView* drop_view =
      GetViewDisplayedAtSlotOnCurrentPage(drop_target_.slot);
  DCHECK(drop_view);

  a11y_announcer_->AnnounceFolderDrop(drag_view_->title()->GetText(),
                                      drop_view->title()->GetText(),
                                      drop_view->is_folder());
}

void AppsGridView::MaybeCreateDragReorderAccessibilityEvent() {
  if (drop_target_region_ == ON_ITEM && !IsFolderItem(drag_item_)) {
    return;
  }

  // If app was dragged out of folder, no need to announce location for the
  // now closed folder.
  if (drag_out_of_folder_container_) {
    return;
  }

  // If drop_target is not set or was already reset, then return.
  if (drop_target_ == GridIndex()) {
    return;
  }

  // Don't create a11y event if |drop_target| has not changed.
  if (last_reorder_a11y_event_location_ == drop_target_) {
    return;
  }

  last_folder_dropping_a11y_event_location_ = GridIndex();
  last_reorder_a11y_event_location_ = drop_target_;

  AnnounceReorder(last_reorder_a11y_event_location_);
}

void AppsGridView::AnnounceReorder(const GridIndex& target_index) {
  const int row =
      ((target_index.slot - (target_index.slot % cols_)) / cols_) + 1;
  const int col = (target_index.slot % cols_) + 1;
  if (!GetMaxRowsInPage(0)) {
    // Don't announce the page for single-page grids (e.g. scrollable grids).
    a11y_announcer_->AnnounceAppsGridReorder(row, col);
  } else {
    // Announce the page for paged grids.
    const int page = target_index.page + 1;
    a11y_announcer_->AnnounceAppsGridReorder(page, row, col);
  }
}

void AppsGridView::CreateGhostImageView() {
  if (!drag_item_) {
    return;
  }

  // OnReorderTimer() can trigger this function even when the
  // |reorder_placeholder_| does not change, no need to set a new GhostImageView
  // in this case.
  if (reorder_placeholder_ == current_ghost_location_) {
    return;
  }

  // When the item is dragged outside the boundaries of the app grid, if the
  // |reorder_placeholder_| moves to another page, then do not show a ghost.
  if (GetSelectedPage() != reorder_placeholder_.page) {
    BeginHideCurrentGhostImageView();
    return;
  }

  BeginHideCurrentGhostImageView();
  current_ghost_location_ = reorder_placeholder_;

  if (last_ghost_view_) {
    delete last_ghost_view_;
  }

  // Preserve |current_ghost_view_| while it fades out and instantiate a new
  // GhostImageView that will fade in.
  last_ghost_view_ = current_ghost_view_;

  auto current_ghost_view =
      std::make_unique<GhostImageView>(reorder_placeholder_);
  gfx::Rect ghost_view_bounds = GetExpectedTileBounds(reorder_placeholder_);
  ghost_view_bounds.Offset(
      CalculateTransitionOffset(reorder_placeholder_.page));
  current_ghost_view->Init(ghost_view_bounds,
                           app_list_config_->grid_focus_corner_radius());
  current_ghost_view_ =
      items_container_->AddChildView(std::move(current_ghost_view));
  current_ghost_view_->FadeIn();

  // Adding the ghost view can reorder the child layers of the
  // |items_container_| so make sure the background cards remain at the bottom.
  StackCardsAtBottom();
}

void AppsGridView::BeginHideCurrentGhostImageView() {
  current_ghost_location_ = GridIndex();

  if (current_ghost_view_) {
    current_ghost_view_->FadeOut();
  }
}

void AppsGridView::PrepareItemsForBoundsAnimation() {
  for (size_t i = 0; i < view_model_.view_size(); ++i)
    view_model_.view_at(i)->EnsureLayer();
}

bool AppsGridView::HasExtraSlotForReorderPlaceholder() const {
  return reorder_placeholder_.IsValid() && !drag_view_;
}

void AppsGridView::OnAppListItemViewActivated(
    AppListItemView* pressed_item_view,
    const ui::Event& event) {
  if (IsDragging()) {
    return;
  }

  if (IsFolderItem(pressed_item_view->item())) {
    // Note that `folder_controller_` will be null inside a folder apps grid,
    // but those grid are not expected to contain folder items.
    DCHECK(folder_controller_);
    SetOpenFolderInfo(pressed_item_view->item()->id(),
                      GetIndexOfView(pressed_item_view), GridIndex());
    ShowFolderForView(pressed_item_view, /*new_folder=*/false);
    return;
  }

  base::RecordAction(base::UserMetricsAction("AppList_ClickOnApp"));

  RecordAppListByCollectionLaunched(pressed_item_view->item()->collection_id(),
                                    /*is_apps_collections_page=*/false);

  // Avoid using |item->id()| as the parameter. In some rare situations,
  // activating the item may destruct it. Using the reference to an object
  // which may be destroyed during the procedure as the function parameter
  // may bring the crash like https://crbug.com/990282.
  const std::string id = pressed_item_view->item()->id();
  const bool is_above_the_fold = IsAboveTheFold(pressed_item_view);
  app_list_view_delegate()->ActivateItem(id, event.flags(),
                                         AppListLaunchedFrom::kLaunchedFromGrid,
                                         is_above_the_fold);
}

bool AppsGridView::IsAboveTheFold(AppListItemView* item_view) {
  return false;
}

void AppsGridView::OnFadeOutAnimationEnded(ReorderAnimationCallback callback,
                                           bool aborted) {
  grid_animation_status_ =
      AppListGridAnimationStatus::kReorderIntermediaryState;

  // Reset with the identical transformation. Because the apps grid view is
  // translucent now, setting the layer transform does not bring noticeable
  // differences.
  layer()->SetTransform(gfx::Transform());

  if (aborted) {
    // If the fade out animation is aborted, show the apps grid because the fade
    // in animation should not be called when the fade out animation is aborted.
    layer()->SetOpacity(1.f);
  } else {
    // Hide all item views before the fade in animation in order to reduce the
    // painting cost incurred by the bounds changes because of reorder. The
    // fade in animation should be responsible for reshowing the item views that
    // are within the visible view port after reorder.
    for (size_t view_index = 0; view_index < view_model_.view_size();
         ++view_index) {
      view_model_.view_at(view_index)->SetVisible(false);
    }
  }

  // Before starting the fade in animation, the reordered items should be at
  // their final positions instantly.
  base::AutoReset auto_reset(&enable_item_move_animation_, false);

  callback.Run(aborted);

  if (fade_out_done_closure_for_test_)
    std::move(fade_out_done_closure_for_test_).Run();

  // When the fade out animation is abortted, the fade in animation should not
  // run. Hence, the reorder animation ends. The aborted animation's smoothness
  // is not reported.
  if (aborted) {
    grid_animation_status_ = AppListGridAnimationStatus::kEmpty;
    MaybeRunNextReorderAnimationCallbackForTest(
        /*aborted=*/true, AppListGridAnimationStatus::kReorderFadeOut);

    // Reset `reorder_animation_tracker_` without calling Stop() because the
    // aborted animation's smoothness is not reported.
    reorder_animation_tracker_.reset();
  }
}

void AppsGridView::OnFadeInAnimationEnded(ReorderAnimationCallback callback,
                                          bool aborted) {
  // If the animation is aborted, reset the apps grid's layer.
  if (aborted) {
    layer()->SetOpacity(1.f);
  }

  // Ensure that all item views are visible after fade in animation completes.
  for (size_t view_index = 0; view_index < view_model_.view_size();
       ++view_index) {
    view_model_.view_at(view_index)->SetVisible(true);
  }

  grid_animation_status_ = AppListGridAnimationStatus::kEmpty;

  // Do not report the smoothness data for the aborted animation.
  if (!aborted) {
    reorder_animation_tracker_->Stop();
  }
  reorder_animation_tracker_.reset();

  // Clean app list items' layers.
  DestroyLayerItemsIfNotNeeded();

  if (!callback.is_null()) {
    callback.Run(aborted);
  }

  MaybeRunNextReorderAnimationCallbackForTest(
      aborted, AppListGridAnimationStatus::kReorderFadeIn);
}

void AppsGridView::MaybeRunNextReorderAnimationCallbackForTest(
    bool aborted,
    AppListGridAnimationStatus animation_source) {
  if (reorder_animation_callback_queue_for_test_.empty()) {
    return;
  }

  TestReorderDoneCallbackType front_callback =
      std::move(reorder_animation_callback_queue_for_test_.front());
  reorder_animation_callback_queue_for_test_.pop();
  std::move(front_callback).Run(aborted, animation_source);
}

void AppsGridView::OnIdealBoundsAnimationDone() {
  if (item_reorder_animation_tracker_) {
    item_reorder_animation_tracker_->Stop();
    item_reorder_animation_tracker_.reset();
  }
  DestroyLayerItemsIfNotNeeded();
}

BEGIN_METADATA(AppsGridView)
END_METADATA

}  // namespace ash
