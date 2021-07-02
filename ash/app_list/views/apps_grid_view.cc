// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/apps_grid_view.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/views/app_list_a11y_announcer.h"
#include "ash/app_list/views/app_list_drag_and_drop_host.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/ghost_image_view.h"
#include "ash/app_list/views/pulsing_block_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_tile_item_view.h"
#include "ash/app_list/views/top_icon_animation_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_config_provider.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/metrics_util.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// Distance a drag needs to be from the app grid to be considered 'outside', at
// which point we rearrange the apps to their pre-drag configuration, as a drop
// then would be canceled. We have a buffer to make it easier to drag apps to
// other pages.
constexpr int kDragBufferPx = 20;

// Time delay before shelf starts to handle icon drag operation,
// such as shelf icons re-layout.
constexpr base::TimeDelta kShelfHandleIconDragDelay =
    base::TimeDelta::FromMilliseconds(500);

// The drag and drop proxy should get scaled by this factor.
constexpr float kDragAndDropProxyScale = 1.2f;

// Delays in milliseconds to show re-order preview.
constexpr int kReorderDelay = 120;

// Delays in milliseconds to show folder item reparent UI.
constexpr int kFolderItemReparentDelay = 50;

// Maximum vertical and horizontal spacing between tiles.
constexpr int kMaximumTileSpacing = 96;

// RowMoveAnimationDelegate is used when moving an item into a different row.
// Before running the animation, the item's layer is re-created and kept in
// the original position, then the item is moved to just before its target
// position and opacity set to 0. When the animation runs, this delegate moves
// the layer and fades it out while fading in the item at the same time.
class RowMoveAnimationDelegate : public views::AnimationDelegateViews {
 public:
  RowMoveAnimationDelegate(views::View* view,
                           ui::Layer* layer,
                           const gfx::Rect& layer_target)
      : views::AnimationDelegateViews(view),
        view_(view),
        layer_(layer),
        layer_start_(layer ? layer->bounds() : gfx::Rect()),
        layer_target_(layer_target) {}
  ~RowMoveAnimationDelegate() override = default;

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override {
    view_->layer()->SetOpacity(animation->GetCurrentValue());
    view_->layer()->ScheduleDraw();

    if (layer_) {
      layer_->SetOpacity(1 - animation->GetCurrentValue());
      layer_->SetBounds(
          animation->CurrentValueBetween(layer_start_, layer_target_));
      layer_->ScheduleDraw();
    }
  }
  void AnimationEnded(const gfx::Animation* animation) override {
    if (layer_)
      view_->layer()->SetOpacity(1.0f);
  }
  void AnimationCanceled(const gfx::Animation* animation) override {
    if (layer_)
      view_->layer()->SetOpacity(1.0f);
  }

 private:
  // The view that needs to be wrapped. Owned by views hierarchy.
  views::View* view_;

  std::unique_ptr<ui::Layer> layer_;
  const gfx::Rect layer_start_;
  const gfx::Rect layer_target_;

  DISALLOW_COPY_AND_ASSIGN(RowMoveAnimationDelegate);
};

// ItemRemoveAnimationDelegate is used to show animation for removing an item.
// This happens when user drags an item into a folder. The dragged item will
// be removed from the original list after it is dropped into the folder.
class ItemRemoveAnimationDelegate : public views::AnimationDelegateViews {
 public:
  explicit ItemRemoveAnimationDelegate(views::View* view)
      : views::AnimationDelegateViews(view), view_(view) {}

  ~ItemRemoveAnimationDelegate() override = default;

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override {
    view_->layer()->SetOpacity(1 - animation->GetCurrentValue());
    view_->layer()->ScheduleDraw();
  }

 private:
  std::unique_ptr<views::View> view_;

  DISALLOW_COPY_AND_ASSIGN(ItemRemoveAnimationDelegate);
};

// ItemMoveAnimationDelegate observes when an item finishes animating when it is
// not moving between rows. This is to ensure an item is repainted for the
// "zoom out" case when releasing an item being dragged.
class ItemMoveAnimationDelegate : public views::AnimationDelegateViews {
 public:
  explicit ItemMoveAnimationDelegate(AppListItemView* view,
                                     bool is_released_drag_view)
      : views::AnimationDelegateViews(view),
        view_(view),
        is_released_drag_view_(is_released_drag_view) {
    if (is_released_drag_view_)
      view_->title()->SetVisible(false);
  }

  // views::AnimationDelegateViews:
  void AnimationEnded(const gfx::Animation* animation) override {
    if (is_released_drag_view_)
      view_->title()->SetVisible(true);
    view_->SchedulePaint();
  }
  void AnimationCanceled(const gfx::Animation* animation) override {
    if (is_released_drag_view_)
      view_->title()->SetVisible(true);
    view_->SchedulePaint();
  }

 private:
  AppListItemView* view_;
  bool is_released_drag_view_;

  DISALLOW_COPY_AND_ASSIGN(ItemMoveAnimationDelegate);
};

// This class observes the end of folder dropping animation.
class FolderDroppingAnimationObserver : public TopIconAnimationObserver {
 public:
  FolderDroppingAnimationObserver(AppListModel* model,
                                  const std::string& folder_item_id)
      : model_(model), folder_item_id_(folder_item_id) {}

  // TopIconAnimationObserver:
  void OnTopIconAnimationsComplete(TopIconAnimationView* view) override {
    AppListFolderItem* item =
        static_cast<AppListFolderItem*>(model_->FindItem(folder_item_id_));

    // The folder item may be deleted during the animation.
    if (!item)
      return;

    // Update the folder icon.
    item->NotifyOfDraggedItem(nullptr);

    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

 private:
  AppListModel* model_;  // Not owned.
  const std::string folder_item_id_;

  DISALLOW_COPY_AND_ASSIGN(FolderDroppingAnimationObserver);
};

bool IsOEMFolderItem(AppListItem* item) {
  return IsFolderItem(item) &&
         (static_cast<AppListFolderItem*>(item))->folder_type() ==
             AppListFolderItem::FOLDER_TYPE_OEM;
}

// Returns the relative horizontal position of a point compared to a rect. -1
// means the point is outside on the left side of the rect. 0 means the point is
// within the rect. 1 means it's on the right side of the rect.
int CompareHorizontalPointPositionToRect(gfx::Point point, gfx::Rect bounds) {
  if (point.x() > bounds.right())
    return 1;
  if (point.x() < bounds.x())
    return -1;
  return 0;
}

}  // namespace

std::string GridIndex::ToString() const {
  std::stringstream ss;
  ss << "Page: " << page << ", Slot: " << slot;
  return ss.str();
}

// static
constexpr float AppsGridView::kCardifiedScale;

// static
constexpr int AppsGridView::kDefaultAnimationDuration;

AppsGridView::AppsGridView(ContentsView* contents_view,
                           AppListA11yAnnouncer* a11y_announcer,
                           AppListViewDelegate* app_list_view_delegate,
                           AppsGridViewFolderDelegate* folder_delegate)
    : folder_delegate_(folder_delegate),
      contents_view_(contents_view),
      a11y_announcer_(a11y_announcer),
      app_list_view_delegate_(app_list_view_delegate) {
  DCHECK(a11y_announcer_);
  DCHECK(app_list_view_delegate_);
}

void AppsGridView::Init() {
  SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  // Clip any icons that are outside the grid view's bounds. These icons would
  // otherwise be visible to the user when the grid view is off screen.
  layer()->SetMasksToBounds(true);

  items_container_ = AddChildView(std::make_unique<views::View>());
  items_container_->SetPaintToLayer();
  items_container_->layer()->SetFillsBoundsOpaquely(false);
  bounds_animator_ = std::make_unique<views::BoundsAnimator>(
      items_container_, /*use_transforms=*/true);

  if (!folder_delegate_) {
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets(GetAppListConfig().grid_fadeout_mask_height(), 0)));
  }

  pagination_model_.SetTransitionDurations(
      GetAppListConfig().page_transition_duration(),
      GetAppListConfig().overscroll_page_transition_duration());

  bounds_animator_->AddObserver(this);
}

AppsGridView::~AppsGridView() {
  bounds_animator_->RemoveObserver(this);
  // Coming here |drag_view_| should already be canceled since otherwise the
  // drag would disappear after the app list got animated away and closed,
  // which would look odd.
  DCHECK(!drag_view_);
  if (drag_view_)
    EndDrag(true);

  if (model_)
    model_->RemoveObserver(this);

  if (item_list_)
    item_list_->RemoveObserver(this);

  // Cancel animations now, otherwise RemoveAllChildViews() may call back to
  // ViewHierarchyChanged() during removal, which can lead to double deletes
  // (because ViewHierarchyChanged() may attempt to delete a view that is part
  // way through deletion). Note that cancelling animations may cause
  // AppListItemView to Layout(), which may call back into this object.
  bounds_animator_->Cancel();

  view_model_.Clear();
  RemoveAllChildViews(true);
}

void AppsGridView::SetLayout(int cols, int rows_per_page) {
  cols_ = cols;
  rows_per_page_ = rows_per_page;
}

gfx::Size AppsGridView::GetTotalTileSize() const {
  gfx::Rect rect(GetTileViewSize());
  rect.Inset(GetTilePadding());
  return rect.size();
}

gfx::Size AppsGridView::GetTileGridSizeWithPadding() const {
  gfx::Size size(GetTileViewSize());
  size.SetSize(size.width() * cols_, size.height() * rows_per_page_);

  int horizontal_padding = horizontal_tile_padding_ * 2;
  int vertical_padding = vertical_tile_padding_ * 2;

  if (folder_delegate_) {
    const int tile_padding_in_folder =
        GetAppListConfig().grid_tile_spacing_in_folder();
    horizontal_padding = tile_padding_in_folder;
    vertical_padding = tile_padding_in_folder;
  }

  size.Enlarge(horizontal_padding * (cols_ - 1),
               vertical_padding * (rows_per_page_ - 1));
  return size;
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
  return gfx::Size(tile_size.width() * cols + kMaximumTileSpacing * (cols - 1),
                   tile_size.height() * rows_per_page +
                       kMaximumTileSpacing * (rows_per_page - 1));
}

void AppsGridView::ResetForShowApps() {
  ClearDragState();
  layer()->SetOpacity(1.0f);
  SetVisible(true);

  // The number of non-page-break-items should be the same as item views.
  int item_count = 0;
  for (size_t i = 0; i < item_list_->item_count(); ++i) {
    if (!item_list_->item_at(i)->is_page_break())
      ++item_count;
  }
  CHECK_EQ(item_count, view_model_.view_size());
}

void AppsGridView::DisableFocusForShowingActiveFolder(bool disabled) {
  for (const auto& entry : view_model_.entries())
    entry.view->SetEnabled(!disabled);

  // Ignore the grid view in accessibility tree so that items inside it will not
  // be accessed by ChromeVox.
  GetViewAccessibility().OverrideIsIgnored(disabled);
  GetViewAccessibility().NotifyAccessibilityEvent(
      ax::mojom::Event::kTreeChanged);
}

void AppsGridView::SetModel(AppListModel* model) {
  if (model_)
    model_->RemoveObserver(this);

  model_ = model;
  if (model_)
    model_->AddObserver(this);

  Update();
}

void AppsGridView::SetItemList(AppListItemList* item_list) {
  if (item_list_)
    item_list_->RemoveObserver(this);
  item_list_ = item_list;
  if (item_list_)
    item_list_->AddObserver(this);
  Update();
}

bool AppsGridView::IsInFolder() const {
  return !!folder_delegate_;
}

void AppsGridView::SetSelectedView(AppListItemView* view) {
  if (IsSelectedView(view) || IsDraggedView(view))
    return;

  GridIndex index = GetIndexOfView(view);
  if (IsValidIndex(index))
    SetSelectedItemByIndex(index);
}

void AppsGridView::ClearSelectedView() {
  selected_view_ = nullptr;
}

bool AppsGridView::IsSelectedView(const AppListItemView* view) const {
  return selected_view_ == view;
}

void AppsGridView::InitiateDrag(AppListItemView* view,
                                const gfx::Point& location,
                                const gfx::Point& root_location) {
  DCHECK(view);
  if (drag_view_ || pulsing_blocks_model_.view_size())
    return;

  items_need_layer_for_drag_ = true;
  for (const auto& entry : view_model_.entries())
    static_cast<AppListItemView*>(entry.view)->EnsureLayer();
  drag_view_ = view;

  // Dragged view should have focus. This also fixed the issue
  // https://crbug.com/834682.
  drag_view_->RequestFocus();
  drag_view_init_index_ = GetIndexOfView(drag_view_);
  reorder_placeholder_ = drag_view_init_index_;
  ExtractDragLocation(root_location, &drag_start_grid_view_);
  drag_view_start_ = gfx::Point(drag_view_->x(), drag_view_->y());
}

