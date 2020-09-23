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
#include "ash/app_list/views/app_list_drag_and_drop_host.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/ghost_image_view.h"
#include "ash/app_list/views/pulsing_block_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_tile_item_view.h"
#include "ash/app_list/views/top_icon_animation_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/pagination/pagination_controller.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/transform_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/paint_info.h"
#include "ui/views/view_model_utils.h"
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

// Delay in milliseconds to do the page flip in fullscreen app list.
constexpr int kPageFlipDelayInMsFullscreen = 500;

// The drag and drop proxy should get scaled by this factor.
constexpr float kDragAndDropProxyScale = 1.2f;

// The apps grid should be scaled down by this factor.
constexpr float kCardifiedScale = 0.84f;

// Vertical padding between the apps grid pages in cardified state.
constexpr int kCardifiedPaddingBetweenPages = 12;

// Horizontal padding of the apps grid page in cardified state.
constexpr int kCardifiedHorizontalPadding = 16;

// The radius of the corner of the background cards in the apps grid.
constexpr int kBackgroundCardCornerRadius = 12;

// Delays in milliseconds to show re-order preview.
constexpr int kReorderDelay = 120;

// Delays in milliseconds to show folder item reparent UI.
constexpr int kFolderItemReparentDelay = 50;

// Maximum vertical and horizontal spacing between tiles.
constexpr int kMaximumTileSpacing = 96;

// The duration in ms for most of the apps grid view animations.
constexpr int kDefaultAnimationDuration = 200;

// The opacity for the background cards when hidden.
constexpr float kBackgroundCardOpacityHide = 0.0f;

// Animation curve used for fading in the target page when opening or closing
// a folder.
constexpr gfx::Tween::Type kFolderFadeInTweenType = gfx::Tween::EASE_IN_2;

// Animation curve used for fading out the target page when opening or closing
// a folder.
constexpr gfx::Tween::Type kFolderFadeOutTweenType =
    gfx::Tween::FAST_OUT_LINEAR_IN;

// Animation curve used for entering and exiting cardified state.
constexpr gfx::Tween::Type kCardifiedStateTweenType = gfx::Tween::EASE_OUT_2;

// Presentation time histogram for apps grid scroll by dragging.
constexpr char kPageDragScrollInClamshellHistogram[] =
    "Apps.PaginationTransition.DragScroll.PresentationTime.ClamshellMode";
constexpr char kPageDragScrollInClamshellMaxLatencyHistogram[] =
    "Apps.PaginationTransition.DragScroll.PresentationTime.MaxLatency."
    "ClamshellMode";
constexpr char kPageDragScrollInTabletHistogram[] =
    "Apps.PaginationTransition.DragScroll.PresentationTime.TabletMode";
constexpr char kPageDragScrollInTabletMaxLatencyHistogram[] =
    "Apps.PaginationTransition.DragScroll.PresentationTime.MaxLatency."
    "TabletMode";

// Returns the size of a tile view excluding its padding.
gfx::Size GetTileViewSize(const AppListConfig& config, bool cardified_state) {
  return gfx::ScaleToRoundedSize(
      gfx::Size(config.grid_tile_width(), config.grid_tile_height()),
      (cardified_state ? kCardifiedScale : 1.0f));
}

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
  ~RowMoveAnimationDelegate() override {}

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

  ~ItemRemoveAnimationDelegate() override {}

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override {
    view_->layer()->SetOpacity(1 - animation->GetCurrentValue());
    view_->layer()->ScheduleDraw();
  }

 private:
  std::unique_ptr<views::View> view_;

  DISALLOW_COPY_AND_ASSIGN(ItemRemoveAnimationDelegate);
};

// CardifiedAnimationObserver is used to observe the animation for toggling the
// cardified state of the apps grid view. We used this to ensure app icons are
// repainted with the correct bounds and scale.
class CardifiedAnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  explicit CardifiedAnimationObserver(base::OnceClosure callback)
      : callback_(std::move(callback)) {}
  ~CardifiedAnimationObserver() override {}

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    if (callback_)
      std::move(callback_).Run();
    delete this;
  }

 private:
  base::OnceClosure callback_;

  DISALLOW_COPY_AND_ASSIGN(CardifiedAnimationObserver);
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

// Returns true if the |item| is a folder item.
bool IsFolderItem(AppListItem* item) {
  return (item->GetItemType() == AppListFolderItem::kItemType);
}

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

// A layer delegate used for AppsGridView's mask layer, with top and bottom
// gradient fading out zones.
class AppsGridView::FadeoutLayerDelegate : public ui::LayerDelegate {
 public:
  explicit FadeoutLayerDelegate(int fadeout_mask_height)
      : layer_(ui::LAYER_TEXTURED), fadeout_mask_height_(fadeout_mask_height) {
    layer_.set_delegate(this);
    layer_.SetFillsBoundsOpaquely(false);
  }

  ~FadeoutLayerDelegate() override { layer_.set_delegate(nullptr); }

  ui::Layer* layer() { return &layer_; }

 private:
  // ui::LayerDelegate:
  // TODO(warx): using a mask is expensive. It would be more efficient to avoid
  // the mask for the central area and only use it for top/bottom areas.
  void OnPaintLayer(const ui::PaintContext& context) override {
    const gfx::Size size = layer()->size();
    gfx::Rect top_rect(0, 0, size.width(), fadeout_mask_height_);
    gfx::Rect bottom_rect(0, size.height() - fadeout_mask_height_, size.width(),
                          fadeout_mask_height_);

    views::PaintInfo paint_info =
        views::PaintInfo::CreateRootPaintInfo(context, size);
    const auto& prs = paint_info.paint_recording_size();

    //  Pass the scale factor when constructing PaintRecorder so the MaskLayer
    //  size is not incorrectly rounded (see https://crbug.com/921274).
    ui::PaintRecorder recorder(context, paint_info.paint_recording_size(),
                               static_cast<float>(prs.width()) / size.width(),
                               static_cast<float>(prs.height()) / size.height(),
                               nullptr);

    gfx::Canvas* canvas = recorder.canvas();
    // Clear the canvas.
    canvas->DrawColor(SK_ColorBLACK, SkBlendMode::kSrc);
    // Draw top gradient zone.
    cc::PaintFlags flags;
    flags.setBlendMode(SkBlendMode::kSrc);
    flags.setAntiAlias(false);
    flags.setShader(gfx::CreateGradientShader(
        gfx::Point(), gfx::Point(0, fadeout_mask_height_), SK_ColorTRANSPARENT,
        SK_ColorBLACK));
    canvas->DrawRect(top_rect, flags);
    // Draw bottom gradient zone.
    flags.setShader(gfx::CreateGradientShader(
        gfx::Point(0, size.height() - fadeout_mask_height_),
        gfx::Point(0, size.height()), SK_ColorBLACK, SK_ColorTRANSPARENT));
    canvas->DrawRect(bottom_rect, flags);
  }
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  ui::Layer layer_;
  const int fadeout_mask_height_;

