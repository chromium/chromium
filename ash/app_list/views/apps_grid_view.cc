// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/apps_grid_view.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/pagination_controller.h"
#include "ash/app_list/views/app_list_drag_and_drop_host.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/expand_arrow_view.h"
#include "ash/app_list/views/indicator_chip_view.h"
#include "ash/app_list/views/pulsing_block_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_tile_item_view.h"
#include "ash/app_list/views/suggestions_container_view.h"
#include "ash/app_list/views/top_icon_animation_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view_model_utils.h"
#include "ui/views/widget/widget.h"

namespace app_list {

namespace {

// The preferred width for apps grid.
constexpr int kAppsGridPreferredWidth = 576;

// 32px page break space adjustment needed to keep 48px page break space due to
// the fact that page #02 and all the followings have bottom 56px padding.
constexpr int kPageBreakSpaceAdjustment = 32;

// Distance a drag needs to be from the app grid to be considered 'outside', at
// which point we rearrange the apps to their pre-drag configuration, as a drop
// then would be canceled. We have a buffer to make it easier to drag apps to
// other pages.
constexpr int kDragBufferPx = 20;

// Padding on each side of a tile.
constexpr int kTileHorizontalPadding = 12;
constexpr int kTileVerticalPadding = 6;

// Delay in milliseconds to do the page flip in fullscreen app list.
constexpr int kPageFlipDelayInMsFullscreen = 500;

// The drag and drop proxy should get scaled by this factor.
constexpr float kDragAndDropProxyScale = 1.2f;

// Delays in milliseconds to show re-order preview.
constexpr int kReorderDelay = 120;

// Delays in milliseconds to show folder item reparent UI.
constexpr int kFolderItemReparentDelay = 50;

// Padding between suggested apps tiles and all apps indicator.
constexpr int kSuggestionsAllAppsIndicatorPadding = 26;

// Padding between all apps indicator and all apps.
constexpr int kAllAppsIndicatorBottomPadding = 2;

// The height of gradient fade-out zones.
constexpr int kFadeoutZoneHeight = 24;

// Top padding of expand arrow.
constexpr int kExpandArrowTopPadding = 29;

// Maximum vertical and horizontal spacing between tiles.
constexpr int kMaximumTileSpacing = 96;

// Range of dragging amount fraction for suggested apps to change opacity from
// 0.f to 1.0f.
constexpr float kSuggestedAppsOpacityStartFraction = 0.3f;
constexpr float kSuggestedAppsOpacityEndFraction = 0.7f;

// Range of dragging amount fraction for all apps indicator to change opacity
// from 0.f to 1.0f.
constexpr float kAllAppsIndicatorOpacityStartFraction = 0.7f;
constexpr float kAllAppsIndicatorOpacityEndFraction = 1.0f;

// Range of dragging amount fraction for expand arrow to change opacity from
// 1.0f to 0.f.
constexpr float kExpandArrowDismissStartFraction = 0.f;
constexpr float kExpandArrowDismissEndFraction = 0.2f;

// Range of dragging amount fraction for expand arrow to change opacity from 0.f
// to 1.0f.
constexpr float kExpandArrowShowStartFraction = 0.5f;
constexpr float kExpandArrowShowEndFraction = 1.0f;

// Returns the size of a tile view excluding its padding.
gfx::Size GetTileViewSize() {
  return gfx::Size(AppListConfig::instance().grid_tile_width(),
                   AppListConfig::instance().grid_tile_height());
}

// RowMoveAnimationDelegate is used when moving an item into a different row.
// Before running the animation, the item's layer is re-created and kept in
// the original position, then the item is moved to just before its target
// position and opacity set to 0. When the animation runs, this delegate moves
// the layer and fades it out while fading in the item at the same time.
class RowMoveAnimationDelegate : public gfx::AnimationDelegate {
 public:
  RowMoveAnimationDelegate(views::View* view,
                           ui::Layer* layer,
                           const gfx::Rect& layer_target)
      : view_(view),
        layer_(layer),
        layer_start_(layer ? layer->bounds() : gfx::Rect()),
        layer_target_(layer_target) {}
  ~RowMoveAnimationDelegate() override {}