void AppsGridView::StartDragAndDropHostDragAfterLongPress() {
  TryStartDragAndDropHostDrag(TOUCH, drag_start_grid_view_);
}

void AppsGridView::TryStartDragAndDropHostDrag(
    Pointer pointer,
    const gfx::Point& grid_location) {
  // Stopping the animation may have invalidated our drag view due to the
  // view hierarchy changing.
  if (!drag_view_)
    return;

  drag_pointer_ = pointer;
  // Move the view to the front so that it appears on top of other views.
  items_container_->ReorderChildView(drag_view_, -1);
  bounds_animator_->StopAnimatingView(drag_view_);

  if (!dragging_for_reparent_item_)
    StartDragAndDropHostDrag(grid_location);
}

bool AppsGridView::UpdateDragFromItem(bool is_touch,
                                      const ui::LocatedEvent& event) {
  if (!drag_view_)
    return false;  // Drag canceled.

  MaybeStartCardifiedView();

  gfx::Point drag_point_in_grid_view;
  ExtractDragLocation(event.root_location(), &drag_point_in_grid_view);
  const Pointer pointer = is_touch ? TOUCH : MOUSE;
  UpdateDrag(pointer, drag_point_in_grid_view);
  if (!IsDragging())
    return false;

  // If a drag and drop host is provided, see if the drag operation needs to be
  // forwarded.
  gfx::Point drag_point_in_screen = event.root_location();
  ::wm::ConvertPointToScreen(GetWidget()->GetNativeWindow()->GetRootWindow(),
                             &drag_point_in_screen);
  DispatchDragEventToDragAndDropHost(drag_point_in_screen);
  if (drag_and_drop_host_) {
    drag_and_drop_host_->UpdateDragIconProxyByLocation(
        drag_view_->GetIconBoundsInScreen().origin());
  }
  return true;
}

void AppsGridView::UpdateDrag(Pointer pointer, const gfx::Point& point) {
  if (folder_delegate_)
    UpdateDragStateInsideFolder(pointer, point);

  if (!drag_view_)
    return;  // Drag canceled.

  const gfx::Vector2d drag_vector(point - drag_start_grid_view_);
  if (!IsDragging() && ExceededDragThreshold(drag_vector))
    TryStartDragAndDropHostDrag(pointer, point);

  if (drag_pointer_ != pointer)
    return;

  drag_view_->SetPosition(drag_view_start_ + drag_vector);

  last_drag_point_ = point;
  const GridIndex last_drop_target = drop_target_;
  DropTargetRegion last_drop_target_region = drop_target_region_;
  UpdateDropTargetRegion();

  MaybeStartPageFlip();

  if (last_drop_target != drop_target_ ||
      last_drop_target_region != drop_target_region_) {
    if (drop_target_region_ == ON_ITEM && DraggedItemCanEnterFolder() &&
        DropTargetIsValidFolder()) {
      reorder_timer_.Stop();
      folder_dropping_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(
              GetAppListConfig().folder_dropping_delay()),
          this, &AppsGridView::OnFolderDroppingTimer);
    } else if ((drop_target_region_ == ON_ITEM ||
                drop_target_region_ == NEAR_ITEM) &&
               !folder_delegate_) {
      folder_dropping_timer_.Stop();

      // If the drag changes regions from |BETWEEN_ITEMS| to |NEAR_ITEM| the
      // timer should reset, so that we gain the extra time from hovering near
      // the item
      if (last_drop_target_region == BETWEEN_ITEMS)
        reorder_timer_.Stop();
      reorder_timer_.Start(FROM_HERE,
                           base::TimeDelta::FromMilliseconds(kReorderDelay * 5),
                           this, &AppsGridView::OnReorderTimer);
    } else if (drop_target_region_ != NO_TARGET) {
      // If none of the above cases evaluated true, then all of the possible
      // drop regions should result in a fast reorder.
      folder_dropping_timer_.Stop();
      reorder_timer_.Start(FROM_HERE,
                           base::TimeDelta::FromMilliseconds(kReorderDelay),
                           this, &AppsGridView::OnReorderTimer);
    }

    // Reset the previous drop target.
    if (last_drop_target_region == ON_ITEM)
      SetAsFolderDroppingTarget(last_drop_target, false);
  }
}

void AppsGridView::EndDrag(bool cancel) {
  // EndDrag was called before if |drag_view_| is nullptr.
  if (!drag_view_)
    return;

  // Coming here a drag and drop was in progress.
  const bool landed_in_drag_and_drop_host =
      forward_events_to_drag_and_drop_host_;

  // The drag ended by reparenting in a folder.
  bool reparented_into_folder = false;

  // This is the folder view to drop an item into. Cache the |drag_view_|'s item
  // and its bounds for later use in folder dropping animation.
  AppListItemView* folder_item_view = nullptr;
  AppListItem* drag_item = drag_view_->item();
  gfx::Rect drag_source_bounds(drag_view_->bounds());

  if (forward_events_to_drag_and_drop_host_) {
    DCHECK(!IsDraggingForReparentInRootLevelGridView());
    forward_events_to_drag_and_drop_host_ = false;
    drag_and_drop_host_->EndDrag(cancel);
    if (IsDraggingForReparentInHiddenGridView()) {
      folder_delegate_->DispatchEndDragEventForReparent(
          true /* events_forwarded_to_drag_drop_host */,
          cancel /* cancel_drag */);
    } else {
      // |drag_view_| is reordered when initiating the drag. In addition, the
      // icon's location in AppsGridView does not alter after being dragged to
      // Shelf. So recover the order when drag ends.
      MoveItemInModel(drag_view_, drag_view_init_index_);
    }
  } else {
    if (IsDraggingForReparentInHiddenGridView()) {
      // Forward the EndDrag event to the root level grid view.
      folder_delegate_->DispatchEndDragEventForReparent(
          false /* events_forwarded_to_drag_drop_host */,
          cancel /* cancel_drag */);
      EndDragForReparentInHiddenFolderGridView();
      return;
    }

    if (IsDraggingForReparentInRootLevelGridView()) {
      // An EndDrag can be received during a reparent via a model change. This
      // is always a cancel and needs to be forwarded to the folder.
      DCHECK(cancel);
      contents_view_->GetAppListMainView()->CancelDragInActiveFolder();
      return;
    }

    if (!cancel && IsDragging()) {
      // Regular drag ending path, ie, not for reparenting.
      UpdateDropTargetRegion();
      if (drop_target_region_ == ON_ITEM && DraggedItemCanEnterFolder() &&
          DropTargetIsValidFolder()) {
        MaybeCreateFolderDroppingAccessibilityEvent();
        folder_item_view = MoveItemToFolder(drag_view_, drop_target_);
        // If the view that the folder is replacing had a layer, ensure the new
        // folder view has one too.
        if (drag_view_ && drag_view_->layer())
          folder_item_view->EnsureLayer();
        reparented_into_folder = true;
      } else if (IsValidReorderTargetIndex(drop_target_)) {
        // Ensure reorder event has already been announced by the end of drag.
        MaybeCreateDragReorderAccessibilityEvent();
        MoveItemInModel(drag_view_, drop_target_);
        RecordAppMovingTypeMetrics(folder_delegate_ ? kReorderByDragInFolder
                                                    : kReorderByDragInTopLevel);
      }
    }
  }

  if (drag_and_drop_host_) {
    // If we had a drag and drop proxy icon, we delete it and make the real
    // item visible again.
    drag_and_drop_host_->DestroyDragIconProxy();
    // Issue 439055: MoveItemToFolder() can sometimes delete |drag_view_|
    if (drag_view_) {
      if (landed_in_drag_and_drop_host) {
        // Move the item directly to the target location, avoiding the
        // "zip back" animation if the user was pinning it to the shelf.
        int i = drop_target_.slot;
        gfx::Rect bounds = view_model_.ideal_bounds(i);
        drag_view_->SetBoundsRect(bounds);
      }
      // Fade in slowly if it landed in the shelf.
      SetViewHidden(drag_view_, false /* show */,
                    !landed_in_drag_and_drop_host /* animate */);
    }
  }

  SetAsFolderDroppingTarget(drop_target_, false);

  // Keep track of the |drag_view| after it is released to ensure that it does
  // not have a visible title until its animation to ideal bounds is complete.
  AppListItemView* released_drag_view = drag_view_;

  ClearDragState();
  UpdatePaging();
  if (GetWidget()) {
    // Normally Layout() cancels any animations. At this point there may be a
    // pending Layout(), force it now so that one isn't triggered part way
    // through the animation. Further, ignore this layout so that the position
    // isn't reset.
    DCHECK(!ignore_layout_);
    base::AutoReset<bool> auto_reset(&ignore_layout_, true);
    GetWidget()->LayoutRootViewIfNecessary();
  }
  // AnimateToIdealBounds uses a BoundsAnimator to reposition the dragged view
  // to its ideal bounds. For cardified state, we need to animate the drag view
  // to ideal bounds using Transforms in AnimateToCardifiedState.
  if (!cardified_state_)
    AnimateToIdealBounds(released_drag_view);
  if (!cancel && !folder_delegate_)
    view_structure_.SaveToMetadata();

  if (!cancel) {
    // Select the page where dragged item is dropped. Avoid doing so when the
    // dragged item ends up in a folder.
    const int model_index = GetModelIndexOfItem(drag_item);
    if (model_index < view_model_.view_size()) {
      pagination_model_.SelectPage(GetIndexFromModelIndex(model_index).page,
                                   false /* animate */);
    }
  }

  // Hide the |current_ghost_view_| for item drag that started
  // within |apps_grid_view_|.
  BeginHideCurrentGhostImageView();
  MaybeStopPageFlip();
  if (cardified_state_) {
    if (!reparented_into_folder) {
      // Temporarily set to cardified UI State so it animates back to its
      // position smoothly with all other icons.
      released_drag_view->EnterCardifyState();
    }
    // Compensate drag_source_bounds for the translation of the items_container
    // during AnimateCardifiedState().
    gfx::Point start_position = items_container_->origin();
    MaybeEndCardifiedView();
    drag_source_bounds.Offset(
        0, start_position.y() - items_container_->origin().y());
  }
  if (folder_item_view) {
    // Run an animation to move dragged item to the folder.
    StartFolderDroppingAnimation(folder_item_view, drag_item,
                                 drag_source_bounds);
  }
}

AppListItemView* AppsGridView::GetItemViewAt(int index) const {
  if (index < 0 || index >= view_model_.view_size())
    return nullptr;
  return view_model_.view_at(index);
}

void AppsGridView::InitiateDragFromReparentItemInRootLevelGridView(
    AppListItemView* original_drag_view,
    const gfx::Rect& drag_view_rect,
    const gfx::Point& drag_point,
    bool has_native_drag) {
  DCHECK(original_drag_view && !drag_view_);
  DCHECK(!dragging_for_reparent_item_);

  // Since the item is new, its placeholder is conceptually at the back of the
  // entire apps grid.
  reorder_placeholder_ = GetLastTargetIndex();

  // Create a new AppListItemView to duplicate the original_drag_view in the
  // folder's grid view.
  auto view = CreateViewForItem(original_drag_view->item());
  items_need_layer_for_drag_ = true;
  auto* view_ptr = items_container_->AddChildView(std::move(view));
  for (const auto& entry : view_model_.entries())
    static_cast<AppListItemView*>(entry.view)->EnsureLayer();
  view_ptr->EnsureLayer();
  drag_view_ = view_ptr;

  // Dragged view should have focus. This also fixed the issue
  // https://crbug.com/834682.
  drag_view_->RequestFocus();
  gfx::Point converted_origin = drag_view_rect.origin();
  ConvertPointToTarget(this, items_container_, &converted_origin);
  drag_view_->SetBoundsRect(gfx::Rect(converted_origin, drag_view_rect.size()));
  drag_view_->SetDragUIState();  // Hide the title of the drag_view_.

  // Hide the drag_view_ for drag icon proxy when a native drag is responsible
  // for showing the icon.
  if (has_native_drag)
    SetViewHidden(drag_view_, true /* hide */, true /* no animate */);

  // Add drag_view_ to the end of the view_model_.
  view_model_.Add(drag_view_, view_model_.view_size());
  if (!folder_delegate_)
    view_structure_.Add(drag_view_, GetLastTargetIndex());

  drag_start_grid_view_ = drag_point;

  drag_view_start_ = drag_view_->origin();

  // Set the flag in root level grid view.
  dragging_for_reparent_item_ = true;

  MaybeStartCardifiedView();
}