  DISALLOW_COPY_AND_ASSIGN(FadeoutLayerDelegate);
};

AppsGridView::AppsGridView(ContentsView* contents_view,
                           AppsGridViewFolderDelegate* folder_delegate)
    : folder_delegate_(folder_delegate),
      contents_view_(contents_view),
      page_flip_delay_in_ms_(kPageFlipDelayInMsFullscreen),
      view_structure_(this) {
  DCHECK(contents_view_);
  SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  // Clip any icons that are outside the grid view's bounds. These icons would
  // otherwise be visible to the user when the grid view is off screen.
  layer()->SetMasksToBounds(true);

  items_container_ = AddChildView(std::make_unique<views::View>());
  items_container_->SetPaintToLayer();
  items_container_->layer()->SetFillsBoundsOpaquely(false);
  bounds_animator_ = std::make_unique<views::BoundsAnimator>(
      items_container_, /*use_transforms=*/true);

  if (!folder_delegate) {
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets(GetAppListConfig().grid_fadeout_mask_height(), 0)));
  }

  pagination_model_.SetTransitionDurations(
      GetAppListConfig().page_transition_duration(),
      GetAppListConfig().overscroll_page_transition_duration());

  pagination_model_.AddObserver(this);
  pagination_controller_ = std::make_unique<PaginationController>(
      &pagination_model_,
      folder_delegate_ ? PaginationController::SCROLL_AXIS_HORIZONTAL
                       : PaginationController::SCROLL_AXIS_VERTICAL,
      folder_delegate_
          ? base::DoNothing()
          : base::BindRepeating(&AppListRecordPageSwitcherSourceByEventType),
      IsTabletMode());
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
  pagination_model_.RemoveObserver(this);

  if (item_list_)
    item_list_->RemoveObserver(this);

  // Cancel animations now, otherwise RemoveAllChildViews() may call back to
  // ViewHierarchyChanged() during removal, which can lead to double deletes
  // (because ViewHierarchyChanged() may attempt to delete a view that is part
  // way through deletion).
  bounds_animator_->Cancel();

  view_model_.Clear();
  RemoveAllChildViews(true);
}

void AppsGridView::SetLayout(int cols, int rows_per_page) {
  cols_ = cols;
  rows_per_page_ = rows_per_page;
}

gfx::Size AppsGridView::GetTotalTileSize() const {
  gfx::Rect rect(GetTileViewSize(GetAppListConfig(), cardified_state_));
  rect.Inset(GetTilePadding());
  return rect.size();
}

gfx::Insets AppsGridView::GetTilePadding() const {
  if (folder_delegate_) {
    const int tile_padding_in_folder =
        GetAppListConfig().grid_tile_spacing_in_folder() / 2;
    return gfx::Insets(-tile_padding_in_folder, -tile_padding_in_folder);
  }
  return gfx::Insets(-vertical_tile_padding_, -horizontal_tile_padding_);
}

gfx::Size AppsGridView::GetTileGridSizeWithPadding() const {
  gfx::Size size(GetTileViewSize(GetAppListConfig(), cardified_state_));
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
  const gfx::Size tile_size =
      GetTileViewSize(GetAppListConfig(), cardified_state_);
  return gfx::Size(tile_size.width() * cols,
                   tile_size.height() * rows_per_page);
}

gfx::Size AppsGridView::GetMaximumTileGridSize(int cols,
                                               int rows_per_page) const {
  const gfx::Size tile_size =
      GetTileViewSize(GetAppListConfig(), cardified_state_);
  return gfx::Size(tile_size.width() * cols + kMaximumTileSpacing * (cols - 1),
                   tile_size.height() * rows_per_page +
                       kMaximumTileSpacing * (rows_per_page - 1));
}