  // gfx::AnimationDelegate overrides:
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
    view_->layer()->SetOpacity(1.0f);
    view_->SchedulePaint();
  }
  void AnimationCanceled(const gfx::Animation* animation) override {
    view_->layer()->SetOpacity(1.0f);
    view_->SchedulePaint();
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
class ItemRemoveAnimationDelegate : public gfx::AnimationDelegate {
 public:
  explicit ItemRemoveAnimationDelegate(views::View* view) : view_(view) {}

  ~ItemRemoveAnimationDelegate() override {}

  // gfx::AnimationDelegate overrides:
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
class ItemMoveAnimationDelegate : public gfx::AnimationDelegate {
 public:
  explicit ItemMoveAnimationDelegate(views::View* view) : view_(view) {}

  void AnimationEnded(const gfx::Animation* animation) override {
    view_->SchedulePaint();
  }
  void AnimationCanceled(const gfx::Animation* animation) override {
    view_->SchedulePaint();
  }

 private:
  views::View* view_;

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

// Returns true if the |item| is a folder item.
bool IsFolderItem(AppListItem* item) {
  return (item->GetItemType() == AppListFolderItem::kItemType);
}

bool IsOEMFolderItem(AppListItem* item) {
  return IsFolderItem(item) &&
         (static_cast<AppListFolderItem*>(item))->folder_type() ==
             AppListFolderItem::FOLDER_TYPE_OEM;
}

int GetCompositorActivatedFrameCount(ui::Compositor* compositor) {
  return compositor ? compositor->activated_frame_count() : 0;
}

}  // namespace

// A layer delegate used for AppsGridView's mask layer, with top and bottom
// gradient fading out zones.
class AppsGridView::FadeoutLayerDelegate : public ui::LayerDelegate {
 public:
  FadeoutLayerDelegate() : layer_(ui::LAYER_TEXTURED) {
    layer_.set_delegate(this);
    layer_.SetFillsBoundsOpaquely(false);
  }

  ~FadeoutLayerDelegate() override { layer_.set_delegate(nullptr); }

  ui::Layer* layer() { return &layer_; }

 private:
  // ui::LayerDelegate overrides:
  // TODO(warx): using a mask is expensive. It would be more efficient to avoid
  // the mask for the central area and only use it for top/bottom areas.
  void OnPaintLayer(const ui::PaintContext& context) override {
    const gfx::Size size = layer()->size();
    gfx::Rect top_rect(0, 0, size.width(), kFadeoutZoneHeight);
    gfx::Rect bottom_rect(0, size.height() - kFadeoutZoneHeight, size.width(),
                          kFadeoutZoneHeight);
    ui::PaintRecorder recorder(context, size);
    gfx::Canvas* canvas = recorder.canvas();
    // Clear the canvas.
    canvas->DrawColor(SK_ColorBLACK, SkBlendMode::kSrc);
    // Draw top gradient zone.
    cc::PaintFlags flags;
    flags.setBlendMode(SkBlendMode::kSrc);
    flags.setAntiAlias(false);
    flags.setShader(gfx::CreateGradientShader(
        0, kFadeoutZoneHeight, SK_ColorTRANSPARENT, SK_ColorBLACK));
    canvas->DrawRect(top_rect, flags);
    // Draw bottom gradient zone.
    flags.setShader(gfx::CreateGradientShader(
        size.height() - kFadeoutZoneHeight, size.height(), SK_ColorBLACK,
        SK_ColorTRANSPARENT));
    canvas->DrawRect(bottom_rect, flags);
  }
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  ui::Layer layer_;

  DISALLOW_COPY_AND_ASSIGN(FadeoutLayerDelegate);
};

AppsGridView::AppsGridView(ContentsView* contents_view,
                           AppsGridViewFolderDelegate* folder_delegate)
    : folder_delegate_(folder_delegate),
      contents_view_(contents_view),
      bounds_animator_(this),
      page_flip_delay_in_ms_(kPageFlipDelayInMsFullscreen),
      pagination_animation_start_frame_number_(0),
      view_structure_(this),
      is_apps_grid_gap_feature_enabled_(
          app_list_features::IsAppsGridGapFeatureEnabled()),
      is_new_style_launcher_enabled_(
          app_list_features::IsNewStyleLauncherEnabled()) {
  DCHECK(contents_view_);
  SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  // Clip any icons that are outside the grid view's bounds. These icons would
  // otherwise be visible to the user when the grid view is off screen.
  layer()->SetMasksToBounds(true);

  // In new style launcher, suggestions container is replaced with suggestion
  // chips container, all apps indicator is removed and expand arrow is moved
  // outside this view.
  if (!is_new_style_launcher_enabled_ && !folder_delegate_) {
    // Suggestions container is replaced with suggestion chip container if new
    // style launcher is enabled.
    suggestions_container_ =
        new SuggestionsContainerView(contents_view_, &pagination_model_);
    AddChildView(suggestions_container_);
    UpdateSuggestions();

    all_apps_indicator_ = new IndicatorChipView(
        l10n_util::GetStringUTF16(IDS_ALL_APPS_INDICATOR));
    AddChildView(all_apps_indicator_);

    expand_arrow_view_ =
        new ExpandArrowView(contents_view_, contents_view_->app_list_view());
    AddChildView(expand_arrow_view_);
  }

  if (!folder_delegate && is_new_style_launcher_enabled_)
    SetBorder(views::CreateEmptyBorder(gfx::Insets(kFadeoutZoneHeight, 0)));

  pagination_model_.SetTransitionDurations(kPageTransitionDurationInMs,
                                           kOverscrollPageTransitionDurationMs);

  pagination_model_.AddObserver(this);

  pagination_controller_ = std::make_unique<PaginationController>(
      &pagination_model_, folder_delegate_
                              ? PaginationController::SCROLL_AXIS_HORIZONTAL
                              : PaginationController::SCROLL_AXIS_VERTICAL);
}

AppsGridView::~AppsGridView() {
  // Coming here |drag_view_| should already be canceled since otherwise the
  // drag would disappear after the app list got animated away and closed,
  // which would look odd.
  DCHECK(!drag_view_);
  if (drag_view_)
    EndDrag(true);

  if (model_)
    model_->RemoveObserver(this);
  pagination_model_.RemoveObserver(this);

  if (item_list_)
    item_list_->RemoveObserver(this);

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

gfx::Insets AppsGridView::GetTilePadding() const {
  if (folder_delegate_) {
    const int tile_padding_in_folder =
        AppListConfig::instance().grid_tile_spacing_in_folder() / 2;
    return gfx::Insets(-tile_padding_in_folder, -tile_padding_in_folder);
  }
  if (is_new_style_launcher_enabled_)
    return gfx::Insets(-vertical_tile_padding_, -horizontal_tile_padding_);
  return gfx::Insets(-kTileVerticalPadding, -kTileHorizontalPadding);
}

gfx::Size AppsGridView::GetTileGridSizeWithoutPadding() const {
  gfx::Size size = GetTileGridSize();
  gfx::Insets grid_padding = GetTilePadding();
  size.Enlarge(grid_padding.width(), grid_padding.height());
  return size;
}

gfx::Size AppsGridView::GetMinimumTileGridSize() const {
  DCHECK(is_new_style_launcher_enabled_);

  const gfx::Size tile_size = GetTileViewSize();
  return gfx::Size(tile_size.width() * cols_,
                   tile_size.height() * rows_per_page_);
}

gfx::Size AppsGridView::GetMaximumTileGridSize() const {
  DCHECK(is_new_style_launcher_enabled_);

  const gfx::Size tile_size = GetTileViewSize();
  return gfx::Size(
      tile_size.width() * cols_ + kMaximumTileSpacing * (cols_ - 1),
      tile_size.height() * rows_per_page_ +
          kMaximumTileSpacing * (rows_per_page_ - 1));
}

void AppsGridView::ResetForShowApps() {
  UpdateSuggestions();
  ClearDragState();
  layer()->SetOpacity(1.0f);
  SetVisible(true);
  // Set all views to visible in case they weren't made visible again by an
  // incomplete animation.
  for (int i = 0; i < view_model_.view_size(); ++i) {
    view_model_.view_at(i)->SetVisible(true);
  }

  // The number of non-page-break-items should be the same as item views.
  int item_count = 0;
  for (size_t i = 0; i < item_list_->item_count(); ++i) {
    if (!item_list_->item_at(i)->is_page_break())
      ++item_count;
  }
  CHECK_EQ(item_count, view_model_.view_size());
}

void AppsGridView::DisableFocusForShowingActiveFolder(bool disabled) {
  if (suggestions_container_) {
    for (auto* v : suggestions_container_->tile_views())
      v->SetEnabled(!disabled);
  }
  for (int i = 0; i < view_model_.view_size(); ++i) {
    view_model_.view_at(i)->SetEnabled(!disabled);
  }
}

void AppsGridView::OnTabletModeChanged(bool started) {
  // Enable/Disable folder icons's background blur based on tablet mode.
  for (int i = 0; i < view_model_.view_size(); ++i) {
    auto* item_view = view_model_.view_at(i);
    if (item_view->item()->is_folder())
      item_view->SetBackgroundBlurEnabled(started);
  }
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

void AppsGridView::SetSelectedView(AppListItemView* view) {
  if (IsSelectedView(view) || IsDraggedView(view))
    return;

  GridIndex index = GetIndexOfView(view);
  if (IsValidIndex(index))
    SetSelectedItemByIndex(index);
}

void AppsGridView::ClearSelectedView(AppListItemView* view) {
  if (view && IsSelectedView(view)) {
    selected_view_->SchedulePaint();
    selected_view_ = nullptr;
  }
}

void AppsGridView::ClearAnySelectedView() {
  if (selected_view_) {
    selected_view_->SchedulePaint();
    selected_view_ = nullptr;
  }
}

bool AppsGridView::IsSelectedView(const AppListItemView* view) const {
  return selected_view_ == view;
}

views::View* AppsGridView::GetSelectedView() const {
  if (selected_view_)
    return selected_view_;
  if (expand_arrow_view_ && expand_arrow_view_->HasFocus())
    return expand_arrow_view_;
  return nullptr;
}

void AppsGridView::InitiateDrag(AppListItemView* view,
                                Pointer pointer,
                                const gfx::Point& location,
                                const gfx::Point& root_location) {
  DCHECK(view);
  if (drag_view_ || pulsing_blocks_model_.view_size())
    return;

  drag_view_ = view;
  drag_view_init_index_ = GetIndexOfView(drag_view_);
  drag_view_offset_ = location;
  drag_start_page_ = pagination_model_.selected_page();
  reorder_placeholder_ = drag_view_init_index_;
  ExtractDragLocation(root_location, &drag_start_grid_view_);
  drag_view_start_ = gfx::Point(drag_view_->x(), drag_view_->y());
}

void AppsGridView::StartDragAndDropHostDragAfterLongPress(Pointer pointer) {
  TryStartDragAndDropHostDrag(pointer, drag_start_grid_view_);
}

void AppsGridView::TryStartDragAndDropHostDrag(
    Pointer pointer,
    const gfx::Point& grid_location) {
  drag_pointer_ = pointer;
  // Move the view to the front so that it appears on top of other views.
  ReorderChildView(drag_view_, -1);
  bounds_animator_.StopAnimatingView(drag_view_);
  // Stopping the animation may have invalidated our drag view due to the
  // view hierarchy changing.
  if (!drag_view_)
    return;

  if (!dragging_for_reparent_item_)
    StartDragAndDropHostDrag(grid_location);
}

bool AppsGridView::UpdateDragFromItem(Pointer pointer,
                                      const ui::LocatedEvent& event) {
  if (!drag_view_)
    return false;  // Drag canceled.

  gfx::Point drag_point_in_grid_view;
  ExtractDragLocation(event.root_location(), &drag_point_in_grid_view);
  UpdateDrag(pointer, drag_point_in_grid_view);
  if (!dragging())
    return false;

  // If a drag and drop host is provided, see if the drag operation needs to be
  // forwarded.
  gfx::Point location_in_screen = drag_point_in_grid_view;
  views::View::ConvertPointToScreen(this, &location_in_screen);
  DispatchDragEventToDragAndDropHost(location_in_screen);
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

  gfx::Vector2d drag_vector(point - drag_start_grid_view_);
  if (!dragging() && ExceededDragThreshold(drag_vector))
    TryStartDragAndDropHostDrag(pointer, point);

  if (drag_pointer_ != pointer)
    return;

  drag_view_->SetPosition(drag_view_start_ + drag_vector);

  last_drag_point_ = point;
  const GridIndex last_drop_target = drop_target_;
  DropTargetRegion last_drop_target_region = drop_target_region_;
  UpdateDropTargetRegion();

  MaybeStartPageFlipTimer(last_drag_point_);

  if (last_drop_target != drop_target_ ||
      last_drop_target_region != drop_target_region_) {
    if (drop_target_region_ == ON_ITEM && DraggedItemCanEnterFolder() &&
        DropTargetIsValidFolder()) {
      reorder_timer_.Stop();
      folder_dropping_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(
              AppListConfig::instance().folder_dropping_delay()),
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
  // EndDrag was called before if |drag_view_| is NULL.
  if (!drag_view_)
    return;

  // Coming here a drag and drop was in progress.
  bool landed_in_drag_and_drop_host = forward_events_to_drag_and_drop_host_;

  // This is the folder view to drop an item into. Cache the |drag_view_|'s item
  // and its bounds for later use in folder dropping animation.
  AppListItemView* folder_item_view = nullptr;
  AppListItem* drag_item = drag_view_->item();
  const gfx::Rect drag_source_bounds(drag_view_->bounds());

  if (forward_events_to_drag_and_drop_host_) {
    DCHECK(!IsDraggingForReparentInRootLevelGridView());
    forward_events_to_drag_and_drop_host_ = false;
    drag_and_drop_host_->EndDrag(cancel);
    if (IsDraggingForReparentInHiddenGridView()) {
      folder_delegate_->DispatchEndDragEventForReparent(
          true /* events_forwarded_to_drag_drop_host */,
          cancel /* cancel_drag */);
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

    if (!cancel && dragging()) {
      // Regular drag ending path, ie, not for reparenting.
      UpdateDropTargetRegion();
      if (drop_target_region_ == ON_ITEM && DraggedItemCanEnterFolder() &&
          DropTargetIsValidFolder()) {
        MoveItemToFolder(drag_view_, drop_target_);
        folder_item_view =
            GetViewDisplayedAtSlotOnCurrentPage(drop_target_.slot);
      } else if (IsValidReorderTargetIndex(drop_target_)) {
        MoveItemInModel(drag_view_, drop_target_);
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
  ClearDragState();
  UpdatePaging();
  AnimateToIdealBounds();
  if (!cancel && IsAppsGridGapEnabled())
    view_structure_.SaveToMetadata();

  if (folder_item_view) {
    // Run an animation to move dragged item to the folder.
    StartFolderDroppingAnimation(folder_item_view, drag_item,
                                 drag_source_bounds);
  }

  StopPageFlipTimer();
}

void AppsGridView::StopPageFlipTimer() {
  page_flip_timer_.Stop();
  page_flip_target_ = -1;
}

const gfx::Rect& AppsGridView::GetIdealBounds(AppListItemView* view) const {
  const int index = view_model_.GetIndexOfView(view);
  DCHECK_NE(-1, index);
  return view_model_.ideal_bounds(index);
}

AppListItemView* AppsGridView::GetItemViewAt(int index) const {
  return view_model_.view_at(index);
}

void AppsGridView::ScheduleShowHideAnimation(bool show) {
  // Stop any previous animation.
  layer()->GetAnimator()->StopAnimating();

  // Set initial state.
  SetVisible(true);
  layer()->SetOpacity(show ? 0.0f : 1.0f);

  ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
  animation.AddObserver(this);
  animation.SetTweenType(show ? kFolderFadeInTweenType
                              : kFolderFadeOutTweenType);
  animation.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
      show ? kFolderTransitionInDurationMs : kFolderTransitionOutDurationMs));

  layer()->SetOpacity(show ? 1.0f : 0.0f);
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
  AppListItemView* view =
      new AppListItemView(this, original_drag_view->item(),
                          contents_view_->GetAppListMainView()->view_delegate(),
                          false /* is_in_folder */);
  AddChildView(view);
  drag_view_ = view;
  drag_view_->SetBoundsRect(drag_view_rect);
  drag_view_->SetDragUIState();  // Hide the title of the drag_view_.

  // Hide the drag_view_ for drag icon proxy when a native drag is responsible
  // for showing the icon.
  if (has_native_drag)
    SetViewHidden(drag_view_, true /* hide */, true /* no animate */);

  // Add drag_view_ to the end of the view_model_.
  view_model_.Add(drag_view_, view_model_.view_size());
  if (IsAppsGridGapEnabled())
    view_structure_.Add(drag_view_, GetLastTargetIndex());

  drag_start_page_ = pagination_model_.selected_page();
  drag_start_grid_view_ = drag_point;

  drag_view_start_ = drag_view_->origin();

  // Set the flag in root level grid view.
  dragging_for_reparent_item_ = true;
}

void AppsGridView::UpdateDragFromReparentItem(Pointer pointer,
                                              const gfx::Point& drag_point) {
  // Note that if a cancel ocurrs while reparenting, the |drag_view_| in both
  // root and folder grid views is cleared, so the check in UpdateDragFromItem()
  // for |drag_view_| being NULL (in the folder grid) is sufficient.
  DCHECK(drag_view_);
  DCHECK(IsDraggingForReparentInRootLevelGridView());

  UpdateDrag(pointer, drag_point);
}

bool AppsGridView::IsDraggedView(const AppListItemView* view) const {
  return drag_view_ == view;
}

bool AppsGridView::IsDragViewMoved(const AppListItemView& view) const {
  return IsDraggedView(&view) && drag_view_start_ != view.origin();
}

void AppsGridView::ClearDragState() {
  drop_target_region_ = NO_TARGET;
  drag_pointer_ = NONE;
  drop_target_ = GridIndex();
  reorder_placeholder_ = GridIndex();
  drag_start_grid_view_ = gfx::Point();
  drag_start_page_ = -1;
  drag_view_offset_ = gfx::Point();

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
  return bounds_animator_.IsAnimating(view);
}

gfx::Size AppsGridView::CalculatePreferredSize() const {
  return GetTileGridSize();
}

bool AppsGridView::GetDropFormats(
    int* formats,
    std::set<ui::Clipboard::FormatType>* format_types) {
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

const char* AppsGridView::GetClassName() const {
  return "AppsGridView";
}

void AppsGridView::Layout() {
  if (bounds_animator_.IsAnimating())
    bounds_animator_.Cancel();

  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  if (fadeout_layer_delegate_)
    fadeout_layer_delegate_->layer()->SetBounds(layer()->bounds());
  rect.Inset(0, kSearchBoxBottomPadding, 0, 0);

  gfx::Rect indicator_rect(rect);
  gfx::Rect arrow_rect(rect);
  if (suggestions_container_) {
    const int tile_width = AppListConfig::instance().grid_tile_width();
    gfx::Rect suggestions_rect(rect);
    suggestions_rect.set_height(
        suggestions_container_->GetHeightForWidth(suggestions_rect.width()));
    suggestions_rect.Offset(
        (suggestions_rect.width() - tile_width) / 2 -
            (tile_width + AppListConfig::instance().grid_tile_spacing()) * 2,
        0);
    suggestions_rect.Offset(CalculateTransitionOffset(0));
    suggestions_container_->SetBoundsRect(suggestions_rect);
    indicator_rect.Inset(0,
                         suggestions_container_->GetPreferredSize().height() +
                             kSuggestionsAllAppsIndicatorPadding,
                         0, 0);
    arrow_rect.Inset(0,
                     suggestions_container_->GetPreferredSize().height() +
                         kExpandArrowTopPadding,
                     0, 0);
  }

  if (all_apps_indicator_) {
    gfx::Size indicator_size;
    indicator_size = all_apps_indicator_->GetPreferredSize();
    indicator_rect.Inset((indicator_rect.width() - indicator_size.width()) / 2,
                         0);
    indicator_rect.Offset(CalculateTransitionOffset(0));
    all_apps_indicator_->SetBoundsRect(indicator_rect);
  }

  if (expand_arrow_view_) {
    int left_right_padding =
        (arrow_rect.width() - expand_arrow_view_->GetPreferredSize().width()) /
        2;
    arrow_rect.Inset(left_right_padding, 0);
    arrow_rect.set_height(expand_arrow_view_->GetPreferredSize().height());
    expand_arrow_view_->SetBoundsRect(arrow_rect);
  }

  UpdateTilePadding();
  CalculateIdealBounds();
  for (int i = 0; i < view_model_.view_size(); ++i) {
    AppListItemView* view = GetItemViewAt(i);
    if (view != drag_view_)
      view->SetBoundsRect(view_model_.ideal_bounds(i));
  }
  views::ViewModelUtils::SetViewBoundsToIdealBounds(pulsing_blocks_model_);
}

void AppsGridView::UpdateControlVisibility(AppListViewState app_list_state,
                                           bool is_in_drag) {
  if (!folder_delegate_ && app_list_features::IsBackgroundBlurEnabled()) {
    if (is_in_drag) {
      layer()->SetMaskLayer(nullptr);
    } else {
      // TODO(newcomer): Improve implementation of the mask layer so we can
      // enable it on all devices https://crbug.com/765292.
      if (!fadeout_layer_delegate_)
        fadeout_layer_delegate_ = std::make_unique<FadeoutLayerDelegate>();
      if (!layer()->layer_mask_layer()) {
        layer()->SetMaskLayer(fadeout_layer_delegate_->layer());
        fadeout_layer_delegate_->layer()->SetBounds(layer()->bounds());
      }
    }
  }

  const bool fullscreen_apps_in_drag =
      app_list_state == AppListViewState::FULLSCREEN_ALL_APPS || is_in_drag;
  if (all_apps_indicator_)
    all_apps_indicator_->SetVisible(fullscreen_apps_in_drag);

  if (expand_arrow_view_) {
    expand_arrow_view_->SetVisible(
        app_list_state == AppListViewState::PEEKING ? true : false);
  }

  for (int i = 0; i < view_model_.view_size(); ++i) {
    AppListItemView* view = GetItemViewAt(i);
    view->SetVisible(fullscreen_apps_in_drag);
  }
}

bool AppsGridView::OnKeyPressed(const ui::KeyEvent& event) {
  // Let the FocusManager handle Left/Right keys.
  if (!CanProcessUpDownKeyTraversal(event))
    return false;

  const AppListViewState state =
      contents_view_->app_list_view()->app_list_state();
  const bool arrow_up = event.key_code() == ui::VKEY_UP;

  if (is_new_style_launcher_enabled_)
    return HandleVerticalFocusMovement(arrow_up);

  if (state == AppListViewState::PEEKING)
    return HandleFocusMovementInPeekingState(arrow_up);

  DCHECK(state == AppListViewState::FULLSCREEN_ALL_APPS);
  return HandleFocusMovementInFullscreenAllAppsState(arrow_up);
}

void AppsGridView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (!details.is_add && details.parent == this) {
    // The view being delete should not have reference in |view_model_|.
    CHECK_EQ(-1, view_model_.GetIndexOfView(details.child));

    if (selected_view_ == details.child)
      selected_view_ = nullptr;
    if (activated_folder_item_view_ == details.child)
      activated_folder_item_view_ = nullptr;

    if (drag_view_ == details.child)
      EndDrag(true);

    bounds_animator_.StopAnimatingView(details.child);
  }
}

void AppsGridView::OnGestureEvent(ui::GestureEvent* event) {
  // If a tap/long-press occurs within a valid tile, it is usually a mistake and
  // should not close the launcher in clamshell mode. Otherwise, we should let
  // those events pass to the ancestor views.
  if (!contents_view_->app_list_view()->IsHomeLauncherEnabledInTabletMode() &&
      (event->type() == ui::ET_GESTURE_TAP ||
       event->type() == ui::ET_GESTURE_LONG_PRESS)) {
    GridIndex nearest_tile_index =
        GetNearestTileIndexForPoint(event->location());
    if (IsValidIndex(nearest_tile_index))
      event->SetHandled();
    return;
  }

  // Bail on STATE_START or no apps page to make PaginationModel happy.
  if (contents_view_->GetActiveState() == ash::AppListState::kStateStart ||
      pagination_model_.total_pages() <= 0) {
    return;
  }

  // If the event is a scroll down in clamshell mode on the first page, don't
  // let |pagination_controller_| handle it. Unless it occurs in a folder.
  if (!folder_delegate_ && event->type() == ui::ET_GESTURE_SCROLL_BEGIN &&
      !contents_view_->app_list_view()->IsHomeLauncherEnabledInTabletMode() &&
      pagination_model_.selected_page() == 0 &&
      event->details().scroll_y_hint() > 0) {
    return;
  }

  // Scroll begin events should not be passed to ancestor views if it occurs
  // inside the folder bounds even it is not handled. This prevents user from
  // closing the folder when scrolling inside it.
  if (pagination_controller_->OnGestureEvent(*event, GetContentsBounds()) ||
      (folder_delegate_ && event->type() == ui::ET_GESTURE_SCROLL_BEGIN)) {
    event->SetHandled();
  }
}

void AppsGridView::Update() {
  DCHECK(!selected_view_ && !drag_view_);
  view_model_.Clear();
  if (!item_list_ || !item_list_->item_count())
    return;
  for (size_t i = 0; i < item_list_->item_count(); ++i) {
    // Skip "page break" items.
    if (item_list_->item_at(i)->is_page_break())
      continue;
    AppListItemView* view = CreateViewForItemAtIndex(i);
    view_model_.Add(view, view_model_.view_size());
    AddChildView(view);
  }
  if (IsAppsGridGapEnabled())
    view_structure_.LoadFromMetadata();
  UpdateColsAndRowsForFolder();
  UpdatePaging();
  UpdatePulsingBlockViews();
  Layout();
  SchedulePaint();

  if (!folder_delegate_)
    RecordPageMetrics();
}

void AppsGridView::UpdateSuggestions() {
  if (!suggestions_container_)
    return;
  suggestions_container_->SetResults(contents_view_->GetAppListMainView()
                                         ->view_delegate()
                                         ->GetSearchModel()
                                         ->results());
}

int AppsGridView::TilesPerPage(int page) const {
  if (folder_delegate_)
    return kMaxFolderItemsPerPage;

  return AppListConfig::instance().GetMaxNumOfItemsPerPage(page);
}

void AppsGridView::UpdatePaging() {
  if (IsAppsGridGapEnabled()) {
    pagination_model_.SetTotalPages(view_structure_.total_pages());
    return;
  }

  if (!view_model_.view_size() || !TilesPerPage(0)) {
    pagination_model_.SetTotalPages(0);
    return;
  }

  int total_pages = 0;
  if (view_model_.view_size() <= TilesPerPage(0)) {
    total_pages = 1;
  } else {
    total_pages =
        (view_model_.view_size() - TilesPerPage(0) - 1) / TilesPerPage(1) + 2;
  }

  pagination_model_.SetTotalPages(total_pages);
}

void AppsGridView::UpdatePulsingBlockViews() {
  const int existing_items = item_list_ ? item_list_->item_count() : 0;
  int current_page = pagination_model_.selected_page();
  const int available_slots =
      TilesPerPage(current_page) - existing_items % TilesPerPage(current_page);
  const int desired =
      model_->status() == ash::AppListModelStatus::kStatusSyncing
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
    PulsingBlockView* view = new PulsingBlockView(GetTotalTileSize(), true);
    pulsing_blocks_model_.Add(view, 0);
    AddChildView(view);
  }
}

AppListItemView* AppsGridView::CreateViewForItemAtIndex(size_t index) {
  // The |drag_view_| might be pending for deletion, therefore |view_model_|
  // may have one more item than |item_list_|.
  DCHECK_LE(index, item_list_->item_count());
  AppListItemView* view = new AppListItemView(
      this, item_list_->item_at(index),
      contents_view_->GetAppListMainView()->view_delegate());
  return view;
}

bool AppsGridView::HandleScroll(int offset, ui::EventType type) {
  // Bail on STATE_START or no apps page to make PaginationModel happy.
  if (contents_view_->GetActiveState() == ash::AppListState::kStateStart ||
      pagination_model_.total_pages() <= 0) {
    return false;
  }

  const gfx::Vector2dF scroll_offset_vector(0, offset);
  return pagination_controller_->OnScroll(
      gfx::ToFlooredVector2d(scroll_offset_vector), type);
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
  selected_view_->SetTitleSubpixelAA();
  selected_view_->SchedulePaint();
  selected_view_->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
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

  // Transition to previous page means negative offset.
  const int dir = transition.target_page > current_page ? -1 : 1;

  int x_offset = 0;
  int y_offset = 0;

  if (pagination_controller_->scroll_axis() ==
      PaginationController::SCROLL_AXIS_HORIZONTAL) {
    // Page size including padding pixels. A tile.x + page_width means the same
    // tile slot in the next page.
    const int page_width =
        grid_size.width() + AppListConfig::instance().page_spacing();
    if (page_of_view < current_page)
      x_offset = -page_width;
    else if (page_of_view > current_page)
      x_offset = page_width;

    if (is_valid) {
      if (page_of_view == current_page ||
          page_of_view == transition.target_page) {
        x_offset += transition.progress * page_width * dir;
      }
    }
  } else {
    const int page_height =
        grid_size.height() + +AppListConfig::instance().page_spacing();
    if (page_of_view < current_page)
      y_offset = -page_height;
    else if (page_of_view > current_page)
      y_offset = page_height;

    if (is_valid) {
      if (page_of_view == current_page ||
          page_of_view == transition.target_page) {
        y_offset += transition.progress * page_height * dir;
      }
    }

    if (!is_new_style_launcher_enabled_) {
      // Adjust pages with bottom 56px spaces to have 48px page break space, but
      // do not over adjust for ideal offset.
      if (page_of_view > current_page && current_page >= 1)
        y_offset = std::max(y_offset - kPageBreakSpaceAdjustment, 0);
      else if (page_of_view < current_page && page_of_view >= 1)
        y_offset = std::min(y_offset + kPageBreakSpaceAdjustment, 0);
    }
  }

  return gfx::Vector2d(x_offset, y_offset);
}

void AppsGridView::CalculateIdealBounds() {
  if (IsAppsGridGapEnabled()) {
    CalculateIdealBoundsWithGridGap();
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

void AppsGridView::AnimateToIdealBounds() {
  const gfx::Rect visible_bounds(GetVisibleBounds());

  CalculateIdealBounds();
  for (int i = 0; i < view_model_.view_size(); ++i) {
    AppListItemView* view = GetItemViewAt(i);
    if (view == drag_view_)
      continue;

    const gfx::Rect& target = view_model_.ideal_bounds(i);
    if (bounds_animator_.GetTargetBounds(view) == target)
      continue;

    const gfx::Rect& current = view->bounds();
    const bool current_visible = visible_bounds.Intersects(current);
    const bool target_visible = visible_bounds.Intersects(target);
    const bool visible = current_visible || target_visible;

    const int y_diff = target.y() - current.y();
    if (visible && y_diff && y_diff % GetTotalTileSize().height() == 0) {
      AnimationBetweenRows(view, current_visible, current, target_visible,
                           target);
    } else if (visible || bounds_animator_.IsAnimating(view)) {
      bounds_animator_.AnimateViewTo(view, target);
      bounds_animator_.SetAnimationDelegate(
          view, std::unique_ptr<gfx::AnimationDelegate>(
                    new ItemMoveAnimationDelegate(view)));
    } else {
      view->SetBoundsRect(target);
    }
  }
}

void AppsGridView::AnimationBetweenRows(AppListItemView* view,
                                        bool animate_current,
                                        const gfx::Rect& current,
                                        bool animate_target,
                                        const gfx::Rect& target) {
  // Determine page of |current| and |target|. -1 means in the left invisible
  // page, 0 is the center visible page and 1 means in the right invisible page.
  const int current_page =
      current.x() < 0 ? -1 : current.x() >= width() ? 1 : 0;
  const int target_page = target.x() < 0 ? -1 : target.x() >= width() ? 1 : 0;

  const int dir = current_page < target_page || (current_page == target_page &&
                                                 current.y() < target.y())
                      ? 1
                      : -1;

  std::unique_ptr<ui::Layer> layer;
  if (animate_current) {
    layer = view->RecreateLayer();
    layer->SuppressPaint();

    view->layer()->SetFillsBoundsOpaquely(false);
    view->layer()->SetOpacity(0.f);
  }

  const gfx::Size total_tile_size = GetTotalTileSize();
  gfx::Rect current_out(current);
  current_out.Offset(dir * total_tile_size.width(), 0);

  gfx::Rect target_in(target);
  if (animate_target)
    target_in.Offset(-dir * total_tile_size.width(), 0);
  view->SetBoundsRect(target_in);
  bounds_animator_.AnimateViewTo(view, target);

  bounds_animator_.SetAnimationDelegate(
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
  const size_t kMaxItemCount = kMaxFolderItemsPerPage * kMaxFolderPages;
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
      AppListConfig::instance().folder_dropping_circle_radius()) {
    return false;
  }

  return true;
}

bool AppsGridView::DraggedItemCanEnterFolder() {
  if (!IsFolderItem(drag_view_->item()) && !folder_delegate_)
    return true;
  return false;
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
  int x_offset = x_offset_direction *
                 (total_tile_size.width() / 2 -
                  AppListConfig::instance().folder_dropping_circle_radius());
  int col = (point.x() - bounds.x() + x_offset) / total_tile_size.width();
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
      (AppListConfig::instance().grid_tile_width() +
       horizontal_tile_padding_ * 2) *
      0.4;
  const int double_icon_radius =
      AppListConfig::instance().folder_dropping_circle_radius() * 2;
  const int minimum_drag_distance_for_reorder =
      std::min(forty_percent_icon_spacing, double_icon_radius);

  if (distance_to_tile_center < minimum_drag_distance_for_reorder)
    return true;
  return false;
}

void AppsGridView::OnReorderTimer() {
  reorder_placeholder_ = drop_target_;
  AnimateToIdealBounds();
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
  SetAsFolderDroppingTarget(drop_target_, true);
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
  gfx::Rect bounds_to_folder_view = ConvertRectToParent(drag_view_->bounds());
  gfx::Point pt = bounds_to_folder_view.CenterPoint();
  bool is_item_dragged_out_of_folder =
      folder_delegate_->IsPointOutsideOfFolderBoundary(pt);
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
          view_ideal_bounds, folder_item_view->GetIconImage().size());
  AppListFolderItem* folder_item =
      static_cast<AppListFolderItem*>(folder_item_view->item());
  return folder_item->GetTargetIconRectInFolderForItem(drag_item,
                                                       icon_ideal_bounds);
}

bool AppsGridView::IsUnderOEMFolder() {
  if (!folder_delegate_)
    return false;

  return folder_delegate_->IsOEMFolder();
}

bool AppsGridView::HandleFocusMovementInPeekingState(bool arrow_up) {
  if (!expand_arrow_view_)
    return false;

  if (expand_arrow_view_->HasFocus())
    return false;
  // In peeking mode, the focus is now in suggestions container.
  if (arrow_up)
    contents_view_->GetSearchBoxView()->search_box()->RequestFocus();
  else
    expand_arrow_view_->RequestFocus();
  return true;
}

// TODO(http://crbug.com/859644): fix focus in new style launcher.
bool AppsGridView::HandleFocusMovementInFullscreenAllAppsState(bool arrow_up) {
  // The global index is the index of focused app in all pages, assuming
  // all pages except last one have |cols_*rows_per_page_| apps, including
  // apps in both |view_model_| and |suggestions_container_|. For example, if
  // the focused app is the n-th app in p-th page, the global index is
  // |(p-1)*(cols_*rows_per_page_)+(n-1)|.
  int global_index;

  // |suggestions_container_| does not exist in folder view.
  bool has_suggestions_app =
      suggestions_container_ && suggestions_container_->num_results() > 0;
  const int suggestions_app_max_num = has_suggestions_app ? cols_ : 0;

  // Calculate current global focus index.
  views::View* focused_view = GetFocusManager()->GetFocusedView();
  if (has_suggestions_app && suggestions_container_->Contains(focused_view)) {
    const std::vector<SearchResultTileItemView*>& tile_views =
        suggestions_container_->tile_views();
    global_index =
        std::find(tile_views.begin(), tile_views.end(), focused_view) -
        tile_views.begin();
    DCHECK(global_index < cols_);
  } else {
    global_index = view_model_.GetIndexOfView(
                       static_cast<const AppListItemView*>(focused_view)) +
                   suggestions_app_max_num;
  }

  // Calculate target global focus index.
  const int tile_total = view_model_.view_size() + suggestions_app_max_num;
  const int row_total = tile_total / cols_ + ((tile_total % cols_) ? 1 : 0);
  int target_global_index = global_index + (arrow_up ? -cols_ : cols_);
  if (target_global_index >= tile_total &&
      target_global_index < cols_ * row_total) {
    // Target index is on last row, so set it to last app's global index.
    target_global_index = tile_total - 1;
  } else if (has_suggestions_app && target_global_index >= 0 &&
             target_global_index < cols_) {
    // Target index is in |suggestions_container|, set it to last suggestion
    // app's index if it is outside range.
    target_global_index = std::min(suggestions_container_->num_results() - 1,
                                   target_global_index);
  }

  // Move focus based on target global focus index.
  if (folder_delegate_ &&
      (target_global_index < 0 || target_global_index >= cols_ * row_total ||
       target_global_index / kMaxFolderItemsPerPage !=
           global_index / kMaxFolderItemsPerPage)) {
    // The pagination inside a folder is set horizontally, so focus should be
    // set on the search box when it is moved up outside the current page and
    // should be set on the folder title when it is moved down outside the
    // current page.
    if (arrow_up) {
      contents_view_->GetSearchBoxView()->search_box()->RequestFocus();
    } else {
      contents_view_->GetAppsContainerView()
          ->app_list_folder_view()
          ->folder_header_view()
          ->SetTextFocus();
    }
    return true;
  }

  if (target_global_index < 0 || target_global_index >= cols_ * row_total) {
    // Target index is outside apps grid view.
    contents_view_->GetSearchBoxView()->search_box()->RequestFocus();
  } else if (has_suggestions_app &&
             target_global_index < suggestions_container_->num_results()) {
    suggestions_container_->tile_views()[target_global_index]->RequestFocus();
  } else {
    view_model_.view_at(target_global_index - suggestions_app_max_num)
        ->RequestFocus();
  }
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
    // Move focus to the last row of previous page if target row is negative.
    --target_page;

    // |target_page| may be invalid which makes |target_row| invalid, but
    // |target_row| will not be used if |target_page| is invalid.
    target_row = (GetItemsNumOfPage(target_page) - 1) / cols_;
  } else if (target_row > (GetItemsNumOfPage(target_page) - 1) / cols_) {
    // Move focus to the first row of next page if target row is beyond range.
    ++target_page;
    target_row = 0;
  }

  if (target_page < 0) {
    // Move focus up outside the apps grid if target page is negative.
    views::View* v = GetFocusManager()->GetNextFocusableView(
        view_model_.view_at(0), nullptr, true, false);
    DCHECK(v);
    v->RequestFocus();
    return true;
  }

  if (target_page >= pagination_model_.total_pages()) {
    // Move focus down outside the apps grid if target page is beyond range.
    views::View* v = GetFocusManager()->GetNextFocusableView(
        view_model_.view_at(view_model_.view_size() - 1), nullptr, false,
        false);
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

  // Try to shape the apps grid into a square.
  int items_in_one_page =
      std::min(kMaxFolderItemsPerPage, item_list_->item_count());
  cols_ = std::sqrt(items_in_one_page - 1) + 1;
  rows_per_page_ = (items_in_one_page - 1) / cols_ + 1;
}

size_t AppsGridView::GetAppListItemViewIndexOffset() const {
  if (is_new_style_launcher_enabled_ || folder_delegate_)
    return 0;

  // The first app list item view must be right behind the expand arrow view.
  return GetIndexOf(expand_arrow_view_) + 1;
}

void AppsGridView::DispatchDragEventForReparent(Pointer pointer,
                                                const gfx::Point& drag_point) {
  folder_delegate_->DispatchDragEventForReparent(pointer, drag_point);
}

void AppsGridView::EndDragFromReparentItemInRootLevel(
    bool events_forwarded_to_drag_drop_host,
    bool cancel_drag) {
  // EndDrag was called before if |drag_view_| is NULL.
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
      if (!cancel_reparent) {
        folder_item_view =
            GetViewDisplayedAtSlotOnCurrentPage(drop_target_.slot);
      }
    } else if (drop_target_region_ != NO_TARGET &&
               IsValidReorderTargetIndex(drop_target_)) {
      ReparentItemForReorder(drag_view_, drop_target_);
    } else {
      NOTREACHED();
    }
    SetViewHidden(drag_view_, false /* show */, true /* no animate */);
  }

  SetAsFolderDroppingTarget(drop_target_, false);
  if (!cancel_reparent) {
    // By setting |drag_view_| to NULL here, we prevent ClearDragState() from
    // cleaning up the newly created AppListItemView, effectively claiming
    // ownership of the newly created drag view.
    drag_view_->OnDragEnded();
    drag_view_ = nullptr;
  }
  ClearDragState();
  AnimateToIdealBounds();
  if (IsAppsGridGapEnabled())
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

  StopPageFlipTimer();
}

void AppsGridView::EndDragForReparentInHiddenFolderGridView() {
  if (drag_and_drop_host_) {
    // If we had a drag and drop proxy icon, we delete it and make the real
    // item visible again.
    drag_and_drop_host_->DestroyDragIconProxy();
  }

  SetAsFolderDroppingTarget(drop_target_, false);
  ClearDragState();
}

void AppsGridView::OnFolderItemRemoved() {
  DCHECK(folder_delegate_);
  if (item_list_)
    item_list_->RemoveObserver(this);
  item_list_ = nullptr;
}

void AppsGridView::UpdateOpacity() {
  AppListView* app_list_view = contents_view_->app_list_view();
  const int current_height = app_list_view->GetCurrentAppListHeight();
  const int peeking_height =
      AppListConfig::instance().peeking_app_list_height();
  bool should_restore_opacity =
      !app_list_view->is_in_drag() &&
      (app_list_view->app_list_state() != AppListViewState::CLOSED);

  // The opacity of suggested apps is a function of the fractional displacement
  // of the app list from collapsed(0) to peeking(1) state. When the fraction
  // changes from |kSuggestedAppsOpacityStartFraction| to
  // |kSuggestedAppsOpacityEndFraction|, the opacity of suggested apps changes
  // from 0.f to 1.0f.
  const int shelf_height = AppListConfig::instance().shelf_height();
  float fraction = std::max<float>(current_height - shelf_height, 0) /
                   (peeking_height - shelf_height);
  float opacity =
      std::min(std::max((fraction - kSuggestedAppsOpacityStartFraction) /
                            (kSuggestedAppsOpacityEndFraction -
                             kSuggestedAppsOpacityStartFraction),
                        0.f),
               1.0f);
  if (suggestions_container_) {
    suggestions_container_->layer()->SetOpacity(
        should_restore_opacity ? 1.0f : opacity);
  }

  // The opacity of expand arrow during dragging from collapsed(0) to peeking(1)
  // state. When the dragging amount fraction changes from
  // |kExpandArrowShowStartFraction| to |kExpandArrowShowEndFraction|, the
  // opacity changes from 0.f to 1.0f.
  float arrow_peeking_opacity = std::min(
      std::max(
          (fraction + kExpandArrowShowEndFraction -
           kExpandArrowShowStartFraction - 1.0f) /
              (kExpandArrowShowEndFraction - kExpandArrowShowStartFraction),
          0.f),
      1.0f);

  // The opacity of all apps indicator is a function of the fractional
  // displacement of the app list from peeking(0) to fullscreen(1) state. When
  // the fraction changes from |kAllAppsIndicatorOpacityStartFraction| to
  // |kAllAppsIndicatorOpacityEndFraction|, the opacity of all apps indicator
  // changes from 0.f to 1.0f.
  const float peeking_to_fullscreen_height =
      app_list_view->GetFullscreenStateHeight() - peeking_height;
  DCHECK_GT(peeking_to_fullscreen_height, 0);
  const float drag_amount = current_height - peeking_height;
  fraction = std::max(drag_amount / peeking_to_fullscreen_height, 0.f);
  opacity = std::min(std::max((fraction + kAllAppsIndicatorOpacityEndFraction -
                               kAllAppsIndicatorOpacityStartFraction - 1.0f) /
                                  (kAllAppsIndicatorOpacityEndFraction -
                                   kAllAppsIndicatorOpacityStartFraction),
                              0.f),
                     1.0f);
  if (all_apps_indicator_) {
    all_apps_indicator_->layer()->SetOpacity(should_restore_opacity ? 1.0f
                                                                    : opacity);
  }

  // The opacity of expand arrow during dragging from peeking(0) to
  // fullscreen(1) state. When the dragging amount fraction changes from
  // |kExpandArrowDismissStartFraction| to |kExpandArrowDismissEndFraction|, the
  // opacity changes from 1.0f to 0.f;
  float arrow_fullscreen_opacity =
      std::min(std::max((fraction - kExpandArrowDismissEndFraction) /
                            (kExpandArrowDismissStartFraction -
                             kExpandArrowDismissEndFraction),
                        0.f),
               1.0f);
  if (expand_arrow_view_) {
    if (peeking_height < current_height) {
      expand_arrow_view_->layer()->SetOpacity(
          should_restore_opacity ? 1.0f : arrow_fullscreen_opacity);
    } else {
      expand_arrow_view_->layer()->SetOpacity(
          should_restore_opacity ? 1.0f : arrow_peeking_opacity);
    }
  }

  if (view_structure_.pages().empty())
    return;

  // Updates the opacity of the apps in current page. The opacity of the app
  // starting at 0.f when the ceterline of the app is |kAllAppsOpacityStartPx|
  // above the bottom of work area and transitioning to 1.0f by the time the
  // centerline reaches |kAllAppsOpacityEndPx| above the work area bottom.
  const int selected_page = pagination_model_.selected_page();
  auto current_page = view_structure_.pages()[selected_page];
  float centerline_above_work_area = 0.f;
  for (size_t i = 0; i < current_page.size(); i += cols_) {
    AppListItemView* item_view = current_page[i];
    gfx::Rect view_bounds = item_view->bounds();
    views::View::ConvertRectToScreen(this, &view_bounds);
    centerline_above_work_area = std::max<float>(
        app_list_view->GetScreenBottom() - view_bounds.CenterPoint().y(), 0.f);
    opacity = std::min(
        std::max((centerline_above_work_area - kAllAppsOpacityStartPx) /
                     (kAllAppsOpacityEndPx - kAllAppsOpacityStartPx),
                 0.f),
        1.0f);
    opacity = should_restore_opacity ? 1.0f : opacity;

    if (opacity == item_view->layer()->opacity())
      continue;

    const size_t end_index = std::min(current_page.size() - 1, i + cols_ - 1);
    for (size_t j = i; j <= end_index; ++j) {
      if (current_page[j] != drag_view_)
        current_page[j]->layer()->SetOpacity(opacity);
    }
  }
}

bool AppsGridView::HandleScrollFromAppListView(int offset, ui::EventType type) {
  // Scroll up at first page in top level apps grid should close the launcher.
  if (!folder_delegate_ && offset > 0 &&
      !pagination_model()->IsValidPageRelative(-1)) {
    return false;
  }

  HandleScroll(offset, type);
  return true;
}

AppListItemView* AppsGridView::GetCurrentPageFirstItemViewInFolder() {
  DCHECK(folder_delegate_);
  int first_index = pagination_model_.selected_page() * kMaxFolderItemsPerPage;
  return view_model_.view_at(first_index);
}

AppListItemView* AppsGridView::GetCurrentPageLastItemViewInFolder() {
  DCHECK(folder_delegate_);
  int last_index = std::min(
      (pagination_model_.selected_page() + 1) * kMaxFolderItemsPerPage - 1,
      item_list_->item_count() - 1);
  return view_model_.view_at(last_index);
}

bool AppsGridView::IsTabletMode() const {
  return contents_view_->app_list_view()->is_tablet_mode();
}

void AppsGridView::StartDragAndDropHostDrag(const gfx::Point& grid_location) {
  // When a drag and drop host is given, the item can be dragged out of the app
  // list window. In that case a proxy widget needs to be used.
  // Note: This code has very likely to be changed for Windows (non metro mode)
  // when a |drag_and_drop_host_| gets implemented.
  if (!drag_view_ || !drag_and_drop_host_)
    return;

  gfx::Point screen_location = grid_location;
  views::View::ConvertPointToScreen(this, &screen_location);

  // Determine the mouse offset to the center of the icon so that the drag and
  // drop host follows this layer.
  gfx::Vector2d delta =
      drag_view_offset_ - drag_view_->GetLocalBounds().CenterPoint();
  delta.set_y(delta.y() + drag_view_->title()->size().height() / 2);

  // We have to hide the original item since the drag and drop host will do
  // the OS dependent code to "lift off the dragged item". Apply the scale
  // factor of this view's transform to the dragged view as well.
  DCHECK(!IsDraggingForReparentInRootLevelGridView());
  drag_and_drop_host_->CreateDragIconProxyByLocationWithNoAnimation(
      drag_view_->GetIconBoundsInScreen().origin(), drag_view_->GetIconImage(),
      drag_view_, kDragAndDropProxyScale * GetTransform().Scale2d().x(),
      is_new_style_launcher_enabled_ && drag_view_->item()->is_folder() &&
              IsTabletMode()
          ? AppListConfig::instance().blur_radius()
          : 0);

  SetViewHidden(drag_view_, true /* hide */, true /* no animation */);
}

void AppsGridView::DispatchDragEventToDragAndDropHost(
    const gfx::Point& location_in_screen_coordinates) {
  if (!drag_view_ || !drag_and_drop_host_)
    return;

  if (GetLocalBounds().Contains(last_drag_point_)) {
    // The event was issued inside the app menu and we should get all events.
    if (forward_events_to_drag_and_drop_host_) {
      // The DnD host was previously called and needs to be informed that the
      // session returns to the owner.
      forward_events_to_drag_and_drop_host_ = false;
      drag_and_drop_host_->EndDrag(true);
    }
  } else {
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
    } else {
      if (drag_and_drop_host_->StartDrag(drag_view_->item()->id(),
                                         location_in_screen_coordinates)) {
        // From now on we forward the drag events.
        forward_events_to_drag_and_drop_host_ = true;
        // Any flip operations are stopped.
        StopPageFlipTimer();
      }
    }
  }
}

void AppsGridView::MaybeStartPageFlipTimer(const gfx::Point& drag_point) {
  if (!IsPointWithinPageFlipBuffer(drag_point))
    StopPageFlipTimer();
  int new_page_flip_target = -1;

  // Drag zones are at the edges of the scroll axis.
  if (pagination_controller_->scroll_axis() ==
      PaginationController::SCROLL_AXIS_VERTICAL) {
    if (drag_point.y() < AppListConfig::instance().page_flip_zone_size()) {
      new_page_flip_target = pagination_model_.selected_page() - 1;
    } else if (IsPointWithinBottomDragBuffer(drag_point)) {
      // If the drag point is within the drag buffer, but not over the shelf.
      new_page_flip_target = pagination_model_.selected_page() + 1;
    }
  } else {
    // TODO(xiyuan): Fix this for RTL.
    if (new_page_flip_target == -1 &&
        drag_point.x() < AppListConfig::instance().page_flip_zone_size())
      new_page_flip_target = pagination_model_.selected_page() - 1;

    if (new_page_flip_target == -1 &&
        drag_point.x() >
            width() - AppListConfig::instance().page_flip_zone_size()) {
      new_page_flip_target = pagination_model_.selected_page() + 1;
    }
  }

  if (new_page_flip_target == page_flip_target_)
    return;

  StopPageFlipTimer();
  if (IsValidPageFlipTarget(new_page_flip_target)) {
    page_flip_target_ = new_page_flip_target;

    if (page_flip_target_ != pagination_model_.selected_page()) {
      page_flip_timer_.Start(
          FROM_HERE, base::TimeDelta::FromMilliseconds(page_flip_delay_in_ms_),
          this, &AppsGridView::OnPageFlipTimer);
    }
  }
}

void AppsGridView::OnPageFlipTimer() {
  DCHECK(IsValidPageFlipTarget(page_flip_target_));

  if (pagination_model_.total_pages() == page_flip_target_) {
    // Create a new page because the user requests to put an item to a new page.
    extra_page_opened_ = true;
    pagination_model_.SetTotalPages(pagination_model_.total_pages() + 1);
  }

  pagination_model_.SelectPage(page_flip_target_, true);
  UMA_HISTOGRAM_ENUMERATION(kAppListPageSwitcherSourceHistogram,
                            kDragAppToBorder, kMaxAppListPageSwitcherSource);
}

void AppsGridView::MoveItemInModel(AppListItemView* item_view,
                                   const GridIndex& target) {
  int current_model_index = view_model_.GetIndexOfView(item_view);
  size_t current_item_index;
  item_list_->FindItemIndex(item_view->item()->id(), &current_item_index);
  DCHECK_GE(current_model_index, 0);

  int target_model_index = GetTargetModelIndexForMove(item_view, target);
  size_t target_item_index = GetTargetItemIndexForMove(item_view, target);

  // The same item index does not guarantee the same visual index, so move the
  // item visual index here.
  if (IsAppsGridGapEnabled())
    view_structure_.Move(item_view, target);

  // Reorder the app list item views in accordance with |view_model_|.
  ReorderChildView(item_view,
                   GetAppListItemViewIndexOffset() + target_model_index);

  if (target_item_index == current_item_index)
    return;

  item_list_->RemoveObserver(this);
  item_list_->MoveItem(current_item_index, target_item_index);
  view_model_.Move(current_model_index, target_model_index);
  item_list_->AddObserver(this);

  if (pagination_model_.selected_page() != target.page)
    pagination_model_.SelectPage(target.page, false);

  RecordAppMovingTypeMetrics(folder_delegate_ ? kReorderInFolder
                                              : kReorderInTopLevel);
}

void AppsGridView::MoveItemToFolder(AppListItemView* item_view,
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
    return;
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
      AppListItemView* target_folder_view =
          CreateViewForItemAtIndex(folder_item_index);
      target_folder_view->SetBoundsRect(target_view_bounds);
      view_model_.Add(target_folder_view, target_model_index);
      if (IsAppsGridGapEnabled())
        view_structure_.Add(target_folder_view, target_index);

      // If drag view is in front of the position where it will be moved to, we
      // should skip it.
      int offset = (drag_view_ &&
                    view_model_.GetIndexOfView(drag_view_) < target_model_index)
                       ? 1
                       : 0;
      AddChildViewAt(target_folder_view, GetAppListItemViewIndexOffset() +
                                             target_model_index - offset);
    } else {
      LOG(ERROR) << "Folder no longer in item_list: " << folder_item_id;
    }
  }

  // Fade out the drag_view_ and delete it when animation ends.
  int drag_model_index = view_model_.GetIndexOfView(drag_view_);
  view_model_.Remove(drag_model_index);
  if (IsAppsGridGapEnabled())
    view_structure_.Remove(drag_view_);
  bounds_animator_.AnimateViewTo(drag_view_, drag_view_->bounds());
  bounds_animator_.SetAnimationDelegate(
      drag_view_, std::unique_ptr<gfx::AnimationDelegate>(
                      new ItemRemoveAnimationDelegate(drag_view_)));

  RecordAppMovingTypeMetrics(kMoveIntoFolder);
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
    if (IsAppsGridGapEnabled() &&
        target.page == deleted_folder_grid_index.page &&
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
  if (IsAppsGridGapEnabled())
    view_structure_.Move(item_view, target_override);
  ReorderChildView(item_view,
                   GetAppListItemViewIndexOffset() + target_model_index);

  RemoveLastItemFromReparentItemFolderIfNecessary(source_folder_id);

  item_list_->AddObserver(this);
  model_->AddObserver(this);
  UpdatePaging();

  RecordAppMovingTypeMetrics(kMoveOutOfFolder);
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
      AppListItemView* new_folder_view =
          CreateViewForItemAtIndex(new_folder_index);
      new_folder_view->SetBoundsRect(target_rect);
      view_model_.Add(new_folder_view, target_model_index);
      if (IsAppsGridGapEnabled())
        view_structure_.Add(new_folder_view, target_index);
      AddChildViewAt(new_folder_view,
                     GetAppListItemViewIndexOffset() + target_model_index);
    } else {
      LOG(ERROR) << "Folder no longer in item_list: " << new_folder_id;
    }
  }

  RemoveLastItemFromReparentItemFolderIfNecessary(source_folder_id);

  item_list_->AddObserver(this);

  // Fade out the drag_view_ and delete it when animation ends.
  int drag_model_index = view_model_.GetIndexOfView(drag_view_);
  view_model_.Remove(drag_model_index);
  if (IsAppsGridGapEnabled())
    view_structure_.Remove(drag_view_);
  bounds_animator_.AnimateViewTo(drag_view_, drag_view_->bounds());
  bounds_animator_.SetAnimationDelegate(
      drag_view_, std::unique_ptr<gfx::AnimationDelegate>(
                      new ItemRemoveAnimationDelegate(drag_view_)));
  UpdatePaging();

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
  if (!source_folder || source_folder->ChildItemCount() != 1u)
    return;

  // Save the folder item view's bounds before deletion, which will be used as
  // last item view's bounds.
  gfx::Rect folder_rect = activated_folder_item_view()->bounds();
  GridIndex target_index = GetIndexOfView(activated_folder_item_view());
  int target_model_index =
      view_model_.GetIndexOfView(activated_folder_item_view());

  // Delete view associated with the folder item to be removed.
  DeleteItemViewAtIndex(
      view_model_.GetIndexOfView(activated_folder_item_view()),
      false /* sanitize */);

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
  AppListItemView* last_item_view = CreateViewForItemAtIndex(last_item_index);
  last_item_view->SetBoundsRect(folder_rect);
  view_model_.Add(last_item_view, target_model_index);
  if (IsAppsGridGapEnabled())
    view_structure_.Add(last_item_view, target_index);
  AddChildViewAt(last_item_view,
                 GetAppListItemViewIndexOffset() + target_model_index);
}

void AppsGridView::CancelContextMenusOnCurrentPage() {
  GridIndex start_index(pagination_model_.selected_page(), 0);
  int start = GetModelIndexFromIndex(start_index);
  int end =
      std::min(view_model_.view_size(), start + TilesPerPage(start_index.page));
  for (int i = start; i < end; ++i)
    GetItemViewAt(i)->CancelContextMenu();
}

void AppsGridView::DeleteItemViewAtIndex(int index, bool sanitize) {
  AppListItemView* item_view = GetItemViewAt(index);
  view_model_.Remove(index);
  if (IsAppsGridGapEnabled()) {
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

bool AppsGridView::IsPointWithinPageFlipBuffer(const gfx::Point& point) const {
  gfx::Point point_in_screen = point;
  ConvertPointToScreen(this, &point_in_screen);
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestView(
          GetWidget()->GetNativeView());
  return display.work_area().Contains(point_in_screen);
}

bool AppsGridView::IsPointWithinBottomDragBuffer(
    const gfx::Point& point) const {
  gfx::Point point_in_screen = point;
  ConvertPointToScreen(this, &point_in_screen);
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestView(
          GetWidget()->GetNativeView());

  const int kBottomDragBufferMin =
      GetBoundsInScreen().bottom() -
      (AppListConfig::instance().page_flip_zone_size());
  const int kBottomDragBufferMax =
      display.bounds().bottom() -
      (contents_view_->app_list_view()->is_side_shelf()
           ? 0
           : (display.bounds().bottom() - display.work_area().bottom()));

  return point_in_screen.y() > kBottomDragBufferMin &&
         point_in_screen.y() < kBottomDragBufferMax;
}

void AppsGridView::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  if (dragging())
    return;

  if (strcmp(sender->GetClassName(), AppListItemView::kViewClassName))
    return;

  // Always set the previous activated_folder_item_view_ to be visible. This
  // prevents a case where the item would remain hidden due the
  // |activated_folder_item_view_| changing during the animation. We only
  // need to track |activated_folder_item_view_| in the root level grid view.
  AppListItemView* pressed_item_view = static_cast<AppListItemView*>(sender);
  if (!folder_delegate_) {
    if (activated_folder_item_view_)
      activated_folder_item_view_->SetVisible(true);
    if (IsFolderItem(pressed_item_view->item()))
      activated_folder_item_view_ = pressed_item_view;
    else
      activated_folder_item_view_ = nullptr;
  }
  contents_view_->GetAppListMainView()->ActivateApp(pressed_item_view->item(),
                                                    event.flags());
}

void AppsGridView::OnListItemAdded(size_t index, AppListItem* item) {
  EndDrag(true);

  if (!item->is_page_break()) {
    AppListItemView* view = CreateViewForItemAtIndex(index);
    int model_index = GetTargetModelIndexFromItemIndex(index);
    view_model_.Add(view, model_index);
    AddChildViewAt(view, GetAppListItemViewIndexOffset() + model_index);

    // Ensure that AppListItems that are added to the AppListItemList are not
    // shown while in PEEKING. The visibility of the app icons will be updated
    // on drag/animation from PEEKING.
    view->SetVisible(model_->state_fullscreen() != AppListViewState::PEEKING);
  }

  if (IsAppsGridGapEnabled())
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

  if (IsAppsGridGapEnabled())
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
    ReorderChildView(view_model_.view_at(to_model_index),
                     GetAppListItemViewIndexOffset() + to_model_index);
  }

  if (IsAppsGridGapEnabled())
    view_structure_.LoadFromMetadata();
  UpdateColsAndRowsForFolder();
  UpdatePaging();
  AnimateToIdealBounds();
}

void AppsGridView::OnAppListItemHighlight(size_t index, bool highlight) {
  int model_index = GetModelIndexOfItem(item_list_->item_at(index));
  AppListItemView* view = GetItemViewAt(model_index);
  view->SetItemIsHighlighted(highlight);
  if (highlight)
    EnsureViewVisible(GetIndexFromModelIndex(model_index));
}

void AppsGridView::TotalPagesChanged() {}

void AppsGridView::SelectedPageChanged(int old_selected, int new_selected) {
  if (dragging()) {
    UpdateDropTargetRegion();
    Layout();
    MaybeStartPageFlipTimer(last_drag_point_);
  } else {
    ClearSelectedView(selected_view_);
    Layout();
  }
}

void AppsGridView::TransitionStarted() {
  CancelContextMenusOnCurrentPage();
  pagination_animation_start_frame_number_ =
      GetCompositorActivatedFrameCount(layer()->GetCompositor());
}

void AppsGridView::TransitionChanged() {
  // Update layout for valid page transition only since over-scroll no longer
  // animates app icons.
  const PaginationModel::Transition& transition =
      pagination_model_.transition();
  if (pagination_model_.is_valid_page(transition.target_page))
    Layout();
}

void AppsGridView::TransitionEnded() {
  const base::TimeDelta duration =
      pagination_model_.GetTransitionAnimationSlideDuration();

  ui::Compositor* compositor = layer()->GetCompositor();
  // Do not record animation smoothness if |compositor| is nullptr.
  if (!compositor)
    return;

  const int end_frame_number = GetCompositorActivatedFrameCount(compositor);
  if (end_frame_number > pagination_animation_start_frame_number_ &&
      !duration.is_zero()) {
    RecordPaginationAnimationSmoothness(
        end_frame_number - pagination_animation_start_frame_number_,
        duration.InMilliseconds(), compositor->refresh_rate());
  }
}

void AppsGridView::OnAppListModelStatusChanged() {
  UpdatePulsingBlockViews();
  Layout();
  SchedulePaint();
}

void AppsGridView::SetViewHidden(AppListItemView* view,
                                 bool hide,
                                 bool immediate) {
  ui::ScopedLayerAnimationSettings animator(view->layer()->GetAnimator());
  animator.SetPreemptionStrategy(
      immediate ? ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET
                : ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  view->layer()->SetOpacity(hide ? 0 : 1);
}

void AppsGridView::OnImplicitAnimationsCompleted() {
  if (layer()->opacity() == 0.0f)
    SetVisible(false);
}

GridIndex AppsGridView::GetNearestTileIndexForPoint(
    const gfx::Point& point) const {
  gfx::Rect bounds = GetContentsBounds();
  const int current_page = pagination_model_.selected_page();
  bounds.Inset(0, GetHeightOnTopOfAllAppsTiles(current_page), 0, 0);
  bounds.Inset(GetTilePadding());
  const gfx::Size total_tile_size = GetTotalTileSize();
  int col = base::ClampToRange(
      (point.x() - bounds.x()) / total_tile_size.width(), 0, cols_ - 1);

  const bool show_suggested_apps =
      !is_new_style_launcher_enabled_ && current_page == 0 && !folder_delegate_;
  int row =
      base::ClampToRange((point.y() - bounds.y()) / total_tile_size.height(), 0,
                         rows_per_page_ - (show_suggested_apps ? 2 : 1));
  return GridIndex(current_page, row * cols_ + col);
}

gfx::Size AppsGridView::GetTileGridSize() const {
  if (!is_new_style_launcher_enabled_ && !folder_delegate_)
    return gfx::Size(kAppsGridPreferredWidth, kHorizontalPagePreferredHeight);

  gfx::Rect rect(GetTotalTileSize());
  rect.set_size(
      gfx::Size(rect.width() * cols_, rect.height() * rows_per_page_));
  rect.Inset(-GetTilePadding());
  return rect.size();
}

int AppsGridView::GetHeightOnTopOfAllAppsTiles(int page) const {
  if (is_new_style_launcher_enabled_ || folder_delegate_)
    return 0;

  if (page == 0) {
    const int suggestions_container_height =
        suggestions_container_
            ? suggestions_container_->GetPreferredSize().height()
            : 0;
    DCHECK(all_apps_indicator_);
    return kSearchBoxBottomPadding + suggestions_container_height +
           kSuggestionsAllAppsIndicatorPadding +
           all_apps_indicator_->GetPreferredSize().height() +
           kAllAppsIndicatorBottomPadding;
  }
  return kSearchBoxBottomPadding;
}

gfx::Rect AppsGridView::GetExpectedTileBounds(const GridIndex& index) const {
  if (!cols_)
    return gfx::Rect();

  gfx::Rect bounds(GetContentsBounds());
  bounds.Inset(0, GetHeightOnTopOfAllAppsTiles(index.page), 0, 0);
  bounds.Inset(GetTilePadding());
  int row = index.slot / cols_;
  int col = index.slot % cols_;
  const gfx::Size total_tile_size = GetTotalTileSize();
  gfx::Rect tile_bounds(gfx::Point(bounds.x() + col * total_tile_size.width(),
                                   bounds.y() + row * total_tile_size.height()),
                        total_tile_size);
  tile_bounds.Inset(-GetTilePadding());
  return tile_bounds;
}

AppListItemView* AppsGridView::GetViewDisplayedAtSlotOnCurrentPage(
    int slot) const {
  if (slot < 0)
    return nullptr;

  // Calculate the original bound of the tile at |index|.
  gfx::Rect tile_rect =
      GetExpectedTileBounds(GridIndex(pagination_model_.selected_page(), slot));

  for (int i = 0; i < view_model_.view_size(); ++i) {
    AppListItemView* view = GetItemViewAt(i);
    if (view->bounds() == tile_rect && view != drag_view_)
      return view;
  }
  return nullptr;
}

void AppsGridView::SetAsFolderDroppingTarget(const GridIndex& target_index,
                                             bool is_target_folder) {
  AppListItemView* target_view =
      GetViewDisplayedAtSlotOnCurrentPage(target_index.slot);
  if (target_view) {
    target_view->SetAsAttemptedFolderTarget(is_target_folder);
    if (is_new_style_launcher_enabled_) {
      if (is_target_folder)
        target_view->OnDraggedViewEnter();
      else
        target_view->OnDraggedViewExit();
    }
  }
}

bool AppsGridView::IsAppsGridGapEnabled() const {
  return !folder_delegate_ && is_apps_grid_gap_feature_enabled_;
}

GridIndex AppsGridView::GetIndexFromModelIndex(int model_index) const {
  if (IsAppsGridGapEnabled())
    return view_structure_.GetIndexFromModelIndex(model_index);

  const int tiles_in_page0 = TilesPerPage(0);
  const int tiles_in_page1 = TilesPerPage(1);

  if (model_index < tiles_in_page0)
    return GridIndex(0, model_index);

  return GridIndex(1 + (model_index - tiles_in_page0) / tiles_in_page1,
                   (model_index - tiles_in_page0) % tiles_in_page1);
}

int AppsGridView::GetModelIndexFromIndex(const GridIndex& index) const {
  if (IsAppsGridGapEnabled())
    return view_structure_.GetModelIndexFromIndex(index);

  if (index.page == 0)
    return index.slot;

  return TilesPerPage(0) + (index.page - 1) * TilesPerPage(1) + index.slot;
}

GridIndex AppsGridView::GetLastTargetIndex() const {
  if (IsAppsGridGapEnabled())
    return view_structure_.GetLastTargetIndex();

  DCHECK_LT(0, view_model_.view_size());
  int view_index = view_model_.view_size() - 1;
  return GetIndexFromModelIndex(view_index);
}

GridIndex AppsGridView::GetLastTargetIndexOfPage(int page) const {
  if (IsAppsGridGapEnabled())
    return view_structure_.GetLastTargetIndexOfPage(page);

  if (page == pagination_model_.total_pages() - 1)
    return GetLastTargetIndex();

  return GridIndex(page, TilesPerPage(page) - 1);
}

int AppsGridView::GetTargetModelIndexForMove(AppListItemView* moved_view,
                                             const GridIndex& index) const {
  if (IsAppsGridGapEnabled())
    return view_structure_.GetTargetModelIndexForMove(moved_view, index);

  return GetModelIndexFromIndex(index);
}

size_t AppsGridView::GetTargetItemIndexForMove(AppListItemView* moved_view,
                                               const GridIndex& index) const {
  if (IsAppsGridGapEnabled())
    return view_structure_.GetTargetItemIndexForMove(moved_view, index);

  // Model index is the same as item index when apps grid gap is disabled.
  return GetModelIndexFromIndex(index);
}

bool AppsGridView::IsValidIndex(const GridIndex& index) const {
  return index.page >= 0 && index.page < pagination_model_.total_pages() &&
         index.slot >= 0 && index.slot < TilesPerPage(index.page) &&
         GetModelIndexFromIndex(index) < view_model_.view_size();
}

bool AppsGridView::IsValidReorderTargetIndex(const GridIndex& index) const {
  if (IsAppsGridGapEnabled())
    return view_structure_.IsValidReorderTargetIndex(index);

  return IsValidIndex(index);
}

bool AppsGridView::IsValidPageFlipTarget(int page) const {
  if (pagination_model_.is_valid_page(page))
    return true;

  // If the user wants to drag an app to the next new page and has not done so
  // during the dragging session, then it is the right target because a new page
  // will be created in OnPageFlipTimer().
  return IsAppsGridGapEnabled() && !extra_page_opened_ &&
         pagination_model_.total_pages() == page;
}

void AppsGridView::CalculateIdealBoundsWithGridGap() {
  DCHECK(IsAppsGridGapEnabled());

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
    if (pulsing_block_index.slot == TilesPerPage(pulsing_block_index.page)) {
      ++pulsing_block_index.page;
      pulsing_block_index.slot = 0;
    }
    gfx::Rect tile_slot = GetExpectedTileBounds(pulsing_block_index);
    tile_slot.Offset(CalculateTransitionOffset(pulsing_block_index.page));
    pulsing_blocks_model_.set_ideal_bounds(i, tile_slot);
    ++pulsing_block_index.slot;
  }
}

int AppsGridView::GetModelIndexOfItem(const AppListItem* item) {
  for (int i = 0; i < view_model_.view_size(); ++i) {
    if (view_model_.view_at(i)->item() == item) {
      return i;
    }
  }
  return view_model_.view_size();
}

int AppsGridView::GetTargetModelIndexFromItemIndex(size_t item_index) {
  if (!IsAppsGridGapEnabled())
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
  UMA_HISTOGRAM_COUNTS_100(kNumberOfPagesHistogram,
                           pagination_model_.total_pages());

  // Calculate the number of pages that have empty slots.
  int page_count = 0;
  if (IsAppsGridGapEnabled()) {
    const auto& pages = view_structure_.pages();
    for (size_t i = 0; i < pages.size(); ++i) {
      if (static_cast<int>(pages[i].size()) < TilesPerPage(i))
        ++page_count;
    }
  } else {
    int item_num = view_model_.view_size();
    for (int i = 0; item_num > 0; ++i) {
      item_num -= TilesPerPage(i);
    }

    // Only last page allows gaps if it is not full when apps grid gap is
    // disabled.
    if (item_num != 0)
      page_count = 1;
  }
  UMA_HISTOGRAM_COUNTS_100(kNumberOfPagesNotFullHistogram, page_count);
}

void AppsGridView::RecordAppMovingTypeMetrics(AppListAppMovingType type) {
  UMA_HISTOGRAM_ENUMERATION(kAppListAppMovingType, type,
                            kMaxAppListAppMovingType);
}

void AppsGridView::UpdateTilePadding() {
  const gfx::Size content_size = GetContentsBounds().size();
  const gfx::Size tile_size = GetTileViewSize();

  // Item tiles should be evenly distributed in this view.
  horizontal_tile_padding_ =
      cols_ > 1 ? (content_size.width() - cols_ * tile_size.width()) /
                      ((cols_ - 1) * 2)
                : 0;
  vertical_tile_padding_ =
      rows_per_page_ > 1
          ? (content_size.height() - rows_per_page_ * tile_size.height()) /
                ((rows_per_page_ - 1) * 2)
          : 0;
}

int AppsGridView::GetItemsNumOfPage(int page) const {
  DCHECK(is_new_style_launcher_enabled_);
  if (page < 0 || page >= pagination_model_.total_pages())
    return 0;

  if (IsAppsGridGapEnabled())
    return view_structure_.items_on_page(page);

  if (page < pagination_model_.total_pages() - 1)
    return TilesPerPage(page);

  return item_list_->item_count() -
         (pagination_model_.total_pages() - 1) * TilesPerPage(0);
}

void AppsGridView::StartFolderDroppingAnimation(
    AppListItemView* folder_item_view,
    AppListItem* drag_item,
    const gfx::Rect& source_bounds) {
  // Calculate target bounds of dragged item.
  gfx::Rect target_bounds =
      GetTargetIconRectInFolder(drag_item, folder_item_view);

  // Update folder icon.
  AppListFolderItem* folder_item =
      static_cast<AppListFolderItem*>(folder_item_view->item());
  folder_item->NotifyOfDraggedItem(drag_item);

  // Start animation.
  TopIconAnimationView* animation_view = new TopIconAnimationView(
      drag_item->icon(), base::UTF8ToUTF16(drag_item->GetDisplayName()),
      target_bounds, false, true);
  AddChildView(animation_view);
  animation_view->SetBoundsRect(source_bounds);
  animation_view->AddObserver(
      new FolderDroppingAnimationObserver(model_, folder_item->id()));
  animation_view->TransformView();
}

}  // namespace app_list