void AppsGridView::UpdateDragFromReparentItem(Pointer pointer,
                                              const gfx::Point& drag_point) {
  // Note that if a cancel ocurrs while reparenting, the |drag_view_| in both
  // root and folder grid views is cleared, so the check in UpdateDragFromItem()
  // for |drag_view_| being nullptr (in the folder grid) is sufficient.
  DCHECK(drag_view_);
  DCHECK(IsDraggingForReparentInRootLevelGridView());

  UpdateDrag(pointer, drag_point);
}

bool AppsGridView::IsDragging() const {
  return drag_pointer_ != NONE;
}

bool AppsGridView::IsDraggedView(const AppListItemView* view) const {
  return drag_view_ == view;
}

bool AppsGridView::IsDragViewMoved(const AppListItemView& view) const {
  return IsDraggedView(&view) && drag_view_start_ != view.origin();
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

  // Drag may end before |host_drag_start_timer_| gets fired.
  if (host_drag_start_timer_.IsRunning())
    host_drag_start_timer_.AbandonAndStop();

  if (drag_view_) {
    drag_view_->OnDragEnded();
    if (IsDraggingForReparentInRootLevelGridView()) {
      const int drag_view_index = view_model_.GetIndexOfView(drag_view_);
      CHECK_EQ(view_model_.view_size() - 1, drag_view_index);
      DeleteItemViewAtIndex(drag_view_index, true /* sanitize */);
    }
  }
  drag_view_ = nullptr;
  dragging_for_reparent_item_ = false;
  extra_page_opened_ = false;
}

void AppsGridView::SetDragViewVisible(bool visible) {
  DCHECK(drag_view_);
  SetViewHidden(drag_view_, !visible, true);
}

void AppsGridView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  drag_and_drop_host_ = drag_and_drop_host;
}

bool AppsGridView::IsAnimatingView(AppListItemView* view) {
  return bounds_animator_->IsAnimating(view);
}

gfx::Size AppsGridView::CalculatePreferredSize() const {
  return GetTileGridSize();
}

bool AppsGridView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  // TODO(koz): Only accept a specific drag type for app shortcuts.
  *formats = OSExchangeData::FILE_NAME;
  return true;
}

bool AppsGridView::CanDrop(const OSExchangeData& data) {
  return true;
}

int AppsGridView::OnDragUpdated(const ui::DropTargetEvent& event) {
  return ui::DragDropTypes::DRAG_MOVE;
}

void AppsGridView::UpdateControlVisibility(AppListViewState app_list_state,
                                           bool is_in_drag) {
  const bool fullscreen_or_in_drag =
      is_in_drag || app_list_state == AppListViewState::kFullscreenAllApps ||
      app_list_state == AppListViewState::kFullscreenSearch;
  SetVisible(fullscreen_or_in_drag);
}

bool AppsGridView::OnKeyPressed(const ui::KeyEvent& event) {
  // The user may press VKEY_CONTROL before an arrow key when intending to do an
  // app move with control+arrow.
  if (event.key_code() == ui::VKEY_CONTROL)
    return true;

  if (selected_view_ && IsArrowKeyEvent(event) && event.IsControlDown()) {
    HandleKeyboardAppOperations(event.key_code(), event.IsShiftDown());
    return true;
  }

  // Let the FocusManager handle Left/Right keys.
  if (!IsUnhandledUpDownKeyEvent(event))
    return false;

  const bool arrow_up = event.key_code() == ui::VKEY_UP;
  return HandleVerticalFocusMovement(arrow_up);
}

bool AppsGridView::OnKeyReleased(const ui::KeyEvent& event) {
  if (event.IsControlDown() || !handling_keyboard_move_)
    return false;

  handling_keyboard_move_ = false;
  RecordAppMovingTypeMetrics(folder_delegate_ ? kReorderByKeyboardInFolder
                                              : kReorderByKeyboardInTopLevel);
  return false;
}

void AppsGridView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (!details.is_add && details.parent == items_container_) {
    // The view being delete should not have reference in |view_model_|.
    CHECK_EQ(-1, view_model_.GetIndexOfView(details.child));

    if (selected_view_ == details.child)
      selected_view_ = nullptr;
    if (activated_folder_item_view_ == details.child)
      activated_folder_item_view_ = nullptr;

    if (drag_view_ == details.child)
      EndDrag(true);

    if (app_list_features::IsAppGridGhostEnabled()) {
      if (current_ghost_view_ == details.child)
        current_ghost_view_ = nullptr;
      if (last_ghost_view_ == details.child)
        last_ghost_view_ = nullptr;
    }

    bounds_animator_->StopAnimatingView(details.child);
  }
}

bool AppsGridView::EventIsBetweenOccupiedTiles(const ui::LocatedEvent* event) {
  gfx::Point mirrored_point(GetMirroredXInView(event->location().x()),
                            event->location().y());
  return IsValidIndex(GetNearestTileIndexForPoint(mirrored_point));
}

void AppsGridView::Update() {
  DCHECK(!selected_view_ && !drag_view_);
  if (!folder_delegate_) {
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets(GetAppListConfig().grid_fadeout_mask_height(), 0)));
  }

  view_model_.Clear();
  if (!item_list_ || !item_list_->item_count())
    return;
  for (size_t i = 0; i < item_list_->item_count(); ++i) {
    // Skip "page break" items.
    if (item_list_->item_at(i)->is_page_break())
      continue;
    std::unique_ptr<AppListItemView> view = CreateViewForItemAtIndex(i);
    view_model_.Add(view.get(), view_model_.view_size());
    items_container_->AddChildView(std::move(view));
  }
  if (!folder_delegate_)
    view_structure_.LoadFromMetadata();
  UpdateColsAndRowsForFolder();
  UpdatePaging();
  UpdatePulsingBlockViews();
  Layout();
  SchedulePaint();

  if (!folder_delegate_)
    RecordPageMetrics();
}

int AppsGridView::TilesPerPage() const {
  if (folder_delegate_)
    return GetAppListConfig().max_folder_items_per_page();

  return GetAppListConfig().preferred_cols() *
         GetAppListConfig().preferred_rows();
}

void AppsGridView::UpdatePaging() {
  if (!folder_delegate_) {
    pagination_model_.SetTotalPages(view_structure_.total_pages());
    return;
  }

  const int tiles = view_model_.view_size();
  const int tiles_per_page = TilesPerPage();
  const int total_pages =
      tiles / tiles_per_page + (tiles % tiles_per_page ? 1 : 0);
  pagination_model_.SetTotalPages(total_pages);
}

void AppsGridView::UpdatePulsingBlockViews() {
  const int existing_items = item_list_ ? item_list_->item_count() : 0;
  const int tiles_per_page = TilesPerPage();
  const int available_slots =
      tiles_per_page - (existing_items % tiles_per_page);
  const int desired = model_->status() == AppListModelStatus::kStatusSyncing
                          ? available_slots
                          : 0;

  if (pulsing_blocks_model_.view_size() == desired)
    return;

  while (pulsing_blocks_model_.view_size() > desired) {
    PulsingBlockView* view = pulsing_blocks_model_.view_at(0);
    pulsing_blocks_model_.Remove(0);
    delete view;
  }

  while (pulsing_blocks_model_.view_size() < desired) {
    auto view = std::make_unique<PulsingBlockView>(GetTotalTileSize(), true);
    pulsing_blocks_model_.Add(view.get(), 0);
    items_container_->AddChildView(std::move(view));
  }
}

std::unique_ptr<AppListItemView> AppsGridView::CreateViewForItem(
    AppListItem* item) {
  std::unique_ptr<AppListItemView> view =
      std::make_unique<AppListItemView>(this, item, app_list_view_delegate_);
  return view;
}

std::unique_ptr<AppListItemView> AppsGridView::CreateViewForItemAtIndex(
    size_t index) {
  // The |drag_view_| might be pending for deletion, therefore |view_model_|
  // may have one more item than |item_list_|.
  DCHECK_LE(index, item_list_->item_count());
  auto* item = item_list_->item_at(index);
  return CreateViewForItem(item);
}

void AppsGridView::EnsureViewVisible(const GridIndex& index) {
  if (pagination_model_.has_transition())
    return;

  if (IsValidIndex(index))
    pagination_model_.SelectPage(index.page, false);
}

void AppsGridView::SetSelectedItemByIndex(const GridIndex& index) {
  if (GetIndexOfView(selected_view_) == index)
    return;

  AppListItemView* new_selection = GetViewAtIndex(index);
  if (!new_selection)
    return;  // Keep current selection.

  if (selected_view_)
    selected_view_->SchedulePaint();

  EnsureViewVisible(index);
  selected_view_ = new_selection;
  selected_view_->SchedulePaint();
  selected_view_->NotifyAccessibilityEvent(ax::mojom::Event::kFocus, true);
  if (selected_view_->HasNotificationBadge()) {
    a11y_announcer_->AnnounceItemNotificationBadge(
        selected_view_->title()->GetText());
  }
}

GridIndex AppsGridView::GetIndexOfView(const AppListItemView* view) const {
  const int model_index = view_model_.GetIndexOfView(view);
  if (model_index == -1)
    return GridIndex();

  return GetIndexFromModelIndex(model_index);
}

AppListItemView* AppsGridView::GetViewAtIndex(const GridIndex& index) const {
  if (!IsValidIndex(index))
    return nullptr;

  const int model_index = GetModelIndexFromIndex(index);
  return GetItemViewAt(model_index);
}

const gfx::Vector2d AppsGridView::CalculateTransitionOffset(
    int page_of_view) const {
  gfx::Size grid_size = GetTileGridSize();

  // If there is a transition, calculates offset for current and target page.
  const int current_page = pagination_model_.selected_page();
  const PaginationModel::Transition& transition =
      pagination_model_.transition();
  const bool is_valid = pagination_model_.is_valid_page(transition.target_page);

  int multiplier = page_of_view;
  if (is_valid && abs(transition.target_page - current_page) > 1) {
    if (page_of_view == transition.target_page) {
      if (transition.target_page > current_page)
        multiplier = current_page + 1;
      else
        multiplier = current_page - 1;
    } else if (page_of_view != current_page) {
      multiplier = -1;
    }
  }

  if (IsScrollAxisVertical()) {
    const int page_height = grid_size.height() + GetPaddingBetweenPages();
    return gfx::Vector2d(0, page_height * multiplier);
  }

  // Page size including padding pixels. A tile.x + page_width means the same
  // tile slot in the next page.
  const int page_width = grid_size.width() + GetPaddingBetweenPages();
  return gfx::Vector2d(page_width * multiplier, 0);
}

void AppsGridView::CalculateIdealBoundsForFolder() {
  if (!folder_delegate_) {
    CalculateIdealBounds();
    return;
  }

  const int total_views =
      view_model_.view_size() + pulsing_blocks_model_.view_size();
  int slot_index = 0;
  for (int i = 0; i < total_views; ++i) {
    if (i < view_model_.view_size() && view_model_.view_at(i) == drag_view_)
      continue;

    GridIndex view_index = GetIndexFromModelIndex(slot_index);

    // Leaves a blank space in the grid for the current reorder placeholder.
    if (reorder_placeholder_ == view_index) {
      ++slot_index;
      view_index = GetIndexFromModelIndex(slot_index);
    }

    gfx::Rect tile_slot = GetExpectedTileBounds(view_index);
    tile_slot.Offset(CalculateTransitionOffset(view_index.page));
    if (i < view_model_.view_size()) {
      view_model_.set_ideal_bounds(i, tile_slot);
    } else {
      pulsing_blocks_model_.set_ideal_bounds(i - view_model_.view_size(),
                                             tile_slot);
    }

    ++slot_index;
  }
}