int AppsGridView::GetPaddingBetweenPages() const {
  // In cardified state, padding between pages should be fixed  and it should
  // include background card padding.
  return cardified_state_
             ? kCardifiedPaddingBetweenPages + 2 * vertical_tile_padding_
             : GetAppListConfig().page_spacing();
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

void AppsGridView::OnTabletModeChanged(bool started) {
  pagination_controller_->set_is_tablet_mode(started);

  // Enable/Disable folder icons's background blur based on tablet mode.
  for (const auto& entry : view_model_.entries()) {
    auto* item_view = static_cast<AppListItemView*>(entry.view);
    if (item_view->item()->is_folder())
      item_view->SetBackgroundBlurEnabled(started);
  }

  // Prevent context menus from remaining open after a transition
  CancelContextMenusOnCurrentPage();
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
  return nullptr;
}

void AppsGridView::InitiateDrag(AppListItemView* view,
                                Pointer pointer,
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

bool AppsGridView::UpdateDragFromItem(Pointer pointer,
                                      const ui::LocatedEvent& event) {
  if (!drag_view_)
    return false;  // Drag canceled.

  if (!cardified_state_)
    StartAppsGridCardifiedView();

  gfx::Point drag_point_in_grid_view;
  ExtractDragLocation(event.root_location(), &drag_point_in_grid_view);
  UpdateDrag(pointer, drag_point_in_grid_view);
  if (!dragging())
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

  if (cardified_state_) {
    int hovered_page = GetPageFlipTargetForDrag(last_drag_point_);
    if (hovered_page == -1)
      hovered_page = pagination_model_.selected_page();

    SetHighlightedBackgroundCard(hovered_page);
  }

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

    if (!cancel && dragging()) {
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

  if (folder_item_view) {
    // Run an animation to move dragged item to the folder.
    StartFolderDroppingAnimation(folder_item_view, drag_item,
                                 drag_source_bounds);
  }

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
  StopPageFlipTimer();
  if (cardified_state_) {
    // Temporarily set to cardified UI State so it animates back to its position
    // smoothly with all other icons.
    released_drag_view->SetCardifyUIState();
    EndAppsGridCardifiedView();
  }
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
  if (index < 0 || index >= view_model_.view_size())
    return nullptr;
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
  animation.SetTransitionDuration(
      show ? GetAppListConfig().folder_transition_in_duration()
           : GetAppListConfig().folder_transition_out_duration());

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
  auto view = std::make_unique<AppListItemView>(
      this, original_drag_view->item(),
      contents_view_->GetAppListMainView()->view_delegate(),
      false /* is_in_folder */);
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

  drag_start_page_ = pagination_model_.selected_page();
  drag_start_grid_view_ = drag_point;

  drag_view_start_ = drag_view_->origin();

  // Set the flag in root level grid view.
  dragging_for_reparent_item_ = true;

  if (!cardified_state_)
    StartAppsGridCardifiedView();
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
  drag_start_page_ = -1;
  drag_view_offset_ = gfx::Point();

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

const char* AppsGridView::GetClassName() const {
  return "AppsGridView";
}

void AppsGridView::Layout() {
  if (ignore_layout_)
    return;

  if (bounds_animator_->IsAnimating())
    bounds_animator_->Cancel();

  if (GetContentsBounds().IsEmpty())
    return;

  // Update cached tile padding first, as grid size calculations depend on the
  // cached padding value.
  UpdateTilePadding();

  // Prepare |page_size| * number-of-pages for |items_container_|, and sets the
  // origin properly to show the correct page.
  const gfx::Size page_size = GetTileGridSize();
  const int pages = pagination_model_.total_pages();
  const int current_page = pagination_model_.selected_page();
  if (pagination_controller_->scroll_axis() ==
      PaginationController::SCROLL_AXIS_HORIZONTAL) {
    const int page_width = page_size.width() + GetPaddingBetweenPages();
    items_container_->SetBoundsRect(gfx::Rect(-page_width * current_page, 0,
                                              page_width * pages,
                                              GetContentsBounds().height()));
  } else {
    const int page_height = page_size.height() + GetPaddingBetweenPages();
    items_container_->SetBoundsRect(gfx::Rect(0, -page_height * current_page,
                                              GetContentsBounds().width(),
                                              page_height * pages));
  }

  if (fadeout_layer_delegate_)
    fadeout_layer_delegate_->layer()->SetBounds(layer()->bounds());

  CalculateIdealBoundsForFolder();
  for (int i = 0; i < view_model_.view_size(); ++i) {
    AppListItemView* view = GetItemViewAt(i);
    if (view != drag_view_) {
      view->SetBoundsRect(view_model_.ideal_bounds(i));
    } else {
      view->SetSize(GetTileViewSize(GetAppListConfig(), cardified_state_));
    }
  }
  if (cardified_state_) {
    DCHECK(!background_cards_.empty());
    MaybeCreateGradientMask();
    // Make sure that the background cards render behind everything
    // else in the items container.
    for (auto& background_card : background_cards_)
      items_container_->layer()->StackAtBottom(background_card.get());
  }
  views::ViewModelUtils::SetViewBoundsToIdealBounds(pulsing_blocks_model_);
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

  return HandleVerticalFocusMovement(event.key_code() ==
                                     ui::VKEY_UP /* arrow_up */);
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

void AppsGridView::OnGestureEvent(ui::GestureEvent* event) {
  // If a tap/long-press occurs within a valid tile, it is usually a mistake and
  // should not close the launcher in clamshell mode. Otherwise, we should let
  // those events pass to the ancestor views.
  if (!contents_view_->app_list_view()->is_tablet_mode() &&
      (event->type() == ui::ET_GESTURE_TAP ||
       event->type() == ui::ET_GESTURE_LONG_PRESS)) {
    if (EventIsBetweenOccupiedTiles(event)) {
      contents_view_->app_list_view()->CloseKeyboardIfVisible();
      event->SetHandled();
    }
    return;
  }

  if (!ShouldHandleDragEvent(*event))
    return;

  // Scroll begin events should not be passed to ancestor views from apps grid
  // in our current design. This prevents both ignoring horizontal scrolls in
  // app list, and closing open folders.
  if (pagination_controller_->OnGestureEvent(*event, GetContentsBounds()) ||
      event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    event->SetHandled();
  }
}

void AppsGridView::OnMouseEvent(ui::MouseEvent* event) {
  if (contents_view_->app_list_view()->is_tablet_mode() ||
      !event->IsLeftMouseButton()) {
    return;
  }
  gfx::PointF point_in_root = event->root_location_f();

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      if (!EventIsBetweenOccupiedTiles(event))
        break;
      event->SetHandled();
      mouse_drag_start_point_ = point_in_root;
      last_mouse_drag_point_ = point_in_root;
      break;
    case ui::ET_MOUSE_DRAGGED:
      if (!ShouldHandleDragEvent(*event)) {
        // We need to send mouse drag/release events to AppListView explicitly
        // because AppsGridView handles the mouse press event and gets captured.
        // Then AppListView cannot receive mouse drag/release events implcitly.

        // Send the fabricated mouse press event to AppListView if AppsGridView
        // is not in mouse drag yet.
        gfx::Point drag_location_in_app_list;
        if (!is_in_mouse_drag_) {
          ui::MouseEvent press_event(
              *event, static_cast<views::View*>(this),
              static_cast<views::View*>(contents_view_->app_list_view()),
              ui::ET_MOUSE_PRESSED, event->flags());
          contents_view_->app_list_view()->OnMouseEvent(&press_event);

          is_in_mouse_drag_ = true;
        }

        drag_location_in_app_list = event->location();
        ConvertPointToTarget(this, contents_view_->app_list_view(),
                             &drag_location_in_app_list);
        event->set_location(drag_location_in_app_list);
        contents_view_->app_list_view()->OnMouseEvent(event);
        break;
      }
      event->SetHandled();
      if (!is_in_mouse_drag_) {
        if (abs(point_in_root.y() - mouse_drag_start_point_.y()) <
            kMouseDragThreshold) {
          break;
        }
        pagination_controller_->StartMouseDrag(point_in_root -
                                               mouse_drag_start_point_);
        is_in_mouse_drag_ = true;
      }

      if (!is_in_mouse_drag_)
        break;

      pagination_controller_->UpdateMouseDrag(
          point_in_root - last_mouse_drag_point_, GetContentsBounds());
      last_mouse_drag_point_ = point_in_root;
      break;
    case ui::ET_MOUSE_RELEASED: {
      // Calculate |should_handle| before resetting |mouse_drag_start_point_|
      // because ShouldHandleDragEvent depends on its value.
      const bool should_handle = ShouldHandleDragEvent(*event);

      is_in_mouse_drag_ = false;
      mouse_drag_start_point_ = gfx::PointF();
      last_mouse_drag_point_ = gfx::PointF();

      if (!should_handle) {
        gfx::Point drag_location_in_app_list = event->location();
        ConvertPointToTarget(this, contents_view_->app_list_view(),
                             &drag_location_in_app_list);
        event->set_location(drag_location_in_app_list);
        contents_view_->app_list_view()->OnMouseEvent(event);
        break;
      }
      event->SetHandled();
      pagination_controller_->EndMouseDrag(*event);
      break;
    }
    default:
      return;
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

int AppsGridView::TilesPerPage(int page) const {
  if (folder_delegate_)
    return GetAppListConfig().max_folder_items_per_page();

  return GetAppListConfig().GetMaxNumOfItemsPerPage(page);
}

void AppsGridView::UpdatePaging() {
  if (!folder_delegate_) {
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

std::unique_ptr<AppListItemView> AppsGridView::CreateViewForItemAtIndex(
    size_t index) {
  // The |drag_view_| might be pending for deletion, therefore |view_model_|
  // may have one more item than |item_list_|.
  DCHECK_LE(index, item_list_->item_count());
  std::unique_ptr<AppListItemView> view = std::make_unique<AppListItemView>(
      this, item_list_->item_at(index),
      contents_view_->GetAppListMainView()->view_delegate());
  return view;
}

bool AppsGridView::HandleScroll(const gfx::Vector2d& offset,
                                ui::EventType type) {
  // If |pagination_model_| is empty, don't handle scroll events.
  if (pagination_model_.total_pages() <= 0)
    return false;

  return pagination_controller_->OnScroll(offset, type);
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
  if (selected_view_->HasNotificationBadge())
    AnnounceItemNotificationBadge(selected_view_->title()->GetText());
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

  if (pagination_controller_->scroll_axis() ==
      PaginationController::SCROLL_AXIS_HORIZONTAL) {
    // Page size including padding pixels. A tile.x + page_width means the same
    // tile slot in the next page.
    const int page_width = grid_size.width() + GetPaddingBetweenPages();
    return gfx::Vector2d(page_width * multiplier, 0);
  }

  const int page_height = grid_size.height() + GetPaddingBetweenPages();
  return gfx::Vector2d(0, page_height * multiplier);
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
      (GetAppListConfig().grid_tile_width() + horizontal_tile_padding_ * 2) *
      0.4;
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

  const base::string16 moving_view_title = selected_view_->title()->GetText();
  AppListItemView* target_view =
      GetViewDisplayedAtSlotOnCurrentPage(target_index.slot);
  const base::string16 target_view_title = target_view->title()->GetText();
  const bool target_view_is_folder = target_view->is_folder();

  AppListItemView* folder_item = MoveItemToFolder(selected_view_, target_index);
  AnnounceKeyboardFoldering(moving_view_title, target_view_title,
                            target_view_is_folder);
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
  int items_in_one_page = std::min(
      GetAppListConfig().max_folder_items_per_page(), item_list_->item_count());
  cols_ = std::sqrt(items_in_one_page - 1) + 1;
  rows_per_page_ = (items_in_one_page - 1) / cols_ + 1;
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

  if (cardified_state_)
    EndAppsGridCardifiedView();

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

  // Hide |current_ghost_view_| in the hidden folder grid view.
  BeginHideCurrentGhostImageView();
}

void AppsGridView::OnFolderItemRemoved() {
  DCHECK(folder_delegate_);
  if (item_list_)
    item_list_->RemoveObserver(this);
  item_list_ = nullptr;
}

void AppsGridView::UpdateOpacity(bool restore_opacity) {
  if (view_structure_.pages().empty())
    return;

  // App list view state animations animate the apps grid view opacity rather
  // than individual items' opacity. This method (used during app list view
  // drag) sets up opacity for individual grid item, and assumes that the apps
  // grid view is fully opaque.
  layer()->SetOpacity(1.0f);

  // Updates the opacity of the apps in current page. The opacity of the app
  // starting at 0.f when the ceterline of the app is |kAllAppsOpacityStartPx|
  // above the bottom of work area and transitioning to 1.0f by the time the
  // centerline reaches |kAllAppsOpacityEndPx| above the work area bottom.
  AppListView* app_list_view = contents_view_->app_list_view();
  const int selected_page = pagination_model_.selected_page();
  auto current_page = view_structure_.pages()[selected_page];

  // First it should prepare the layers for all of the app items in the current
  // page when necessary, or destroy all of the layers when they become
  // unnecessary. Do not dynamically ensure/destroy layers of individual items
  // since the creation/destruction of the layer requires to repaint the parent
  // view (i.e. this class).
  if (restore_opacity) {
    // If drag is in progress, layers are still required, so just update the
    // opacity (the layers will be deleted when drag operation completes).
    if (items_need_layer_for_drag_) {
      for (const auto& entry : view_model_.entries()) {
        if (drag_view_ != entry.view && entry.view->layer())
          entry.view->layer()->SetOpacity(1.0f);
      }
      return;
    }

    // Layers are not necessary. Destroy them, and return. No need to update
    // opacity. This needs to be done on all views within |view_model_| because
    // some item view might have been moved out from the current page. See also
    // https://crbug.com/990529.
    for (const auto& entry : view_model_.entries())
      entry.view->DestroyLayer();
    return;
  }

  // Ensure layers and update their opacity.
  for (size_t i = 0; i < current_page.size(); ++i)
    current_page[i]->EnsureLayer();

  float centerline_above_work_area = 0.f;
  float opacity = 0.f;
  for (size_t i = 0; i < current_page.size(); i += cols_) {
    AppListItemView* item_view = current_page[i];
    gfx::Rect view_bounds = item_view->GetLocalBounds();
    views::View::ConvertRectToScreen(item_view, &view_bounds);
    centerline_above_work_area = std::max<float>(
        app_list_view->GetScreenBottom() - view_bounds.CenterPoint().y(), 0.f);
    const float start_px = GetAppListConfig().all_apps_opacity_start_px();
    opacity = base::ClampToRange(
        (centerline_above_work_area - start_px) /
            (GetAppListConfig().all_apps_opacity_end_px() - start_px),
        0.f, 1.0f);

    if (opacity == item_view->layer()->opacity())
      continue;

    const size_t end_index = std::min(current_page.size() - 1, i + cols_ - 1);
    for (size_t j = i; j <= end_index; ++j) {
      if (current_page[j] != drag_view_)
        current_page[j]->layer()->SetOpacity(opacity);
    }
  }
}

bool AppsGridView::HandleScrollFromAppListView(const gfx::Vector2d& offset,
                                               ui::EventType type) {
  // Scroll up at first page in top level apps grid should close the launcher.
  if (!folder_delegate_ && offset.y() > 0 &&
      !pagination_model()->IsValidPageRelative(-1)) {
    return false;
  }

  HandleScroll(offset, type);
  return true;
}

void AppsGridView::HandleKeyboardReparent(AppListItemView* reparented_view,
                                          ui::KeyboardCode key_code) {
  DCHECK(key_code == ui::VKEY_LEFT || key_code == ui::VKEY_RIGHT ||
         key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN);
  DCHECK(!folder_delegate_);
  DCHECK(activated_folder_item_view_);

  auto reparented_view_in_root_grid = std::make_unique<AppListItemView>(
      this, reparented_view->item(),
      contents_view_->GetAppListMainView()->view_delegate(),
      false /* is_in_folder */);

  auto* reparented_view_in_root_grid_ptr =
      items_container_->AddChildView(std::move(reparented_view_in_root_grid));
  view_model_.Add(reparented_view_in_root_grid_ptr, view_model_.view_size());
  view_structure_.Add(reparented_view_in_root_grid_ptr, GetLastTargetIndex());

  // Set |activated_folder_item_view_| selected so |target_index| will be
  // computed relative to the open folder.
  SetSelectedView(activated_folder_item_view_);
  const GridIndex target_index =
      GetTargetGridIndexForKeyboardReparent(key_code);
  AnnounceReorder(target_index);
  ReparentItemForReorder(reparented_view_in_root_grid_ptr, target_index);

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
  return contents_view_->app_list_view()->is_tablet_mode();
}

void AppsGridView::OnAppListConfigUpdated() {
  for (int i = 0; i < view_model_.view_size(); ++i)
    view_model_.view_at(i)->RefreshIcon();

  InvalidateLayout();
}

const AppListConfig& AppsGridView::GetAppListConfig() const {
  return contents_view_->app_list_view()->GetAppListConfig();
}

void AppsGridView::StartAppsGridCardifiedView() {
  if (!app_list_features::IsNewDragSpecInLauncherEnabled())
    return;
  if (folder_delegate_)
    return;
  DCHECK(!cardified_state_);
  StopObservingImplicitAnimations();
  RemoveAllBackgroundCards();
  cardified_state_ = true;
  UpdateTilePadding();
  for (int i = 0; i < pagination_model_.total_pages(); i++)
    AppendBackgroundCard();
  MaybeCreateGradientMask();
  AnimateCardifiedState();
}

void AppsGridView::EndAppsGridCardifiedView() {
  if (!app_list_features::IsNewDragSpecInLauncherEnabled())
    return;
  if (folder_delegate_)
    return;
  DCHECK(cardified_state_);
  StopObservingImplicitAnimations();
  cardified_state_ = false;
  // Update the padding between tiles, so we can animate back the apps grid
  // elements to their original positions.
  UpdateTilePadding();
  AnimateCardifiedState();
  layer()->SetClipRect(gfx::Rect());
}

void AppsGridView::AnimateCardifiedState() {
  if (GetWidget()) {
    // Normally Layout() cancels any animations. At this point there may be a
    // pending Layout(), force it now so that one isn't triggered part way
    // through the animation. Further, ignore this layout so that the position
    // isn't reset.
    DCHECK(!ignore_layout_);
    base::AutoReset<bool> auto_reset(&ignore_layout_, true);
    GetWidget()->LayoutRootViewIfNecessary();
  }

  CalculateIdealBounds();
  // Cache the current item container position, as RecenterItemsContainer() may
  // change it.
  gfx::Point start_position = items_container_->origin();
  RecenterItemsContainer();
  gfx::Vector2d translate_offset(
      0, start_position.y() - items_container_->origin().y());
  if (cardified_state_) {
    // The drag view is translated when the items container is recentered.
    // Reposition the drag view to compensate for the translation offset.
    drag_view_start_ += translate_offset;
    drag_view_->SetPosition(drag_view_start_);
  }
  // Drag view can be nullptr by EndDrag.
  const int number_of_views_to_animate =
      view_model_.view_size() - (drag_view_ ? 1 : 0);

  base::RepeatingClosure on_bounds_animator_callback;
  if (number_of_views_to_animate > 0) {
    on_bounds_animator_callback = base::BarrierClosure(
        number_of_views_to_animate,
        base::BindOnce(&AppsGridView::MaybeCallOnBoundsAnimatorDone,
                       weak_ptr_factory_.GetWeakPtr()));
    bounds_animation_for_cardified_state_in_progress_++;
  }

  for (int i = 0; i < view_model_.view_size(); ++i) {
    AppListItemView* entry_view = view_model_.view_at(i);
    // We don't animate bounds for the dragged view.
    if (entry_view == drag_view_)
      continue;
    // Reposition view bounds to compensate for the translation offset.
    gfx::Rect current_bounds = entry_view->bounds();
    current_bounds.Offset(translate_offset);

    if (cardified_state_)
      entry_view->SetCardifyUIState();
    else
      entry_view->SetNormalUIState();

    gfx::Rect target_bounds(view_model_.ideal_bounds(i));
    entry_view->SetBoundsRect(target_bounds);

    entry_view->EnsureLayer();

    // View bounds are currently |target_bounds|. Transform the view so it
    // appears in |current_bounds|.
    gfx::Transform transform = gfx::TransformBetweenRects(
        gfx::RectF(target_bounds), gfx::RectF(current_bounds));
    entry_view->layer()->SetTransform(transform);

    ui::ScopedLayerAnimationSettings animator(
        entry_view->layer()->GetAnimator());
    animator.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    animator.SetTweenType(kCardifiedStateTweenType);
    if (!cardified_state_) {
      animator.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(kDefaultAnimationDuration));
    }
    // When the animations are done, discard the layer and reset view to
    // proper scale.
    animator.AddObserver(
        new CardifiedAnimationObserver(on_bounds_animator_callback));
    entry_view->layer()->SetTransform(gfx::Transform());
  }

  for (auto& background_card : background_cards_) {
    if (!cardified_state_) {
      // Reposition card bounds to compensate for the translation offset.
      gfx::Transform translate_transform = gfx::Transform();
      translate_transform.Translate(translate_offset);
      background_card->SetTransform(translate_transform);
    }
    ui::ScopedLayerAnimationSettings animator(background_card->GetAnimator());
    animator.SetTweenType(kCardifiedStateTweenType);
    animator.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    if (!cardified_state_) {
      animator.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(kDefaultAnimationDuration));
    }
    animator.AddObserver(this);
    ui::AnimationThroughputReporter reporter(
        background_card->GetAnimator(),
        metrics_util::ForSmoothness(
            base::BindRepeating(&ReportCardifiedSmoothness, cardified_state_)));
    if (cardified_state_) {
      const bool is_active_page =
          background_cards_[pagination_model_.selected_page()] ==
          background_card;
      background_card->SetColor(
          GetAppListConfig().GetCardifiedBackgroundColor(is_active_page));
    } else {
      background_card->SetOpacity(kBackgroundCardOpacityHide);
    }
  }
  highlighted_page_ = pagination_model_.selected_page();
}

void AppsGridView::RecenterItemsContainer() {
  const int pages = pagination_model_.total_pages();
  const int current_page = pagination_model_.selected_page();
  const int page_height = GetTileGridSize().height() + GetPaddingBetweenPages();
  items_container_->SetBoundsRect(gfx::Rect(0, -page_height * current_page,
                                            GetContentsBounds().width(),
                                            page_height * pages));
}

bool AppsGridView::FirePageFlipTimerForTest() {
  if (!page_flip_timer_.IsRunning())
    return false;
  page_flip_timer_.FireNow();
  return true;
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
    StopPageFlipTimer();
  }
}

void AppsGridView::MaybeStartPageFlipTimer(const gfx::Point& drag_point) {
  if (!IsPointWithinPageFlipBuffer(drag_point))
    StopPageFlipTimer();
  int new_page_flip_target = GetPageFlipTargetForDrag(drag_point);

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
  if (!folder_delegate_)
    RecordPageSwitcherSource(kDragAppToBorder, IsTabletMode());

  BeginHideCurrentGhostImageView();
}

void AppsGridView::MoveItemInModel(AppListItemView* item_view,
                                   const GridIndex& target,
                                   bool clear_overflow) {
  int current_model_index = view_model_.GetIndexOfView(item_view);
  size_t current_item_list_index;
  item_list_->FindItemIndex(item_view->item()->id(), &current_item_list_index);
  DCHECK_GE(current_model_index, 0);

  int target_model_index = GetTargetModelIndexForMove(item_view, target);
  size_t target_item_list_index = GetTargetItemIndexForMove(item_view, target);
  // The same item index does not guarantee the same visual index, so move the
  // item visual index here.
  if (!folder_delegate_)
    view_structure_.Move(item_view, target, clear_overflow);

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
  int end =
      std::min(view_model_.view_size(), start + TilesPerPage(start_index.page));
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

bool AppsGridView::IsPointWithinPageFlipBuffer(const gfx::Point& point) const {
  // The page flip buffer is the work area bounds excluding shelf bounds, which
  // is the same as AppsContainerView's bounds.
  gfx::Point point_in_parent = point;
  ConvertPointToTarget(this, parent(), &point_in_parent);
  return parent()->GetContentsBounds().Contains(point_in_parent);
}

bool AppsGridView::IsPointWithinBottomDragBuffer(
    const gfx::Point& point) const {
  // The bottom drag buffer is between the bottom of apps grid and top of shelf.
  gfx::Point point_in_parent = point;
  ConvertPointToTarget(this, parent(), &point_in_parent);
  gfx::Rect parent_rect = parent()->GetContentsBounds();
  const int kBottomDragBufferMax = parent_rect.bottom();
  const int kBottomDragBufferMin = bounds().bottom() - GetInsets().bottom() -
                                   GetAppListConfig().page_flip_zone_size();
  return point_in_parent.y() > kBottomDragBufferMin &&
         point_in_parent.y() < kBottomDragBufferMax;
}

void AppsGridView::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  if (dragging())
    return;

  if (strcmp(sender->GetClassName(), AppListItemView::kViewClassName))
    return;

  if (contents_view_->apps_container_view()
          ->app_list_folder_view()
          ->IsAnimationRunning()) {
    return;
  }

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

void AppsGridView::AppendBackgroundCard() {
  background_cards_.push_back(
      std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR));
  ui::Layer* current_layer = background_cards_.back().get();
  // The size of the grid excluding the outer padding.
  const gfx::Size grid_size = GetTileGridSize();
  // The size for the background card that will be displayed. The outer padding
  // of the grid need to be added.
  const gfx::Size background_card_size =
      grid_size +
      gfx::Size(2 * horizontal_tile_padding_, 2 * vertical_tile_padding_);
  const int padding_between_pages = GetPaddingBetweenPages();
  // The space that each page occupies in the items container. This is the size
  // of the grid without outer padding plus the padding between pages.
  const int grid_size_height = grid_size.height() + padding_between_pages;
  const int new_page_index = background_cards_.size() - 1;
  // We position a new card in the last place in items container view.
  const int vertical_page_start_offset = grid_size_height * new_page_index;
  // Add a padding on the sides to make space for pagination preview.
  const int horizontal_padding =
      (GetContentsBounds().width() - background_card_size.width()) / 2 +
      kCardifiedHorizontalPadding;
  // The vertical padding should account for the fadeout mask.
  const int vertical_padding =
      (GetContentsBounds().height() - background_card_size.height()) / 2 +
      GetAppListConfig().grid_fadeout_mask_height();
  current_layer->SetBounds(gfx::Rect(
      horizontal_padding, vertical_padding + vertical_page_start_offset,
      background_card_size.width() - 2 * kCardifiedHorizontalPadding,
      background_card_size.height()));
  current_layer->SetVisible(true);
  current_layer->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kBackgroundCardCornerRadius));
  items_container_->layer()->Add(current_layer);
}

void AppsGridView::RemoveBackgroundCard() {
  items_container_->layer()->Remove(background_cards_.back().get());
  background_cards_.pop_back();
}

void AppsGridView::MaskContainerToBackgroundBounds() {
  DCHECK(!background_cards_.empty());
  // Mask apps grid container layer to the background card width.
  layer()->SetClipRect(gfx::Rect(background_cards_[0]->bounds().x(), 0,
                                 background_cards_[0]->bounds().width(),
                                 layer()->bounds().height()));
}

void AppsGridView::RemoveAllBackgroundCards() {
  for (auto& card : background_cards_)
    items_container_->layer()->Remove(card.get());
  background_cards_.clear();
}

void AppsGridView::TotalPagesChanged(int previous_page_count,
                                     int new_page_count) {
  // Don't record from folder.
  if (folder_delegate_)
    return;

  // Initial setup for the AppList starts with -1 pages. Ignore the page count
  // change resulting from the initialization of the view.
  if (previous_page_count == -1)
    return;

  if (previous_page_count < new_page_count) {
    AppListPageCreationType type = AppListPageCreationType::kSyncOrInstall;
    if (handling_keyboard_move_)
      type = AppListPageCreationType::kMovingAppWithKeyboard;
    else if (dragging())
      type = AppListPageCreationType::kDraggingApp;
    UMA_HISTOGRAM_ENUMERATION("Apps.AppList.AppsGridAddPage", type);
  }

  if (!cardified_state_)
    return;

  const int page_difference = new_page_count - previous_page_count;
  if (page_difference > 0) {
    for (int i = background_cards_.size(); i <= new_page_count; ++i) {
      AppendBackgroundCard();
    }
  } else {
    for (int i = 0; i < page_difference; ++i)
      RemoveBackgroundCard();
  }
}

void AppsGridView::SelectedPageChanged(int old_selected, int new_selected) {
  items_container_->layer()->SetTransform(gfx::Transform());
  if (dragging()) {
    drag_view_->layer()->SetTransform(gfx::Transform());

    // Sets the transform to locate the scrolled content.
    gfx::Size grid_size = GetTileGridSize();
    gfx::Vector2d update;
    if (pagination_controller_->scroll_axis() ==
        PaginationController::SCROLL_AXIS_HORIZONTAL) {
      const int page_width = grid_size.width() + GetPaddingBetweenPages();
      update.set_x(page_width * (new_selected - old_selected));
    } else {
      const int page_height = grid_size.height() + GetPaddingBetweenPages();
      update.set_y(page_height * (new_selected - old_selected));
    }
    drag_view_start_ += update;
    drag_view_->SetPosition(drag_view_->origin() + update);
    UpdateDropTargetRegion();
    Layout();
    MaybeStartPageFlipTimer(last_drag_point_);
  } else {
    // If |selected_view_| is no longer on the page, select the first item in
    // the page relative to the page swap in order to keep keyboard focus
    // movement predictable.
    if (selected_view_ && GetIndexOfView(selected_view_).page != new_selected) {
      GetViewAtIndex(
          GridIndex(new_selected, (old_selected < new_selected)
                                      ? 0
                                      : (GetItemsNumOfPage(new_selected) - 1)))
          ->RequestFocus();
    } else {
      ClearSelectedView(selected_view_);
    }
    Layout();
  }
}

void AppsGridView::TransitionStarting() {
  // Drag ends and animation starts.
  presentation_time_recorder_.reset();

  MaybeCreateGradientMask();
  CancelContextMenusOnCurrentPage();
}

void AppsGridView::TransitionStarted() {
  if (abs(pagination_model_.transition().target_page -
          pagination_model_.selected_page()) > 1) {
    Layout();
  }

  pagination_metrics_tracker_ =
      GetWidget()->GetCompositor()->RequestNewThroughputTracker();
  pagination_metrics_tracker_->Start(metrics_util::ForSmoothness(
      base::BindRepeating(&ReportPaginationSmoothness, IsTabletMode())));
}

void AppsGridView::TransitionChanged() {
  const PaginationModel::Transition& transition =
      pagination_model_.transition();
  if (!pagination_model_.is_valid_page(transition.target_page))
    return;

  // Sets the transform to locate the scrolled content.
  gfx::Size grid_size = GetTileGridSize();
  gfx::Vector2dF translate;
  const int dir =
      transition.target_page > pagination_model_.selected_page() ? -1 : 1;
  if (pagination_controller_->scroll_axis() ==
      PaginationController::SCROLL_AXIS_HORIZONTAL) {
    const int page_width = grid_size.width() + GetPaddingBetweenPages();
    translate.set_x(page_width * transition.progress * dir);
  } else {
    const int page_height = grid_size.height() + GetPaddingBetweenPages();
    translate.set_y(page_height * transition.progress * dir);
  }
  gfx::Transform transform;
  transform.Translate(translate);
  items_container_->layer()->SetTransform(transform);

  // |drag_view_| should stay in the same location in the screen, so makes
  // the opposite effect of the transform.
  if (drag_view_) {
    gfx::Transform drag_view_transform;
    drag_view_transform.Translate(-translate);
    drag_view_->layer()->SetTransform(drag_view_transform);
  }

  if (presentation_time_recorder_)
    presentation_time_recorder_->RequestNext();
}

void AppsGridView::TransitionEnded() {
  pagination_metrics_tracker_->Stop();

  // Gradient mask is no longer necessary once transition is finished.
  if (layer()->layer_mask_layer())
    layer()->SetMaskLayer(nullptr);
}

void AppsGridView::ScrollStarted() {
  DCHECK(!presentation_time_recorder_);

  MaybeCreateGradientMask();
  if (IsTabletMode()) {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        GetWidget()->GetCompositor(), kPageDragScrollInTabletHistogram,
        kPageDragScrollInTabletMaxLatencyHistogram);
  } else {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        GetWidget()->GetCompositor(), kPageDragScrollInClamshellHistogram,
        kPageDragScrollInClamshellMaxLatencyHistogram);
  }
}

void AppsGridView::ScrollEnded() {
  // Scroll can end without triggering state animation.
  presentation_time_recorder_.reset();
  // Need to reset the mask because transition will not happen in some
  // cases. (See https://crbug.com/1049275)
  layer()->SetMaskLayer(nullptr);
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

void AppsGridView::OnImplicitAnimationsCompleted() {
  if (layer()->opacity() == 0.0f)
    SetVisible(false);
  if (cardified_state_) {
    MaskContainerToBackgroundBounds();
    return;
  }
  RemoveAllBackgroundCards();
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

void AppsGridView::MaybeCallOnBoundsAnimatorDone() {
  --bounds_animation_for_cardified_state_in_progress_;
  if (bounds_animation_for_cardified_state_in_progress_ == 0)
    OnBoundsAnimatorDone(/*animator=*/nullptr);
}

GridIndex AppsGridView::GetNearestTileIndexForPoint(
    const gfx::Point& point) const {
  gfx::Rect bounds = GetContentsBounds();
  const int current_page = pagination_model_.selected_page();
  bounds.Inset(GetTilePadding());
  const gfx::Size total_tile_size = GetTotalTileSize();
  int col = base::ClampToRange(
      (point.x() - bounds.x()) / total_tile_size.width(), 0, cols_ - 1);
  int row =
      base::ClampToRange((point.y() - bounds.y()) / total_tile_size.height(), 0,
                         rows_per_page_ - 1);
  return GridIndex(current_page, row * cols_ + col);
}

gfx::Size AppsGridView::GetTileGridSize() const {
  gfx::Rect rect(GetTotalTileSize());
  rect.set_size(
      gfx::Size(rect.width() * cols_, rect.height() * rows_per_page_));
  rect.Inset(-GetTilePadding());
  return rect.size();
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
  if (cardified_state_) {
    //  In cardified state, add padding to center the apps grid within the
    //  contents.
    const gfx::Rect contents_bounds(GetContentsBounds());
    const gfx::Size tile_grid_size = GetTileGridSize();
    tile_bounds.Offset(
        (contents_bounds.width() - tile_grid_size.width()) / 2,
        (contents_bounds.height() - tile_grid_size.height()) / 2);
  }
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

  const int tiles_in_page0 = TilesPerPage(0);
  const int tiles_in_page1 = TilesPerPage(1);

  if (model_index < tiles_in_page0)
    return GridIndex(0, model_index);

  return GridIndex(1 + (model_index - tiles_in_page0) / tiles_in_page1,
                   (model_index - tiles_in_page0) % tiles_in_page1);
}

int AppsGridView::GetModelIndexFromIndex(const GridIndex& index) const {
  if (!folder_delegate_)
    return view_structure_.GetModelIndexFromIndex(index);

  if (index.page == 0)
    return index.slot;

  return TilesPerPage(0) + (index.page - 1) * TilesPerPage(1) + index.slot;
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

  return GridIndex(page, TilesPerPage(page) - 1);
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
         index.slot >= 0 && index.slot < TilesPerPage(index.page) &&
         GetModelIndexFromIndex(index) < view_model_.view_size();
}

bool AppsGridView::IsValidReorderTargetIndex(const GridIndex& index) const {
  if (!folder_delegate_)
    return view_structure_.IsValidReorderTargetIndex(index);

  return IsValidIndex(index);
}

bool AppsGridView::IsValidPageFlipTarget(int page) const {
  if (pagination_model_.is_valid_page(page))
    return true;

  // If the user wants to drag an app to the next new page and has not done so
  // during the dragging session, then it is the right target because a new page
  // will be created in OnPageFlipTimer().
  return !folder_delegate_ && !extra_page_opened_ &&
         pagination_model_.total_pages() == page;
}

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
  UMA_HISTOGRAM_COUNTS_100(kNumberOfPagesHistogram,
                           pagination_model_.total_pages());

  // Calculate the number of pages that have empty slots.
  int page_count = 0;
  if (!folder_delegate_) {
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

    // Only last page allows gaps if it is not full for folder.
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
  gfx::Size content_size = GetContentsBounds().size();
  const gfx::Size tile_size =
      GetTileViewSize(GetAppListConfig(), cardified_state_);
  if (cardified_state_)
    content_size = gfx::ScaleToRoundedSize(content_size, kCardifiedScale);

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
  if (page < 0 || page >= pagination_model_.total_pages())
    return 0;

  if (!folder_delegate_)
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
      GetMirroredRect(GetTargetIconRectInFolder(drag_item, folder_item_view));

  // Update folder icon.
  AppListFolderItem* folder_item =
      static_cast<AppListFolderItem*>(folder_item_view->item());
  folder_item->NotifyOfDraggedItem(drag_item);

  // Start animation.
  auto animation_view = std::make_unique<TopIconAnimationView>(
      this, drag_item->GetIcon(GetAppListConfig().type()), base::string16(),
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

  AnnounceFolderDrop(drag_view_->title()->GetText(),
                     drop_view->title()->GetText(), drop_view->is_folder());
}

void AppsGridView::AnnounceItemNotificationBadge(
    const base::string16& selected_view_title) {
  // Set a11y name to announce the notification badge for the focused item.
  auto* announcement_view =
      contents_view_->app_list_view()->announcement_view();
  announcement_view->GetViewAccessibility().OverrideName(
      l10n_util::GetStringFUTF16(IDS_APP_LIST_APP_FOCUS_NOTIFICATION_BADGE,
                                 selected_view_title));
  announcement_view->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}

void AppsGridView::AnnounceFolderDrop(const base::string16& moving_view_title,
                                      const base::string16& target_view_title,
                                      bool target_is_folder) {
  // Set a11y name to announce possible move to folder or creation of folder.
  auto* announcement_view =
      contents_view_->app_list_view()->announcement_view();
  announcement_view->GetViewAccessibility().OverrideName(
      l10n_util::GetStringFUTF16(
          target_is_folder
              ? IDS_APP_LIST_APP_DRAG_MOVE_TO_FOLDER_ACCESSIBILE_NAME
              : IDS_APP_LIST_APP_DRAG_CREATE_FOLDER_ACCESSIBILE_NAME,
          moving_view_title, target_view_title));
  announcement_view->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}

void AppsGridView::AnnounceKeyboardFoldering(
    const base::string16& moving_view_title,
    const base::string16& target_view_title,
    bool target_is_folder) {
  // Set a11y name to announce keyboard move to folder or creation of folder.
  auto* announcement_view =
      contents_view_->app_list_view()->announcement_view();
  announcement_view->GetViewAccessibility().OverrideName(
      l10n_util::GetStringFUTF16(
          target_is_folder
              ? IDS_APP_LIST_APP_KEYBOARD_MOVE_TO_FOLDER_ACCESSIBILE_NAME
              : IDS_APP_LIST_APP_KEYBOARD_CREATE_FOLDER_ACCESSIBILE_NAME,
          moving_view_title, target_view_title));
  announcement_view->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
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
  const int row =
      ((target_index.slot - (target_index.slot % cols_)) / cols_) + 1;
  const int col = (target_index.slot % cols_) + 1;
  const int page = target_index.page + 1;

  // Set the accessible name of the announcement view.
  auto* announcement_view =
      contents_view_->app_list_view()->announcement_view();
  announcement_view->GetViewAccessibility().OverrideName(
      l10n_util::GetStringFUTF16(
          IDS_APP_LIST_APP_DRAG_LOCATION_ACCESSIBILE_NAME,
          base::NumberToString16(page), base::NumberToString16(row),
          base::NumberToString16(col)));
  announcement_view->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
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

bool AppsGridView::ShouldHandleDragEvent(const ui::LocatedEvent& event) {
  // If |pagination_model_| is empty, don't handle scroll events.
  if (pagination_model_.total_pages() <= 0)
    return false;

  DCHECK(event.IsGestureEvent() || event.IsMouseEvent());

  // If the event is a scroll down in clamshell mode on the first page, don't
  // let |pagination_controller_| handle it. Unless it occurs in a folder.
  auto calculate_offset = [this](const ui::LocatedEvent& event) -> int {
    if (event.IsGestureEvent())
      return event.AsGestureEvent()->details().scroll_y_hint();
    gfx::PointF root_location = event.root_location_f();
    return root_location.y() - mouse_drag_start_point_.y();
  };
  if (!folder_delegate_ &&
      (event.IsMouseEvent() || event.type() == ui::ET_GESTURE_SCROLL_BEGIN) &&
      !contents_view_->app_list_view()->is_tablet_mode() &&
      pagination_model_.selected_page() == 0 && calculate_offset(event) > 0) {
    return false;
  }

  return true;
}

void AppsGridView::MaybeCreateGradientMask() {
  if (!folder_delegate_ && features::IsBackgroundBlurEnabled()) {
    // TODO(newcomer): Improve implementation of the mask layer so we can
    // enable it on all devices https://crbug.com/765292.
    if (!layer()->layer_mask_layer()) {
      // Always create a new layer. The layer may be recreated by animation,
      // and using the mask layer used by the detached layer can lead to
      // crash. b/118822974.
      if (!fadeout_layer_delegate_) {
        fadeout_layer_delegate_ = std::make_unique<FadeoutLayerDelegate>(
            GetAppListConfig().grid_fadeout_mask_height());
        fadeout_layer_delegate_->layer()->SetBounds(layer()->bounds());
      }
      layer()->SetMaskLayer(fadeout_layer_delegate_->layer());
    }
  }
}

int AppsGridView::GetPageFlipTargetForDrag(const gfx::Point& drag_point) {
  int new_page_flip_target = -1;

  // Drag zones are at the edges of the scroll axis.
  if (pagination_controller_->scroll_axis() ==
      PaginationController::SCROLL_AXIS_VERTICAL) {
    if (drag_point.y() <
        GetAppListConfig().page_flip_zone_size() + GetInsets().top()) {
      new_page_flip_target = pagination_model_.selected_page() - 1;
    } else if (IsPointWithinBottomDragBuffer(drag_point)) {
      // If the drag point is within the drag buffer, but not over the shelf.
      new_page_flip_target = pagination_model_.selected_page() + 1;
    }
  } else {
    // TODO(xiyuan): Fix this for RTL.
    if (new_page_flip_target == -1 &&
        drag_point.x() < GetAppListConfig().page_flip_zone_size())
      new_page_flip_target = pagination_model_.selected_page() - 1;

    if (new_page_flip_target == -1 &&
        drag_point.x() > width() - GetAppListConfig().page_flip_zone_size()) {
      new_page_flip_target = pagination_model_.selected_page() + 1;
    }
  }
  return new_page_flip_target;
}

void AppsGridView::SetHighlightedBackgroundCard(int new_highlighted_page) {
  if (!IsValidPageFlipTarget(new_highlighted_page))
    return;

  if (new_highlighted_page != highlighted_page_) {
    background_cards_[highlighted_page_]->SetColor(
        GetAppListConfig().GetCardifiedBackgroundColor(
            /*is_active=*/false));
    if (static_cast<int>(background_cards_.size()) == new_highlighted_page)
      AppendBackgroundCard();
    background_cards_[new_highlighted_page]->SetColor(
        GetAppListConfig().GetCardifiedBackgroundColor(/*is_active=*/true));

    highlighted_page_ = new_highlighted_page;
  }
}

}  // namespace ash