void AppsGridView::AnimateToIdealBounds(AppListItemView* released_drag_view) {
  gfx::Rect visible_bounds(GetVisibleBounds());
  gfx::Point visible_origin = visible_bounds.origin();
  ConvertPointToTarget(this, items_container_, &visible_origin);
  visible_bounds.set_origin(visible_origin);

  CalculateIdealBoundsForFolder();
  for (int i = 0; i < view_model_.view_size(); ++i) {
    AppListItemView* view = GetItemViewAt(i);
    if (view == drag_view_)
      continue;

    const gfx::Rect& target = view_model_.ideal_bounds(i);
    if (bounds_animator_->GetTargetBounds(view) == target)
      continue;

    const gfx::Rect& current = view->bounds();
    const bool current_visible = visible_bounds.Intersects(current);
    const bool target_visible = visible_bounds.Intersects(target);
    const bool visible = current_visible || target_visible;

    const int y_diff = target.y() - current.y();
    if (visible && y_diff && y_diff % GetTotalTileSize().height() == 0) {
      AnimationBetweenRows(view, current_visible, current, target_visible,
                           target);
    } else if (visible || bounds_animator_->IsAnimating(view)) {
      bounds_animator_->AnimateViewTo(view, target);
      bounds_animator_->SetAnimationDelegate(
          view,
          std::make_unique<ItemMoveAnimationDelegate>(
              view, view == released_drag_view /* is_released_drag_view */));
    } else {
      view->SetBoundsRect(target);
    }
  }

  // Destroy layers created for drag if they're not longer necessary.
  if (!bounds_animator_->IsAnimating())
    OnBoundsAnimatorDone(bounds_animator_.get());
}

void AppsGridView::AnimationBetweenRows(AppListItemView* view,
                                        bool animate_current,
                                        const gfx::Rect& current,
                                        bool animate_target,
                                        const gfx::Rect& target) {
  // Determine page of |current| and |target|.
  const int current_page =
      CompareHorizontalPointPositionToRect(current.origin(), GetLocalBounds());
  const int target_page =
      CompareHorizontalPointPositionToRect(target.origin(), GetLocalBounds());

  const int dir = current_page < target_page || (current_page == target_page &&
                                                 current.y() < target.y())
                      ? 1
                      : -1;

  std::unique_ptr<ui::Layer> layer;
  if (view->layer()) {
    if (animate_current) {
      layer = view->RecreateLayer();
      layer->SuppressPaint();

      view->layer()->SetFillsBoundsOpaquely(false);
      view->layer()->SetOpacity(0.f);
    }
  } else {
    view->EnsureLayer();
  }

  const gfx::Size total_tile_size = GetTotalTileSize();
  gfx::Rect current_out(current);
  current_out.Offset(dir * total_tile_size.width(), 0);

  gfx::Rect target_in(target);
  if (animate_target)
    target_in.Offset(-dir * total_tile_size.width(), 0);
  bounds_animator_->StopAnimatingView(view);
  view->SetBoundsRect(target_in);
  bounds_animator_->AnimateViewTo(view, target);

  bounds_animator_->SetAnimationDelegate(
      view, std::make_unique<RowMoveAnimationDelegate>(view, layer.release(),
                                                       current_out));
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
  // Ensure that |drag_point| is correct if RTL.
  drag_point->set_x(GetMirroredXInView(drag_point->x()));
}

void AppsGridView::UpdateDropTargetRegion() {
  DCHECK(drag_view_);

  gfx::Point point = drag_view_->GetIconBounds().CenterPoint();
  views::View::ConvertPointToTarget(drag_view_, this, &point);
  // Ensure that the drop target location is correct if RTL.
  point.set_x(GetMirroredXInView(point.x()));
  if (IsPointWithinDragBuffer(point)) {
    if (DragPointIsOverItem(point)) {
      drop_target_region_ = ON_ITEM;
      drop_target_ = GetNearestTileIndexForPoint(point);
      return;
    }

    UpdateDropTargetForReorder(point);
    drop_target_region_ = DragIsCloseToItem() ? NEAR_ITEM : BETWEEN_ITEMS;
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
  drop_target_region_ = DragIsCloseToItem() ? NEAR_ITEM : BETWEEN_ITEMS;
}

bool AppsGridView::DropTargetIsValidFolder() {
  AppListItemView* target_view =
      GetViewDisplayedAtSlotOnCurrentPage(drop_target_.slot);
  if (!target_view)
    return false;

  AppListItem* target_item = target_view->item();

  // Items can only be dropped into non-folders (which have no children) or
  // folders that have fewer than the max allowed items.
  // The OEM folder does not allow drag/drop of other items into it.
  const size_t kMaxItemCount = GetAppListConfig().max_folder_items_per_page() *
                               GetAppListConfig().max_folder_pages();
  if (target_item->ChildItemCount() >= kMaxItemCount ||
      IsOEMFolderItem(target_item)) {
    return false;
  }

  if (!IsValidIndex(drop_target_))
    return false;

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
      (GetAppListConfig().folder_dropping_circle_radius() *
       (cardified_state_ ? kCardifiedScale : 1.0f))) {
    return false;
  }

  return true;
}

bool AppsGridView::DraggedItemCanEnterFolder() {
  if (!IsFolderItem(drag_view_->item()) && !folder_delegate_)
    return true;
  return false;
}

gfx::Vector2d AppsGridView::GetGridCenteringOffset() const {
  if (!cardified_state_)
    return gfx::Vector2d();
  const gfx::Rect bounds = GetContentsBounds();
  const gfx::Size tile_grid_size = GetTileGridSize();
  return gfx::Vector2d((bounds.width() - tile_grid_size.width()) / 2,
                       (bounds.height() - tile_grid_size.height()) / 2);
}

void AppsGridView::UpdateDropTargetForReorder(const gfx::Point& point) {
  gfx::Rect bounds = GetContentsBounds();
  bounds.Inset(GetTilePadding());
  GridIndex nearest_tile_index = GetNearestTileIndexForPoint(point);
  gfx::Point reorder_placeholder_center =
      GetExpectedTileBounds(reorder_placeholder_).CenterPoint();

  int x_offset_direction = 0;
  if (nearest_tile_index == reorder_placeholder_) {
    x_offset_direction = reorder_placeholder_center.x() <= point.x() ? -1 : 1;
  } else {
    x_offset_direction = reorder_placeholder_ < nearest_tile_index ? -1 : 1;
  }

  const gfx::Size total_tile_size = GetTotalTileSize();
  int row = nearest_tile_index.slot / cols_;

  // Offset the target column based on the direction of the target. This will
  // result in earlier targets getting their reorder zone shifted backwards
  // and later targets getting their reorder zones shifted forwards.
  //
  // This makes reordering feel like the user is slotting items into the spaces
  // between apps.
  int x_offset =
      x_offset_direction * (total_tile_size.width() / 2 -
                            GetAppListConfig().folder_dropping_circle_radius() *
                                (cardified_state_ ? kCardifiedScale : 1.0f));
  int col = (point.x() - bounds.x() + x_offset - GetGridCenteringOffset().x()) /
            total_tile_size.width();
  col = base::ClampToRange(col, 0, cols_ - 1);
  drop_target_ =
      std::min(GridIndex(pagination_model_.selected_page(), row * cols_ + col),
               GetLastTargetIndexOfPage(pagination_model_.selected_page()));

  DCHECK(IsValidReorderTargetIndex(drop_target_));
}

bool AppsGridView::DragIsCloseToItem() {
  DCHECK(drag_view_);

  gfx::Point point = drag_view_->GetIconBounds().CenterPoint();
  views::View::ConvertPointToTarget(drag_view_, this, &point);
  // Ensure that the drop target location is correct if RTL.
  point.set_x(GetMirroredXInView(point.x()));

  GridIndex nearest_tile_index = GetNearestTileIndexForPoint(point);

  if (nearest_tile_index == reorder_placeholder_)
    return false;

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
      (GetAppListConfig().grid_tile_width() + horizontal_tile_padding_ * 2) *
      0.4 * (cardified_state_ ? kCardifiedScale : 1.0f);
  const int double_icon_radius =
      GetAppListConfig().folder_dropping_circle_radius() * 2 *
      (cardified_state_ ? kCardifiedScale : 1.0f);
  const int minimum_drag_distance_for_reorder =
      std::min(forty_percent_icon_spacing, double_icon_radius);

  if (distance_to_tile_center < minimum_drag_distance_for_reorder)
    return true;
  return false;
}

void AppsGridView::OnReorderTimer() {
  reorder_placeholder_ = drop_target_;
  MaybeCreateDragReorderAccessibilityEvent();
  AnimateToIdealBounds(nullptr /* released_drag_view */);
  CreateGhostImageView();
}

void AppsGridView::OnFolderItemReparentTimer() {
  DCHECK(folder_delegate_);
  if (drag_out_of_folder_container_ && drag_view_) {
    bool has_native_drag = drag_and_drop_host_ != nullptr;
    folder_delegate_->ReparentItem(drag_view_, last_drag_point_,
                                   has_native_drag);

    // Set the flag in the folder's grid view.
    dragging_for_reparent_item_ = true;

    // Do not observe any data change since it is going to be hidden.
    item_list_->RemoveObserver(this);
    item_list_ = nullptr;
  }
}

void AppsGridView::OnFolderDroppingTimer() {
  MaybeCreateFolderDroppingAccessibilityEvent();
  SetAsFolderDroppingTarget(drop_target_, true);
  BeginHideCurrentGhostImageView();
}

void AppsGridView::UpdateDragStateInsideFolder(Pointer pointer,
                                               const gfx::Point& drag_point) {
  if (IsUnderOEMFolder())
    return;

  if (IsDraggingForReparentInHiddenGridView()) {
    // Dispatch drag event to root level grid view for re-parenting folder
    // folder item purpose.
    DispatchDragEventForReparent(pointer, drag_point);
    return;
  }

  // Calculate if the drag_view_ is dragged out of the folder's container
  // ink bubble.
  bool is_item_dragged_out_of_folder =
      folder_delegate_->IsViewOutsideOfFolder(drag_view_);
  if (is_item_dragged_out_of_folder) {
    if (!drag_out_of_folder_container_) {
      folder_item_reparent_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kFolderItemReparentDelay), this,
          &AppsGridView::OnFolderItemReparentTimer);
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
  const gfx::Rect view_ideal_bounds =
      view_model_.ideal_bounds(view_model_.GetIndexOfView(folder_item_view));
  const gfx::Rect icon_ideal_bounds =
      folder_item_view->GetIconBoundsForTargetViewBounds(
          GetAppListConfig(), view_ideal_bounds,
          folder_item_view->GetIconImage().size(), /*icon_scale=*/1.0f);
  AppListFolderItem* folder_item =
      static_cast<AppListFolderItem*>(folder_item_view->item());
  return folder_item->GetTargetIconRectInFolderForItem(
      GetAppListConfig(), drag_item, icon_ideal_bounds);
}

bool AppsGridView::IsUnderOEMFolder() {
  if (!folder_delegate_)
    return false;

  return folder_delegate_->IsOEMFolder();
}

void AppsGridView::HandleKeyboardAppOperations(ui::KeyboardCode key_code,
                                               bool folder) {
  DCHECK(selected_view_);

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
  const GridIndex target_index = GetTargetGridIndexForKeyboardMove(key_code);
  if (!CanMoveSelectedToTargetForKeyboardFoldering(target_index))
    return;

  const std::u16string moving_view_title = selected_view_->title()->GetText();
  AppListItemView* target_view =
      GetViewDisplayedAtSlotOnCurrentPage(target_index.slot);
  const std::u16string target_view_title = target_view->title()->GetText();
  const bool target_view_is_folder = target_view->is_folder();

  AppListItemView* folder_item = MoveItemToFolder(selected_view_, target_index);
  a11y_announcer_->AnnounceKeyboardFoldering(
      moving_view_title, target_view_title, target_view_is_folder);
  DCHECK(folder_item->is_folder());
  folder_item->RequestFocus();
  Layout();
  RecordAppMovingTypeMetrics(kMoveByKeyboardIntoFolder);
}

bool AppsGridView::CanMoveSelectedToTargetForKeyboardFoldering(
    const GridIndex& target_index) const {
  DCHECK(selected_view_);

  // To folder an item, the item must be moved into the folder, not the folder
  // moved over the item.
  const AppListItem* selected_item = selected_view_->item();
  if (selected_item->is_folder())
    return false;

  // Do not allow foldering across pages because the destination folder cannot
  // be seen.
  if (target_index.page != GetIndexOfView(selected_view_).page)
    return false;

  return true;
}

bool AppsGridView::HandleVerticalFocusMovement(bool arrow_up) {
  views::View* focused = GetFocusManager()->GetFocusedView();
  if (focused->GetClassName() != AppListItemView::kViewClassName)
    return false;

  const GridIndex source_index =
      GetIndexOfView(static_cast<const AppListItemView*>(focused));
  int target_page = source_index.page;
  int target_row = source_index.slot / cols_ + (arrow_up ? -1 : 1);
  int target_col = source_index.slot % cols_;

  if (target_row < 0) {
    if (folder_delegate_) {
      // Move focus to search box if we are in folder.
      contents_view_->GetSearchBoxView()->search_box()->RequestFocus();
      return true;
    }

    // Move focus to the last row of previous page if target row is negative.
    --target_page;

    // |target_page| may be invalid which makes |target_row| invalid, but
    // |target_row| will not be used if |target_page| is invalid.
    target_row = (GetItemsNumOfPage(target_page) - 1) / cols_;
  } else if (target_row > (GetItemsNumOfPage(target_page) - 1) / cols_) {
    if (folder_delegate_) {
      // Move focus to folder name if we are in folder.
      contents_view_->apps_container_view()
          ->app_list_folder_view()
          ->folder_header_view()
          ->SetTextFocus();
      return true;
    }

    // Move focus to the first row of next page if target row is beyond range.
    ++target_page;
    target_row = 0;
  }

  if (target_page < 0) {
    // Move focus up outside the apps grid if target page is negative.
    views::View* v = GetFocusManager()->GetNextFocusableView(
        view_model_.view_at(0), /*starting_widget=*/nullptr, /*reverse=*/true,
        /*dont_loop=*/false);
    DCHECK(v);
    v->RequestFocus();
    return true;
  }

  if (target_page >= pagination_model_.total_pages()) {
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
      std::min(GetItemsNumOfPage(target_page) - 1, target_index.slot);
  if (IsValidIndex(target_index)) {
    GetViewAtIndex(target_index)->RequestFocus();
    return true;
  }
  return false;
}

void AppsGridView::UpdateColsAndRowsForFolder() {
  if (!folder_delegate_ || !item_list_->item_count())
    return;

  int prev_cols = cols_;
  int prev_rows = rows_per_page_;

  // Try to shape the apps grid into a square.
  int items_in_one_page = std::min(
      GetAppListConfig().max_folder_items_per_page(), item_list_->item_count());
  cols_ = std::sqrt(items_in_one_page - 1) + 1;
  rows_per_page_ = (items_in_one_page - 1) / cols_ + 1;

  // Update the folder bounds if the number of columns or rows changed.
  if (prev_cols != cols_ || prev_rows != rows_per_page_) {
    folder_delegate_->UpdateFolderBounds();
  }
}

void AppsGridView::DispatchDragEventForReparent(Pointer pointer,
                                                const gfx::Point& drag_point) {
  folder_delegate_->DispatchDragEventForReparent(pointer, drag_point);
}

void AppsGridView::EndDragFromReparentItemInRootLevel(
    bool events_forwarded_to_drag_drop_host,
    bool cancel_drag) {
  // EndDrag was called before if |drag_view_| is nullptr.
  if (!drag_view_)
    return;

  DCHECK(activated_folder_item_view_);
  static_cast<AppListFolderItem*>(activated_folder_item_view_->item())
      ->NotifyOfDraggedItem(nullptr);

  DCHECK(IsDraggingForReparentInRootLevelGridView());
  bool cancel_reparent = cancel_drag || drop_target_region_ == NO_TARGET;

  // This is the folder view to drop an item into. Cache the |drag_view_|'s item
  // and its bounds for later use in folder dropping animation.
  AppListItemView* folder_item_view = nullptr;
  AppListItem* drag_item = drag_view_->item();
  const gfx::Rect drag_source_bounds(drag_view_->bounds());

  if (!events_forwarded_to_drag_drop_host && !cancel_reparent) {
    UpdateDropTargetRegion();
    if (drop_target_region_ == ON_ITEM && DropTargetIsValidFolder() &&
        DraggedItemCanEnterFolder()) {
      cancel_reparent = !ReparentItemToAnotherFolder(drag_view_, drop_target_);
      // Announce folder dropping event before end of drag of reparented item.
      MaybeCreateFolderDroppingAccessibilityEvent();
      if (!cancel_reparent) {
        folder_item_view =
            GetViewDisplayedAtSlotOnCurrentPage(drop_target_.slot);
      }
    } else if (drop_target_region_ != NO_TARGET &&
               IsValidReorderTargetIndex(drop_target_)) {
      ReparentItemForReorder(drag_view_, drop_target_);
      RecordAppMovingTypeMetrics(kMoveByDragOutOfFolder);
      // Announce accessibility event before the end of drag for reparented
      // item.
      MaybeCreateDragReorderAccessibilityEvent();
    } else {
      NOTREACHED();
    }
    SetViewHidden(drag_view_, false /* show */, true /* no animate */);
  }

  MaybeEndCardifiedView();
  SetAsFolderDroppingTarget(drop_target_, false);

  AppListItemView* released_drag_view = nullptr;
  if (!cancel_reparent) {
    // By setting |drag_view_| to nullptr here, we prevent ClearDragState() from
    // cleaning up the newly created AppListItemView, effectively claiming
    // ownership of the newly created drag view.
    drag_view_->OnDragEnded();
    // Hide the title if the item is being dropped into another folder, so it
    // doesn't flash during transition. Otherwise, the item is being dropped
    // into the root apps grid - pass the released view to
    // AnimateToIdealBounds(), which will ensure the title remains hidden
    // during the item view bounds animation to the target apps grid location.
    if (folder_item_view) {
      drag_view_->title()->SetVisible(false);
    } else {
      released_drag_view = drag_view_;
    }
    drag_view_ = nullptr;
  }
  UpdatePaging();
  ClearDragState();
  AnimateToIdealBounds(released_drag_view);
  if (!folder_delegate_)
    view_structure_.SaveToMetadata();

  if (cancel_reparent) {
    // Run an animation to move dragged item back to its original folder.
    StartFolderDroppingAnimation(activated_folder_item_view_, drag_item,
                                 drag_source_bounds);
  } else if (folder_item_view) {
    // Run an animation to move dragged item to the new folder.
    StartFolderDroppingAnimation(folder_item_view, drag_item,
                                 drag_source_bounds);
  }

  // Hide the |current_ghost_view_| after completed drag from within
  // folder to |apps_grid_view_|.
  BeginHideCurrentGhostImageView();
  MaybeStopPageFlip();
}

void AppsGridView::EndDragForReparentInHiddenFolderGridView() {
  if (drag_and_drop_host_) {
    // If we had a drag and drop proxy icon, we delete it and make the real
    // item visible again.
    drag_and_drop_host_->DestroyDragIconProxy();
  }

  SetAsFolderDroppingTarget(drop_target_, false);
  ClearDragState();

  // Hide |current_ghost_view_| in the hidden folder grid view.
  BeginHideCurrentGhostImageView();
}

void AppsGridView::OnFolderItemRemoved() {
  DCHECK(folder_delegate_);
  if (item_list_)
    item_list_->RemoveObserver(this);
  item_list_ = nullptr;
}

void AppsGridView::HandleKeyboardReparent(AppListItemView* reparented_view,
                                          ui::KeyboardCode key_code) {
  DCHECK(key_code == ui::VKEY_LEFT || key_code == ui::VKEY_RIGHT ||
         key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN);
  DCHECK(!folder_delegate_);
  DCHECK(activated_folder_item_view_);

  auto* reparented_view_in_root_grid = items_container_->AddChildView(
      CreateViewForItem(reparented_view->item()));
  view_model_.Add(reparented_view_in_root_grid, view_model_.view_size());
  view_structure_.Add(reparented_view_in_root_grid, GetLastTargetIndex());

  // Set |activated_folder_item_view_| selected so |target_index| will be
  // computed relative to the open folder.
  SetSelectedView(activated_folder_item_view_);
  const GridIndex target_index =
      GetTargetGridIndexForKeyboardReparent(key_code);
  AnnounceReorder(target_index);
  ReparentItemForReorder(reparented_view_in_root_grid, target_index);

  GetViewAtIndex(target_index)->RequestFocus();
  Layout();
  RecordAppMovingTypeMetrics(kMoveByKeyboardOutOfFolder);
}

AppListItemView* AppsGridView::GetCurrentPageFirstItemViewInFolder() {
  DCHECK(folder_delegate_);
  int first_index = pagination_model_.selected_page() *
                    GetAppListConfig().max_folder_items_per_page();
  return view_model_.view_at(first_index);
}

AppListItemView* AppsGridView::GetCurrentPageLastItemViewInFolder() {
  DCHECK(folder_delegate_);
  int last_index =
      std::min((pagination_model_.selected_page() + 1) *
                       GetAppListConfig().max_folder_items_per_page() -
                   1,
               item_list_->item_count() - 1);
  return view_model_.view_at(last_index);
}

void AppsGridView::UpdatePagedViewStructure() {
  if (!folder_delegate_)
    view_structure_.SaveToMetadata();
}

bool AppsGridView::IsTabletMode() const {
  return app_list_view_delegate_->IsInTabletMode();
}

void AppsGridView::OnAppListConfigUpdated() {
  for (int i = 0; i < view_model_.view_size(); ++i)
    view_model_.view_at(i)->RefreshIcon();

  InvalidateLayout();
}

const AppListConfig& AppsGridView::GetAppListConfig() const {
  // TODO(crbug.com/1211608): Get the real config. This method cannot be pure
  // virtual and implemented in subclasses because AppListItemView may call
  // GetAppListConfig() during this object's destruction.
  if (!contents_view_) {
    AppListConfig* config = AppListConfigProvider::Get().GetConfigForType(
        AppListConfigType::kLarge, /*can_create=*/true);
    return *config;
  }
  return contents_view_->app_list_view()->GetAppListConfig();
}

bool AppsGridView::IsAnimationRunningForTest() {
  return bounds_animator_->IsAnimating() ||
         bounds_animation_for_cardified_state_in_progress_ > 0;
}

void AppsGridView::CancelAnimationsForTest() {
  bounds_animator_->Cancel();

  const int total_views = view_model_.view_size();
  for (int i = 0; i < total_views; ++i) {
    if (view_model_.view_at(i)->layer())
      view_model_.view_at(i)->layer()->CompleteAllAnimations();
  }
}

bool AppsGridView::FireFolderItemReparentTimerForTest() {
  if (!folder_item_reparent_timer_.IsRunning())
    return false;
  folder_item_reparent_timer_.FireNow();
  return true;
}

bool AppsGridView::FireFolderDroppingTimerForTest() {
  if (!folder_dropping_timer_.IsRunning())
    return false;
  folder_dropping_timer_.FireNow();
  return true;
}

bool AppsGridView::FireDragToShelfTimerForTest() {
  if (!host_drag_start_timer_.IsRunning())
    return false;
  host_drag_start_timer_.FireNow();
  return true;
}

void AppsGridView::StartDragAndDropHostDrag(const gfx::Point& grid_location) {
  // When a drag and drop host is given, the item can be dragged out of the app
  // list window. In that case a proxy widget needs to be used.
  if (!drag_view_ || !drag_and_drop_host_)
    return;

  // We have to hide the original item since the drag and drop host will do
  // the OS dependent code to "lift off the dragged item". Apply the scale
  // factor of this view's transform to the dragged view as well.
  DCHECK(!IsDraggingForReparentInRootLevelGridView());
  drag_and_drop_host_->CreateDragIconProxyByLocationWithNoAnimation(
      drag_view_->GetIconBoundsInScreen().origin(), drag_view_->GetIconImage(),
      drag_view_,
      drag_view_->item()->is_folder() ? kDragAndDropProxyScale : 1.0f,
      drag_view_->item()->is_folder() && IsTabletMode()
          ? GetAppListConfig().blur_radius()
          : 0);

  SetViewHidden(drag_view_, true /* hide */, true /* no animation */);
}

void AppsGridView::DispatchDragEventToDragAndDropHost(
    const gfx::Point& location_in_screen_coordinates) {
  if (!drag_view_ || !drag_and_drop_host_)
    return;

  const bool should_host_start_drag = drag_and_drop_host_->ShouldStartDrag(
      drag_view_->item()->id(), location_in_screen_coordinates);
  if (!should_host_start_drag && host_drag_start_timer_.IsRunning())
    host_drag_start_timer_.AbandonAndStop();

  if (GetLocalBounds().Contains(last_drag_point_)) {
    // The event was issued inside the app menu and we should get all events.
    if (forward_events_to_drag_and_drop_host_) {
      // The DnD host was previously called and needs to be informed that the
      // session returns to the owner.
      forward_events_to_drag_and_drop_host_ = false;
      drag_and_drop_host_->EndDrag(true);
    }
    return;
  }

  if (IsFolderItem(drag_view_->item()))
    return;

  // The event happened outside our app menu and we might need to dispatch.
  if (forward_events_to_drag_and_drop_host_) {
    // Dispatch since we have already started.
    if (!drag_and_drop_host_->Drag(location_in_screen_coordinates)) {
      // The host is not active any longer and we cancel the operation.
      forward_events_to_drag_and_drop_host_ = false;
      drag_and_drop_host_->EndDrag(true);
    }
    return;
  }

  if (should_host_start_drag && !host_drag_start_timer_.IsRunning()) {
    host_drag_start_timer_.Start(FROM_HERE, kShelfHandleIconDragDelay, this,
                                 &AppsGridView::OnHostDragStartTimerFired);
    MaybeStopPageFlip();
  }
}

void AppsGridView::MoveItemInModel(AppListItemView* item_view,
                                   const GridIndex& target,
                                   bool clear_overflow) {
  int current_model_index = view_model_.GetIndexOfView(item_view);
  CHECK_GE(current_model_index, 0);
  size_t current_item_list_index = 0;
  bool found = item_list_->FindItemIndex(item_view->item()->id(),
                                         &current_item_list_index);
  CHECK(found);

  int target_model_index = GetTargetModelIndexForMove(item_view, target);
  size_t target_item_list_index = GetTargetItemIndexForMove(item_view, target);
  // The same item index does not guarantee the same visual index, so move the
  // item visual index here.
  if (!folder_delegate_)
    view_structure_.Move(item_view, target, clear_overflow);

  DVLOG(1) << "MoveItemInModel: view model: " << current_model_index << " -> "
           << target_model_index << ", item list: " << current_item_list_index
           << " -> " << target_item_list_index;

  // Reorder the app list item views in accordance with |view_model_|.
  items_container_->ReorderChildView(item_view, target_model_index);
  items_container_->NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                                             true /* send_native_event */);

  if (target_item_list_index == current_item_list_index)
    return;

  item_list_->RemoveObserver(this);
  item_list_->MoveItem(current_item_list_index, target_item_list_index);
  view_model_.Move(current_model_index, target_model_index);
  item_list_->AddObserver(this);
}

AppListItemView* AppsGridView::MoveItemToFolder(AppListItemView* item_view,
                                                const GridIndex& target) {
  const std::string& source_item_id = item_view->item()->id();
  AppListItemView* target_view =
      GetViewDisplayedAtSlotOnCurrentPage(target.slot);
  DCHECK(target_view);
  const std::string& target_view_item_id = target_view->item()->id();

  // Check that the item is not being dropped onto itself; this should not
  // happen, but it can if something allows multiple views to share an
  // item (e.g., if a folder drop does not clean up properly).
  DCHECK_NE(source_item_id, target_view_item_id);

  // Make change to data model.
  item_list_->RemoveObserver(this);
  std::string folder_item_id =
      model_->MergeItems(target_view_item_id, source_item_id);
  item_list_->AddObserver(this);
  if (folder_item_id.empty()) {
    LOG(ERROR) << "Unable to merge into item id: " << target_view_item_id;
    return nullptr;
  }
  if (folder_item_id != target_view_item_id) {
    // New folder was created, change the view model to replace the old target
    // view with the new folder item view.
    size_t folder_item_index;
    if (item_list_->FindItemIndex(folder_item_id, &folder_item_index)) {
      int target_model_index = view_model_.GetIndexOfView(target_view);
      GridIndex target_index = GetIndexOfView(target_view);
      gfx::Rect target_view_bounds = target_view->bounds();
      DeleteItemViewAtIndex(target_model_index, false /* sanitize */);
      std::unique_ptr<AppListItemView> new_target_view =
          CreateViewForItemAtIndex(folder_item_index);
      new_target_view->SetBoundsRect(target_view_bounds);
      view_model_.Add(new_target_view.get(), target_model_index);
      if (!folder_delegate_)
        view_structure_.Add(new_target_view.get(), target_index);

      // If drag view is in front of the position where it will be moved to, we
      // should skip it.
      const int offset = (drag_view_ && view_model_.GetIndexOfView(drag_view_) <
                                            target_model_index)
                             ? 1
                             : 0;
      target_view = items_container_->AddChildViewAt(
          std::move(new_target_view), target_model_index - offset);
    } else {
      LOG(ERROR) << "Folder no longer in item_list: " << folder_item_id;
    }
  }

  FadeOutItemViewAndDelete(item_view);
  if (drag_view_ == item_view)
    RecordAppMovingTypeMetrics(kMoveByDragIntoFolder);

  return target_view;
}

void AppsGridView::FadeOutItemViewAndDelete(AppListItemView* item_view) {
  const int model_index = view_model_.GetIndexOfView(item_view);

  view_model_.Remove(model_index);
  if (!folder_delegate_)
    view_structure_.Remove(item_view);
  item_view->title()->SetVisible(false);
  bounds_animator_->AnimateViewTo(item_view, item_view->bounds());
  bounds_animator_->SetAnimationDelegate(
      item_view, std::unique_ptr<gfx::AnimationDelegate>(
                     new ItemRemoveAnimationDelegate(item_view)));
}

void AppsGridView::ReparentItemForReorder(AppListItemView* item_view,
                                          const GridIndex& target) {
  item_list_->RemoveObserver(this);
  model_->RemoveObserver(this);

  AppListItem* reparent_item = item_view->item();
  DCHECK(reparent_item->IsInFolder());
  const std::string source_folder_id = reparent_item->folder_id();
  AppListFolderItem* source_folder =
      static_cast<AppListFolderItem*>(item_list_->FindItem(source_folder_id));

  int target_model_index = GetTargetModelIndexForMove(item_view, target);
  int target_item_index = GetTargetItemIndexForMove(item_view, target);

  // Remove the source folder view if there is only 1 item in it, since the
  // source folder will be deleted after its only child item removed from it.
  GridIndex target_override = target;
  if (source_folder->ChildItemCount() == 1u) {
    const int deleted_folder_index =
        view_model_.GetIndexOfView(activated_folder_item_view_);
    const GridIndex deleted_folder_grid_index =
        GetIndexOfView(activated_folder_item_view_);
    DeleteItemViewAtIndex(deleted_folder_index, false /* sanitize */);

    // Adjust |target_model_index| if it is beyond the deleted folder index.
    if (target_model_index > deleted_folder_index) {
      --target_model_index;

      // Do not decrement |target_item_index| since the folder item has not been
      // removed from the item list yet.
    }

    // Adjust |target_override| if it is beyond the deleted folder grid index in
    // the same page.
    if (!folder_delegate_ && target.page == deleted_folder_grid_index.page &&
        target.slot > deleted_folder_grid_index.slot) {
      --target_override.slot;
    }
  }

  // Move the item from its parent folder to top level item list.
  // Must move to target_model_index, the location we expect the target item
  // to be, not the item location we want to insert before.
  int current_model_index = view_model_.GetIndexOfView(item_view);
  syncer::StringOrdinal target_position;
  if (target_item_index < static_cast<int>(item_list_->item_count()))
    target_position = item_list_->item_at(target_item_index)->position();
  model_->MoveItemToFolderAt(reparent_item, "", target_position);
  view_model_.Move(current_model_index, target_model_index);
  if (!folder_delegate_)
    view_structure_.Move(item_view, target_override);
  items_container_->ReorderChildView(item_view, target_model_index);
  items_container_->NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                                             true /* send_native_event */);

  RemoveLastItemFromReparentItemFolderIfNecessary(source_folder_id);

  item_list_->AddObserver(this);
  model_->AddObserver(this);
}

bool AppsGridView::ReparentItemToAnotherFolder(AppListItemView* item_view,
                                               const GridIndex& target) {
  DCHECK(IsDraggingForReparentInRootLevelGridView());

  AppListItemView* target_view =
      GetViewDisplayedAtSlotOnCurrentPage(target.slot);
  if (!target_view)
    return false;

  AppListItem* reparent_item = item_view->item();
  DCHECK(reparent_item->IsInFolder());
  const std::string source_folder_id = reparent_item->folder_id();
  AppListFolderItem* source_folder =
      static_cast<AppListFolderItem*>(item_list_->FindItem(source_folder_id));

  AppListItem* target_item = target_view->item();

  // An app is being reparented to its original folder. Just cancel the
  // reparent.
  if (target_item->id() == reparent_item->folder_id())
    return false;

  // Make change to data model.
  item_list_->RemoveObserver(this);

  // Remove the source folder view if there is only 1 item in it, since the
  // source folder will be deleted after its only child item merged into the
  // target item.
  if (source_folder->ChildItemCount() == 1u) {
    DeleteItemViewAtIndex(
        view_model_.GetIndexOfView(activated_folder_item_view()),
        false /* sanitize */);
  }

  // Move item to the target folder.
  std::string target_id_after_merge =
      model_->MergeItems(target_item->id(), reparent_item->id());
  if (target_id_after_merge.empty()) {
    LOG(ERROR) << "Unable to reparent to item id: " << target_item->id();
    item_list_->AddObserver(this);
    return false;
  }

  if (target_id_after_merge != target_item->id()) {
    // New folder was created, change the view model to replace the old target
    // view with the new folder item view.
    const std::string& new_folder_id = reparent_item->folder_id();
    size_t new_folder_index;
    if (item_list_->FindItemIndex(new_folder_id, &new_folder_index)) {
      // Save the target view's bounds before deletion, which will be used as
      // new folder view's bounds.
      gfx::Rect target_rect = target_view->bounds();
      int target_model_index = view_model_.GetIndexOfView(target_view);
      GridIndex target_index = GetIndexOfView(target_view);
      DeleteItemViewAtIndex(target_model_index, false /* sanitize */);
      std::unique_ptr<AppListItemView> new_folder_view =
          CreateViewForItemAtIndex(new_folder_index);
      new_folder_view->SetBoundsRect(target_rect);
      view_model_.Add(new_folder_view.get(), target_model_index);
      if (!folder_delegate_)
        view_structure_.Add(new_folder_view.get(), target_index);
      items_container_->AddChildViewAt(std::move(new_folder_view),
                                       target_model_index);
    } else {
      LOG(ERROR) << "Folder no longer in item_list: " << new_folder_id;
    }
  }

  RemoveLastItemFromReparentItemFolderIfNecessary(source_folder_id);

  item_list_->AddObserver(this);

  // Fade out the drag_view_ and delete it when animation ends.
  int drag_model_index = view_model_.GetIndexOfView(drag_view_);
  view_model_.Remove(drag_model_index);
  if (!folder_delegate_)
    view_structure_.Remove(drag_view_);
  bounds_animator_->AnimateViewTo(drag_view_, drag_view_->bounds());
  bounds_animator_->SetAnimationDelegate(
      drag_view_, std::unique_ptr<gfx::AnimationDelegate>(
                      new ItemRemoveAnimationDelegate(drag_view_)));

  RecordAppMovingTypeMetrics(kMoveIntoAnotherFolder);
  return true;
}

// After moving the re-parenting item out of the folder, if there is only 1 item
// left, remove the last item out of the folder, delete the folder and insert it
// to the data model at the same position. Make the same change to view_model_
// accordingly.
void AppsGridView::RemoveLastItemFromReparentItemFolderIfNecessary(
    const std::string& source_folder_id) {
  AppListFolderItem* source_folder =
      static_cast<AppListFolderItem*>(item_list_->FindItem(source_folder_id));
  if (!source_folder || (source_folder && !source_folder->ShouldAutoRemove()))
    return;

  // Save the folder item view's bounds before deletion, which will be used as
  // last item view's bounds.
  gfx::Rect folder_rect = activated_folder_item_view()->bounds();
  const GridIndex target_index = GetIndexOfView(activated_folder_item_view());
  const int target_model_index =
      view_model_.GetIndexOfView(activated_folder_item_view());

  // Delete view associated with the folder item to be removed.
  DeleteItemViewAtIndex(
      view_model_.GetIndexOfView(activated_folder_item_view()),
      false /* sanitize */);

  // For single-app folders (which can exist for system-managed folders, see
  // crbug.com/925052) there will not be a "last item" so we can ignore the
  // rest.
  if (!source_folder || source_folder->item_list()->item_count() != 1)
    return;

  // Now make the data change to remove the folder item in model.
  AppListItem* last_item = source_folder->item_list()->item_at(0);
  model_->MoveItemToFolderAt(last_item, "", source_folder->position());

  // Create a new item view for the last item in folder.
  size_t last_item_index;
  if (!item_list_->FindItemIndex(last_item->id(), &last_item_index) ||
      last_item_index > item_list_->item_count()) {
    NOTREACHED();
    return;
  }
  std::unique_ptr<AppListItemView> last_item_view =
      CreateViewForItemAtIndex(last_item_index);
  last_item_view->SetBoundsRect(folder_rect);
  view_model_.Add(last_item_view.get(), target_model_index);
  if (!folder_delegate_)
    view_structure_.Add(last_item_view.get(), target_index);
  items_container_->AddChildViewAt(std::move(last_item_view),
                                   target_model_index);
}

void AppsGridView::CancelContextMenusOnCurrentPage() {
  GridIndex start_index(pagination_model_.selected_page(), 0);
  if (!IsValidIndex(start_index))
    return;
  int start = GetModelIndexFromIndex(start_index);
  int end = std::min(view_model_.view_size(), start + TilesPerPage());
  for (int i = start; i < end; ++i)
    GetItemViewAt(i)->CancelContextMenu();
}

void AppsGridView::DeleteItemViewAtIndex(int index, bool sanitize) {
  AppListItemView* item_view = GetItemViewAt(index);
  view_model_.Remove(index);
  if (!folder_delegate_) {
    view_structure_.Remove(item_view, sanitize /* clear_overflow */,
                           sanitize /* clear_empty_pages */);
  }
  if (item_view == drag_view_)
    drag_view_ = nullptr;
  delete item_view;
}

bool AppsGridView::IsPointWithinDragBuffer(const gfx::Point& point) const {
  gfx::Rect rect(GetLocalBounds());
  rect.Inset(-kDragBufferPx, -kDragBufferPx, -kDragBufferPx, -kDragBufferPx);
  return rect.Contains(point);
}

void AppsGridView::OnListItemAdded(size_t index, AppListItem* item) {
  EndDrag(true);

  if (!item->is_page_break()) {
    std::unique_ptr<AppListItemView> view = CreateViewForItemAtIndex(index);
    int model_index = GetTargetModelIndexFromItemIndex(index);
    view_model_.Add(view.get(), model_index);
    items_container_->AddChildViewAt(std::move(view), model_index);
  }

  if (!folder_delegate_)
    view_structure_.LoadFromMetadata();
  UpdateColsAndRowsForFolder();
  UpdatePaging();
  UpdatePulsingBlockViews();
  Layout();
  SchedulePaint();
}

void AppsGridView::OnListItemRemoved(size_t index, AppListItem* item) {
  EndDrag(true);

  if (!item->is_page_break())
    DeleteItemViewAtIndex(GetModelIndexOfItem(item), true /* sanitize */);

  if (!folder_delegate_)
    view_structure_.LoadFromMetadata();
  UpdateColsAndRowsForFolder();
  UpdatePaging();
  UpdatePulsingBlockViews();
  Layout();
  SchedulePaint();
}

void AppsGridView::OnListItemMoved(size_t from_index,
                                   size_t to_index,
                                   AppListItem* item) {
  EndDrag(true);

  if (item->is_page_break()) {
    LOG(ERROR) << "Page break item is moved: " << item->id();
  } else {
    // The item is updated in the item list but the view_model is not updated,
    // so get current model index by looking up view_model and predict the
    // target model index based on its current item index.
    int from_model_index = GetModelIndexOfItem(item);
    int to_model_index = GetTargetModelIndexFromItemIndex(to_index);
    view_model_.Move(from_model_index, to_model_index);
    items_container_->ReorderChildView(view_model_.view_at(to_model_index),
                                       to_model_index);
    items_container_->NotifyAccessibilityEvent(
        ax::mojom::Event::kChildrenChanged, true /* send_native_event */);
  }

  if (!folder_delegate_)
    view_structure_.LoadFromMetadata();
  UpdateColsAndRowsForFolder();
  UpdatePaging();
  if (GetWidget() && GetWidget()->IsVisible())
    AnimateToIdealBounds(nullptr /* released_drag_view */);
  else
    Layout();
}

void AppsGridView::OnAppListModelStatusChanged() {
  UpdatePulsingBlockViews();
  Layout();
  SchedulePaint();
}

void AppsGridView::SetViewHidden(AppListItemView* view,
                                 bool hide,
                                 bool immediate) {
  if (!view->layer())
    return;
  ui::ScopedLayerAnimationSettings animator(view->layer()->GetAnimator());
  animator.SetPreemptionStrategy(
      immediate ? ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET
                : ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  if (immediate)
    animator.SetTransitionDuration(base::TimeDelta::FromMilliseconds(0));
  view->layer()->SetOpacity(hide ? 0 : 1);
}

void AppsGridView::OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) {
}

void AppsGridView::OnBoundsAnimatorDone(views::BoundsAnimator* animator) {
  if (drag_view_)
    return;

  if (bounds_animation_for_cardified_state_in_progress_ ||
      bounds_animator_->IsAnimating()) {
    return;
  }

  items_need_layer_for_drag_ = false;
  for (const auto& entry : view_model_.entries())
    entry.view->DestroyLayer();
}

GridIndex AppsGridView::GetNearestTileIndexForPoint(
    const gfx::Point& point) const {
  gfx::Rect bounds = GetContentsBounds();
  const int current_page = pagination_model_.selected_page();
  bounds.Inset(GetTilePadding());
  const gfx::Size total_tile_size = GetTotalTileSize();
  const gfx::Vector2d grid_offset = GetGridCenteringOffset();
  int col = base::ClampToRange(
      (point.x() - bounds.x() - grid_offset.x()) / total_tile_size.width(), 0,
      cols_ - 1);
  int row = base::ClampToRange(
      (point.y() - bounds.y() - grid_offset.y()) / total_tile_size.height(), 0,
      rows_per_page_ - 1);
  return GridIndex(current_page, row * cols_ + col);
}

gfx::Rect AppsGridView::GetExpectedTileBounds(const GridIndex& index) const {
  if (!cols_)
    return gfx::Rect();

  gfx::Rect bounds(GetContentsBounds());
  bounds.Inset(GetTilePadding());
  int row = index.slot / cols_;
  int col = index.slot % cols_;
  const gfx::Size total_tile_size = GetTotalTileSize();
  gfx::Rect tile_bounds(gfx::Point(bounds.x() + col * total_tile_size.width(),
                                   bounds.y() + row * total_tile_size.height()),
                        total_tile_size);

  tile_bounds.Offset(GetGridCenteringOffset());
  tile_bounds.Inset(-GetTilePadding());
  return tile_bounds;
}

gfx::Rect AppsGridView::GetExpectedItemBoundsInFirstPage(
    const std::string& id) const {
  const AppListItem* item = model_->FindItem(id);
  if (!item)
    return gfx::Rect(GetContentsBounds().CenterPoint(), gfx::Size(1, 1));

  const int model_index = GetModelIndexOfItem(item);
  if (model_index >= view_model_.view_size())
    return gfx::Rect(GetContentsBounds().CenterPoint(), gfx::Size(1, 1));

  const GridIndex grid_index = GetIndexFromModelIndex(model_index);
  if (grid_index.page != 0)
    return gfx::Rect(GetContentsBounds().CenterPoint(), gfx::Size(1, 1));

  return GetExpectedTileBounds(grid_index);
}

AppListItemView* AppsGridView::GetViewDisplayedAtSlotOnCurrentPage(
    int slot) const {
  if (slot < 0)
    return nullptr;

  // Calculate the original bound of the tile at |index|.
  gfx::Rect tile_rect =
      GetExpectedTileBounds(GridIndex(pagination_model_.selected_page(), slot));
  tile_rect.Offset(
      CalculateTransitionOffset(pagination_model_.selected_page()));

  const auto& entries = view_model_.entries();
  const auto iter =
      std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
        return entry.view->bounds() == tile_rect && entry.view != drag_view_;
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

GridIndex AppsGridView::GetIndexFromModelIndex(int model_index) const {
  if (!folder_delegate_)
    return view_structure_.GetIndexFromModelIndex(model_index);

  const int tiles_per_page = TilesPerPage();
  return GridIndex(model_index / tiles_per_page, model_index % tiles_per_page);
}

int AppsGridView::GetModelIndexFromIndex(const GridIndex& index) const {
  if (!folder_delegate_)
    return view_structure_.GetModelIndexFromIndex(index);

  return index.page * TilesPerPage() + index.slot;
}

GridIndex AppsGridView::GetLastTargetIndex() const {
  if (!folder_delegate_)
    return view_structure_.GetLastTargetIndex();

  DCHECK_LT(0, view_model_.view_size());
  int view_index = view_model_.view_size() - 1;
  return GetIndexFromModelIndex(view_index);
}

GridIndex AppsGridView::GetLastTargetIndexOfPage(int page) const {
  if (!folder_delegate_)
    return view_structure_.GetLastTargetIndexOfPage(page);

  if (page == pagination_model_.total_pages() - 1)
    return GetLastTargetIndex();

  return GridIndex(page, TilesPerPage() - 1);
}

int AppsGridView::GetTargetModelIndexForMove(AppListItemView* moved_view,
                                             const GridIndex& index) const {
  if (!folder_delegate_)
    return view_structure_.GetTargetModelIndexForMove(moved_view, index);

  return GetModelIndexFromIndex(index);
}

GridIndex AppsGridView::GetTargetGridIndexForKeyboardMove(
    ui::KeyboardCode key_code) const {
  DCHECK(key_code == ui::VKEY_LEFT || key_code == ui::VKEY_RIGHT ||
         key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN);
  DCHECK(selected_view_);

  const GridIndex source_index = GetIndexOfView(selected_view_);
  GridIndex target_index;
  if (key_code == ui::VKEY_LEFT || key_code == ui::VKEY_RIGHT) {
    // Define backward key for traversal based on RTL.
    const ui::KeyboardCode backward =
        base::i18n::IsRTL() ? ui::VKEY_RIGHT : ui::VKEY_LEFT;

    const int target_model_index = view_model_.GetIndexOfView(selected_view_) +
                                   ((key_code == backward) ? -1 : 1);

    // A forward move on the last item in |view_model_| should result in page
    // creation.
    if (target_model_index == view_model_.view_size()) {
      // If the move is within a folder, do not allow page creation.
      if (folder_delegate_)
        return source_index;
      // If |source_index| is the last item in the grid on a page by itself,
      // moving right to a new page should be a no-op.
      if (view_structure_.items_on_page(source_index.page) == 1)
        return source_index;
      return GridIndex(pagination_model_.total_pages(), 0);
    }

    target_index = GetIndexOfView(
        static_cast<const AppListItemView*>(GetItemViewAt(std::min(
            std::max(0, target_model_index), view_model_.view_size() - 1))));
    if (!folder_delegate_ && key_code == backward &&
        target_index.page < source_index.page &&
        !view_structure_.IsFullPage(target_index.page)) {
      // Apps swap positions if the target page is the same as the
      // destination page, or the target page is full. If the page is not
      // full the app is dumped on the page. Increase the slot in this case
      // to account for the new available spot.
      ++target_index.slot;
    }
    return target_index;
  }

  // Handle the vertical move. Attempt to place the app in the same column.
  int target_page = source_index.page;
  int target_row =
      source_index.slot / cols_ + (key_code == ui::VKEY_UP ? -1 : 1);

  if (target_row < 0) {
    // The app will move to the last row of the previous page.
    --target_page;
    if (target_page < 0)
      return source_index;

    // When moving up, place the app in the last row.
    target_row = (GetItemsNumOfPage(target_page) - 1) / cols_;
  } else if (target_row > (GetItemsNumOfPage(target_page) - 1) / cols_) {
    // The app will move to the first row of the next page.
    ++target_page;
    if (folder_delegate_) {
      if (target_page >= pagination_model_.total_pages())
        return source_index;
    } else {
      if (target_page >= view_structure_.total_pages()) {
        // If |source_index| page only has one item, moving down to a new page
        // should be a no-op.
        if (view_structure_.items_on_page(source_index.page) == 1)
          return source_index;
        return GridIndex(target_page, 0);
      }
    }
    target_row = 0;
  }

  // The ideal slot shares a column with |source_index|.
  const int ideal_slot = target_row * cols_ + source_index.slot % cols_;
  if (folder_delegate_) {
    return GridIndex(target_page,
                     std::min(GetItemsNumOfPage(target_page) - 1, ideal_slot));
  }

  // If the app is being moved to a new page there is 1 extra slot available.
  const int last_slot_in_target_page =
      view_structure_.items_on_page(target_page) -
      (source_index.page != target_page ? 0 : 1);
  return GridIndex(target_page, std::min(last_slot_in_target_page, ideal_slot));
}

GridIndex AppsGridView::GetTargetGridIndexForKeyboardReparent(
    ui::KeyboardCode key_code) const {
  DCHECK(!folder_delegate_) << "Reparenting target calculations occur from the "
                               "root AppsGridView, not the folder AppsGridView";

  const GridIndex folder_index = GetIndexOfView(activated_folder_item_view_);

  // A backward move means the item will be placed previous to the folder. To do
  // this without displacing other items, place the item in the folders slot.
  // The folder will then shift forward.
  const ui::KeyboardCode backward =
      base::i18n::IsRTL() ? ui::VKEY_RIGHT : ui::VKEY_LEFT;
  if (key_code == backward)
    return folder_index;

  GridIndex target_index = GetTargetGridIndexForKeyboardMove(key_code);
  // Ensure the item is placed on the same page as the folder when possible.
  if (target_index.page < folder_index.page) {
    target_index.page = folder_index.page;
    target_index.slot = 0;
  } else if (target_index.page > folder_index.page) {
    // Prefer the last slot of the page over the next page. If the page is full
    // the item will still end up being pushed off the page.
    target_index = folder_index;
    ++target_index.slot;
  }
  return target_index;
}

void AppsGridView::HandleKeyboardMove(ui::KeyboardCode key_code) {
  DCHECK(selected_view_);
  const GridIndex target_index = GetTargetGridIndexForKeyboardMove(key_code);
  const GridIndex starting_index = GetIndexOfView(selected_view_);
  if (target_index == starting_index ||
      !IsValidReorderTargetIndex(target_index)) {
    return;
  }

  handling_keyboard_move_ = true;

  if (target_index.page == pagination_model_.total_pages())
    view_structure_.AppendPage();

  AppListItemView* original_selected_view = selected_view_;
  const GridIndex original_selected_view_index =
      GetIndexOfView(original_selected_view);
  // Moving an AppListItemView is either a swap within the origin page, a swap
  // to a full page, or a dump to a page with room. A move within a folder is
  // always a swap because there are no gaps.
  const bool swap_items =
      folder_delegate_ || view_structure_.IsFullPage(target_index.page) ||
      target_index.page == original_selected_view_index.page;

  AppListItemView* target_view = GetViewAtIndex(target_index);
  // If the move is a two part operation (swap) do not clear the overflow during
  // the initial move. Clearing the overflow when |target_index| is on a full
  // page results in the last item being pushed to the next page.
  MoveItemInModel(selected_view_, target_index, !swap_items /*clear_overflow*/);
  if (!folder_delegate_)
    view_structure_.SaveToMetadata();

  if (swap_items) {
    DCHECK(target_view);
    MoveItemInModel(target_view, original_selected_view_index);
    if (!folder_delegate_)
      view_structure_.SaveToMetadata();
  }

  int target_page = target_index.page;
  if (!folder_delegate_) {
    // Update |pagination_model_| because the move could have resulted in a
    // page getting collapsed or created.
    if (view_structure_.total_pages() != pagination_model_.total_pages()) {
      pagination_model_.SetTotalPages(view_structure_.total_pages());
    }
    // |target_page| may change due to a page collapsing.
    target_page =
        std::min(pagination_model_.total_pages() - 1, target_index.page);
  }
  pagination_model_.SelectPage(target_page, false /*animate*/);
  SetSelectedView(original_selected_view);
  Layout();
  AnnounceReorder(target_index);

  if (target_index.page != original_selected_view_index.page &&
      !folder_delegate_) {
    RecordPageSwitcherSource(kMoveAppWithKeyboard, IsTabletMode());
  }
}

size_t AppsGridView::GetTargetItemIndexForMove(AppListItemView* moved_view,
                                               const GridIndex& index) const {
  if (!folder_delegate_)
    return view_structure_.GetTargetItemIndexForMove(moved_view, index);

  // Model index is the same as item index for folder.
  return GetModelIndexFromIndex(index);
}

bool AppsGridView::IsValidIndex(const GridIndex& index) const {
  return index.page >= 0 && index.page < pagination_model_.total_pages() &&
         index.slot >= 0 && index.slot < TilesPerPage() &&
         GetModelIndexFromIndex(index) < view_model_.view_size();
}

bool AppsGridView::IsValidReorderTargetIndex(const GridIndex& index) const {
  if (!folder_delegate_)
    return view_structure_.IsValidReorderTargetIndex(index);

  return IsValidIndex(index);
}

// TODO(crbug.com/1211608): Move to PagedAppsGridView.
void AppsGridView::CalculateIdealBounds() {
  DCHECK(!folder_delegate_);

  // |view_structure_| should only be updated at the end of drag. So make a
  // copy of it and only change the copy for calculating the ideal bounds of
  // each item view.
  PagedViewStructure copied_view_structure(view_structure_);

  // Remove the item view being dragged.
  if (drag_view_) {
    copied_view_structure.Remove(drag_view_, false /* clear_overflow */,
                                 false /* clear_empty_pages */);
  }

  // Leaves a blank space in the grid for the current reorder placeholder.
  if (IsValidIndex(reorder_placeholder_)) {
    copied_view_structure.Add(nullptr, reorder_placeholder_,
                              true /* clear_overflow */,
                              false /* clear_empty_pages */);
  }

  // Convert visual index to ideal bounds.
  const auto& pages = copied_view_structure.pages();
  int model_index = 0;
  for (size_t i = 0; i < pages.size(); ++i) {
    auto& page = pages[i];
    for (size_t j = 0; j < page.size(); ++j) {
      if (page[j] == nullptr)
        continue;

      // Skip the dragged view
      if (view_model_.view_at(model_index) == drag_view_)
        ++model_index;

      gfx::Rect tile_slot = GetExpectedTileBounds(GridIndex(i, j));
      tile_slot.Offset(CalculateTransitionOffset(i));
      view_model_.set_ideal_bounds(model_index, tile_slot);
      ++model_index;
    }
  }

  // All pulsing blocks come after item views.
  GridIndex pulsing_block_index = copied_view_structure.GetLastTargetIndex();
  for (int i = 0; i < pulsing_blocks_model_.view_size(); ++i) {
    if (pulsing_block_index.slot == TilesPerPage()) {
      ++pulsing_block_index.page;
      pulsing_block_index.slot = 0;
    }
    gfx::Rect tile_slot = GetExpectedTileBounds(pulsing_block_index);
    tile_slot.Offset(CalculateTransitionOffset(pulsing_block_index.page));
    pulsing_blocks_model_.set_ideal_bounds(i, tile_slot);
    ++pulsing_block_index.slot;
  }
}

int AppsGridView::GetModelIndexOfItem(const AppListItem* item) const {
  const auto& entries = view_model_.entries();
  const auto iter =
      std::find_if(entries.begin(), entries.end(), [item](const auto& entry) {
        return static_cast<AppListItemView*>(entry.view)->item() == item;
      });
  return std::distance(entries.begin(), iter);
}

int AppsGridView::GetTargetModelIndexFromItemIndex(size_t item_index) {
  if (folder_delegate_)
    return item_index;

  CHECK(item_index <= item_list_->item_count());
  int target_model_index = 0;
  for (size_t i = 0; i < item_index; ++i) {
    if (!item_list_->item_at(i)->is_page_break())
      ++target_model_index;
  }
  return target_model_index;
}

void AppsGridView::RecordPageMetrics() {
  DCHECK(!folder_delegate_);
  UMA_HISTOGRAM_COUNTS_100("Apps.NumberOfPages",
                           pagination_model_.total_pages());

  // Calculate the number of pages that have empty slots.
  int page_count = 0;
  if (!folder_delegate_) {
    const auto& pages = view_structure_.pages();
    for (const auto& page : pages) {
      if (static_cast<int>(page.size()) < TilesPerPage())
        ++page_count;
    }
  } else {
    int item_num = view_model_.view_size();
    // Folders don't allow page breaks, so if the last page isn't full, there
    // are empty slots.
    if (item_num % TilesPerPage() > 0)
      page_count = 1;
  }
  UMA_HISTOGRAM_COUNTS_100("Apps.NumberOfPagesNotFull", page_count);
}

int AppsGridView::GetItemsNumOfPage(int page) const {
  if (page < 0 || page >= pagination_model_.total_pages())
    return 0;

  if (!folder_delegate_)
    return view_structure_.items_on_page(page);

  if (page < pagination_model_.total_pages() - 1)
    return TilesPerPage();

  return item_list_->item_count() -
         (pagination_model_.total_pages() - 1) * TilesPerPage();
}

void AppsGridView::StartFolderDroppingAnimation(
    AppListItemView* folder_item_view,
    AppListItem* drag_item,
    const gfx::Rect& source_bounds) {
  // Calculate target bounds of dragged item.
  gfx::Rect target_bounds =
      GetMirroredRect(GetTargetIconRectInFolder(drag_item, folder_item_view));

  // Update folder icon.
  AppListFolderItem* folder_item =
      static_cast<AppListFolderItem*>(folder_item_view->item());
  folder_item->NotifyOfDraggedItem(drag_item);

  // Start animation.
  auto animation_view = std::make_unique<TopIconAnimationView>(
      this, drag_item->GetIcon(GetAppListConfig().type()), std::u16string(),
      target_bounds, false, true);
  auto* animation_view_ptr =
      items_container_->AddChildView(std::move(animation_view));
  animation_view_ptr->SetBoundsRect(source_bounds);
  animation_view_ptr->AddObserver(
      new FolderDroppingAnimationObserver(model_, folder_item->id()));
  animation_view_ptr->TransformView();
}

void AppsGridView::MaybeCreateFolderDroppingAccessibilityEvent() {
  if (drop_target_region_ != ON_ITEM || !DropTargetIsValidFolder() ||
      IsFolderItem(drag_view_->item()) || folder_delegate_ ||
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
  if (drop_target_region_ == ON_ITEM && !IsFolderItem(drag_view_->item()))
    return;

  // If app was dragged out of folder, no need to announce location for the
  // now closed folder.
  if (drag_out_of_folder_container_)
    return;

  // If drop_target is not set or was already reset, then return.
  if (drop_target_ == GridIndex())
    return;

  // Don't create a11y event if |drop_target| has not changed.
  if (last_reorder_a11y_event_location_ == drop_target_)
    return;

  last_folder_dropping_a11y_event_location_ = GridIndex();
  last_reorder_a11y_event_location_ = drop_target_;

  AnnounceReorder(last_reorder_a11y_event_location_);
}

void AppsGridView::AnnounceReorder(const GridIndex& target_index) {
  const int page = target_index.page + 1;
  const int row =
      ((target_index.slot - (target_index.slot % cols_)) / cols_) + 1;
  const int col = (target_index.slot % cols_) + 1;
  a11y_announcer_->AnnounceAppsGridReorder(page, row, col);
}

void AppsGridView::CreateGhostImageView() {
  if (!app_list_features::IsAppGridGhostEnabled())
    return;
  if (!drag_view_)
    return;

  // OnReorderTimer() can trigger this function even when the
  // |reorder_placeholder_| does not change, no need to set a new GhostImageView
  // in this case.
  if (reorder_placeholder_ == current_ghost_location_)
    return;

  // When the item is dragged outside the boundaries of the app grid, if the
  // |reorder_placeholder_| moves to another page, then do not show a ghost.
  if (pagination_model_.selected_page() != reorder_placeholder_.page) {
    BeginHideCurrentGhostImageView();
    return;
  }

  BeginHideCurrentGhostImageView();
  current_ghost_location_ = reorder_placeholder_;

  if (last_ghost_view_)
    delete last_ghost_view_;

  // Preserve |current_ghost_view_| while it fades out and instantiate a new
  // GhostImageView that will fade in.
  last_ghost_view_ = current_ghost_view_;

  auto current_ghost_view = std::make_unique<GhostImageView>(
      IsFolderItem(drag_view_->item()) /* is_folder */, folder_delegate_,
      reorder_placeholder_.page);
  gfx::Rect ghost_view_bounds = GetExpectedTileBounds(reorder_placeholder_);
  ghost_view_bounds.Offset(
      CalculateTransitionOffset(reorder_placeholder_.page));
  current_ghost_view->Init(drag_view_, ghost_view_bounds);
  current_ghost_view_ =
      items_container_->AddChildView(std::move(current_ghost_view));
  current_ghost_view_->FadeIn();
}

void AppsGridView::BeginHideCurrentGhostImageView() {
  if (!app_list_features::IsAppGridGhostEnabled())
    return;

  current_ghost_location_ = GridIndex();

  if (current_ghost_view_)
    current_ghost_view_->FadeOut();
}

void AppsGridView::OnHostDragStartTimerFired() {
  gfx::Point last_drag_point_in_screen = last_drag_point_;
  views::View::ConvertPointToScreen(this, &last_drag_point_in_screen);
  if (drag_and_drop_host_->StartDrag(drag_view_->item()->id(),
                                     last_drag_point_in_screen)) {
    // From now on we forward the drag events.
    forward_events_to_drag_and_drop_host_ = true;
  }
}

BEGIN_METADATA(AppsGridView, views::View)
END_METADATA

}  // namespace ash
