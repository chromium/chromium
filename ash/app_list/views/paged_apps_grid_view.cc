// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/paged_apps_grid_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/apps_grid_row_change_animator.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/ghost_image_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/pagination/pagination_controller.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "cc/paint/paint_flags.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/view.h"
#include "ui/views/view_model_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Presentation time histogram for apps grid scroll by dragging.
constexpr char kPageDragScrollInTabletHistogram[] =
    "Apps.PaginationTransition.DragScroll.PresentationTime.TabletMode";
constexpr char kPageDragScrollInTabletMaxLatencyHistogram[] =
    "Apps.PaginationTransition.DragScroll.PresentationTime.MaxLatency."
    "TabletMode";

// Delay in milliseconds to do the page flip in fullscreen app list.
constexpr base::TimeDelta kPageFlipDelay = base::Milliseconds(500);

// Duration for page transition.
constexpr base::TimeDelta kPageTransitionDuration = base::Milliseconds(250);

// Duration for overscroll page transition.
constexpr base::TimeDelta kOverscrollPageTransitionDuration =
    base::Milliseconds(50);

// Vertical padding between the apps grid pages.
constexpr int kPaddingBetweenPages = 48;

// Vertical padding between the apps grid pages in cardified state.
constexpr int kCardifiedPaddingBetweenPages = 8;

// Horizontal padding of the apps grid page in cardified state.
constexpr int kCardifiedHorizontalPadding = 16;

// The radius of the corner of the background cards in the apps grid.
constexpr int kBackgroundCardCornerRadius = 16;

// The opacity for the background cards when hidden.
constexpr float kBackgroundCardOpacityHide = 0.0f;

// Animation curve used for entering and exiting cardified state.
constexpr gfx::Tween::Type kCardifiedStateTweenType =
    gfx::Tween::LINEAR_OUT_SLOW_IN;

// The minimum amount of space that should exist vertically between two app
// tiles in the root grid.
constexpr int kMinVerticalPaddingBetweenTiles = 8;

// The maximum amount of space that should exist vetically between two app
// tiles in the root grid.
constexpr int kMaxVerticalPaddingBetweenTiles = 96;

// The amount of vetical space between the edge of the background card and the
// closest app tile.
constexpr int kBackgroundCardVerticalPadding = 8;

// The amount of horizontal space between the edge of the background card and
// the closest app tile.
constexpr int kBackgroundCardHorizontalPadding = 16;

constexpr int kBackgroundCardBorderStrokeWidth = 1.0f;

// Apply `transform` to `bounds` at an origin of (0,0) so that the scaling
// part of the transform does not modify the position.
gfx::Rect ApplyTransformAtOrigin(gfx::Rect bounds, gfx::Transform transform) {
  gfx::Vector2d origin_offset = bounds.OffsetFromOrigin();

  gfx::RectF bounds_f =
      transform.MapRect(gfx::RectF(gfx::SizeF(bounds.size())));
  bounds_f.Offset(origin_offset);

  return gfx::ToRoundedRect(bounds_f);
}

}  // namespace

class PagedAppsGridView::BackgroundCardLayer : public ui::LayerOwner,
                                               public ui::LayerDelegate {
 public:
  explicit BackgroundCardLayer(PagedAppsGridView* paged_apps_grid_view)
      : LayerOwner(std::make_unique<ui::Layer>(ui::LAYER_TEXTURED)),
        paged_apps_grid_view_(paged_apps_grid_view) {
    layer()->SetFillsBoundsOpaquely(false);
    layer()->set_delegate(this);
  }

  BackgroundCardLayer(const BackgroundCardLayer&) = delete;
  BackgroundCardLayer& operator=(const BackgroundCardLayer&) = delete;
  ~BackgroundCardLayer() override = default;

  void SetIsActivePage(bool is_active_page) {
    is_active_page_ = is_active_page;
    layer()->SchedulePaint(gfx::Rect(layer()->size()));
  }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    ui::ColorProvider* color_provider =
        paged_apps_grid_view_->GetColorProvider();
    if (!color_provider) {
      return;
    }

    const gfx::Size size = layer()->size();
    ui::PaintRecorder recorder(context, size);
    gfx::Canvas* canvas = recorder.canvas();
    gfx::RectF card_size((gfx::SizeF(size)));

    // Draw a solid rounded rect as the background.
    cc::PaintFlags flags;
    if (is_active_page_) {
      flags.setColor(
          color_provider->GetColor(cros_tokens::kCrosSysRippleNeutralOnSubtle));
    } else {
      flags.setColor(
          color_provider->GetColor(cros_tokens::kCrosSysHoverOnSubtle));
    }
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawRoundRect(card_size, kBackgroundCardCornerRadius, flags);

    if (is_active_page_) {
      flags.setColor(color_provider->GetColor(cros_tokens::kCrosSysOutline));
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(kBackgroundCardBorderStrokeWidth);
      flags.setAntiAlias(true);
      card_size.Inset(gfx::InsetsF(kBackgroundCardBorderStrokeWidth / 2.0f));
      canvas->DrawRoundRect(card_size, kBackgroundCardCornerRadius, flags);
    }
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  bool is_active_page_ = false;

  const raw_ptr<PagedAppsGridView> paged_apps_grid_view_;
};

PagedAppsGridView::PagedAppsGridView(
    ContentsView* contents_view,
    AppListA11yAnnouncer* a11y_announcer,
    AppListFolderController* folder_controller,
    ContainerDelegate* container_delegate,
    AppListKeyboardController* keyboard_controller)
    : AppsGridView(a11y_announcer,
                   contents_view->GetAppListMainView()->view_delegate(),
                   /*folder_delegate=*/nullptr,
                   folder_controller,
                   keyboard_controller),
      contents_view_(contents_view),
      container_delegate_(container_delegate),
      page_flip_delay_(kPageFlipDelay) {
  DCHECK(contents_view_);

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  pagination_model_.SetTransitionDurations(kPageTransitionDuration,
                                           kOverscrollPageTransitionDuration);
  pagination_model_.AddObserver(this);

  pagination_controller_ = std::make_unique<PaginationController>(
      &pagination_model_, PaginationController::SCROLL_AXIS_VERTICAL,
      base::BindRepeating(&AppListRecordPageSwitcherSourceByEventType));

  GetViewAccessibility().SetClipsChildren(true);
}

PagedAppsGridView::~PagedAppsGridView() {
  pagination_model_.RemoveObserver(this);
}

void PagedAppsGridView::HandleScrollFromParentView(const gfx::Vector2d& offset,
                                                   ui::EventType type) {
  // If |pagination_model_| is empty, don't handle scroll events.
  if (pagination_model_.total_pages() <= 0)
    return;

  // Maybe switch pages.
  pagination_controller_->OnScroll(offset, type);
}

void PagedAppsGridView::SetMaxColumnsAndRows(int max_columns,
                                             int max_rows_on_first_page,
                                             int max_rows) {
  DCHECK_LE(max_rows_on_first_page, max_rows);
  const std::optional<int> first_page_size = TilesPerPage(0);
  const std::optional<int> default_page_size = TilesPerPage(1);

  max_rows_on_first_page_ = max_rows_on_first_page;
  max_rows_ = max_rows;
  SetMaxColumnsInternal(max_columns);

  // Update paging and pulsing blocks if the page sizes have changed.
  if (item_list() && (TilesPerPage(0) != first_page_size ||
                      TilesPerPage(1) != default_page_size)) {
    UpdatePaging();
    UpdatePulsingBlockViews();
    PreferredSizeChanged();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler:

void PagedAppsGridView::OnGestureEvent(ui::GestureEvent* event) {
  // Scroll begin events should not be passed to ancestor views from apps grid
  // in our current design. This prevents both ignoring horizontal scrolls in
  // app list, and closing open folders.
  if (pagination_controller_->OnGestureEvent(*event, GetContentsBounds()) ||
      event->type() == ui::EventType::kGestureScrollBegin) {
    event->SetHandled();
  }
}

////////////////////////////////////////////////////////////////////////////////
// views::View:

void PagedAppsGridView::Layout(PassKey) {
  if (ignore_layout())
    return;

  if (GetContentsBounds().IsEmpty())
    return;

  // Update cached tile padding first, as grid size calculations depend on the
  // cached padding value.
  UpdateTilePadding();

  // Prepare |page_size| * number-of-pages for |items_container_|, and sets the
  // origin properly to show the correct page.
  const gfx::Size page_size = GetPageSize();
  const int pages = pagination_model_.total_pages();
  const int current_page = pagination_model_.selected_page();
  const int page_height = page_size.height() + GetPaddingBetweenPages();
  items_container()->SetBoundsRect(gfx::Rect(0, -page_height * current_page,
                                             GetContentsBounds().width(),
                                             page_height * pages));

  CalculateIdealBounds();
  for (size_t i = 0; i < view_model()->view_size(); ++i) {
    AppListItemView* view = GetItemViewAt(i);
    view->SetBoundsRect(view_model()->ideal_bounds(i));
  }
  if (cardified_state_) {
    DCHECK(!background_cards_.empty());
    // Make sure that the background cards render behind everything
    // else in the items container.
    for (size_t i = 0; i < background_cards_.size(); ++i) {
      ui::Layer* const background_card = background_cards_[i]->layer();
      background_card->SetBounds(BackgroundCardBounds(i));
      items_container()->layer()->StackAtBottom(background_card);
    }
    MaskContainerToBackgroundBounds();
  }
  views::ViewModelUtils::SetViewBoundsToIdealBounds(pulsing_blocks_model());
}

void PagedAppsGridView::OnThemeChanged() {
  AppsGridView::OnThemeChanged();

  for (auto& card : background_cards_)
    card->layer()->SchedulePaint(gfx::Rect(card->layer()->size()));
}

////////////////////////////////////////////////////////////////////////////////
// AppsGridView:

gfx::Size PagedAppsGridView::GetTileViewSize() const {
  const AppListConfig* config = app_list_config();
  return gfx::ScaleToRoundedSize(
      gfx::Size(config->grid_tile_width(), config->grid_tile_height()),
      (cardified_state_ ? GetAppsGridCardifiedScale() : 1.0f));
}

gfx::Insets PagedAppsGridView::GetTilePadding(int page) const {
  return gfx::Insets::VH(
      page == 0 ? -first_page_vertical_tile_padding_ : -vertical_tile_padding_,
      -horizontal_tile_padding_);
}

gfx::Size PagedAppsGridView::GetTileGridSize() const {
  return GetTileGridSizeForPage(1);
}

int PagedAppsGridView::GetPaddingBetweenPages() const {
  return cardified_state_ ? kCardifiedPaddingBetweenPages +
                                2 * kBackgroundCardVerticalPadding
                          : kPaddingBetweenPages;
}

int PagedAppsGridView::GetTotalPages() const {
  return pagination_model_.total_pages();
}

int PagedAppsGridView::GetSelectedPage() const {
  return pagination_model_.selected_page();
}

bool PagedAppsGridView::IsPageFull(size_t page_index) const {
  const int first_page_size = *TilesPerPage(0);
  const int default_page_size = *TilesPerPage(1);
  const size_t last_page_index =
      first_page_size - 1 + page_index * default_page_size;
  size_t tiles = view_model()->view_size();
  return tiles > last_page_index;
}

GridIndex PagedAppsGridView::GetGridIndexFromIndexInViewModel(int index) const {
  const int first_page_size = *TilesPerPage(0);
  if (index < first_page_size)
    return GridIndex(0, index);
  const int default_page_size = *TilesPerPage(1);
  const int offset_from_first_page = index - first_page_size;
  return GridIndex(1 + (offset_from_first_page / default_page_size),
                   offset_from_first_page % default_page_size);
}

int PagedAppsGridView::GetNumberOfPulsingBlocksToShow(int item_count) const {
  const int tiles_on_first_page = *TilesPerPage(0);
  if (item_count < tiles_on_first_page)
    return tiles_on_first_page - item_count;

  const int tiles_per_page = *TilesPerPage(1);
  return tiles_per_page - (item_count - tiles_on_first_page) % tiles_per_page;
}

bool PagedAppsGridView::IsAnimatingCardifiedState() const {
  return is_animating_cardified_state_;
}

void PagedAppsGridView::MaybeStartCardifiedView() {
  if (!cardified_state_)
    StartAppsGridCardifiedView();
}

void PagedAppsGridView::MaybeEndCardifiedView() {
  if (cardified_state_)
    EndAppsGridCardifiedView();
}

bool PagedAppsGridView::MaybeStartPageFlip() {
  MaybeStartPageFlipTimer(last_drag_point());

  if (cardified_state_) {
    int hovered_page = GetPageFlipTargetForDrag(last_drag_point());
    if (hovered_page == -1)
      hovered_page = pagination_model_.selected_page();

    SetHighlightedBackgroundCard(hovered_page);
  }
  return page_flip_timer_.IsRunning();
}

void PagedAppsGridView::MaybeStopPageFlip() {
  StopPageFlipTimer();
}

bool PagedAppsGridView::MaybeAutoScroll() {
  // Paged view does not auto-scroll.
  return false;
}

void PagedAppsGridView::SetFocusAfterEndDrag(AppListItem* drag_item) {
  // Leave focus on the dragged item. Pressing tab or an arrow key will
  // highlight that item.
  AppListItemView* drag_view = GetItemViewAt(GetModelIndexOfItem(drag_item));
  if (drag_view)
    drag_view->SilentlyRequestFocus();
}

void PagedAppsGridView::RecordAppMovingTypeMetrics(AppListAppMovingType type) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppListAppMovingType", type,
                            kMaxAppListAppMovingType);
}

std::optional<int> PagedAppsGridView::GetMaxRowsInPage(int page) const {
  return page == 0 ? max_rows_on_first_page_ : max_rows_;
}

gfx::Vector2d PagedAppsGridView::GetGridCenteringOffset(int page) const {
  int y_offset = page == 0 ? GetTotalTopPaddingOnFirstPage() : 0;
  if (!cardified_state_)
    return gfx::Vector2d(0, y_offset);

  // The grid's y is set to start below the gradient mask, so subtract
  // this in centering the cardified state between gradient masks.
  y_offset -= margin_for_gradient_mask_;
  const gfx::Rect contents_bounds = GetContentsBounds();
  const gfx::Size grid_size = GetTileGridSizeForPage(page);
  return gfx::Vector2d(
      (contents_bounds.width() - grid_size.width()) / 2,
      (contents_bounds.height() + y_offset - grid_size.height()) / 2);
}

void PagedAppsGridView::UpdatePaging() {
  // The grid can have a different number of tiles per page.
  size_t tiles = view_model()->view_size();
  if (HasExtraSlotForReorderPlaceholder())
    ++tiles;
  int total_pages = 1;
  size_t tiles_on_page = *TilesPerPage(0);
  while (tiles > tiles_on_page) {
    tiles -= tiles_on_page;
    ++total_pages;
    tiles_on_page = *TilesPerPage(total_pages - 1);
  }
  pagination_model_.SetTotalPages(total_pages);
}

void PagedAppsGridView::RecordPageMetrics() {
  UMA_HISTOGRAM_COUNTS_100("Apps.NumberOfPages", GetTotalPages());
}

const gfx::Vector2d PagedAppsGridView::CalculateTransitionOffset(
    int page_of_view) const {
  // If there is a transition, calculates offset for current and target page.
  const int current_page = GetSelectedPage();
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

  const gfx::Size page_size = GetPageSize();
  const int page_height = page_size.height() + GetPaddingBetweenPages();
  return gfx::Vector2d(0, page_height * multiplier);
}

void PagedAppsGridView::EnsureViewVisible(const GridIndex& index) {
  if (pagination_model_.has_transition())
    return;

  if (IsValidIndex(index))
    pagination_model_.SelectPage(index.page, false);

  // The selected page will not get shown when ignoring layouts, which can
  // happen when ending drag and then exiting cardified state. Recenter the
  // items container to show the selected page in this case.
  if (ignore_layout_)
    RecenterItemsContainer();
}

std::optional<PagedAppsGridView::VisibleItemIndexRange>
PagedAppsGridView::GetVisibleItemIndexRange() const {
  // Expect that there is no active page transitions. Otherwise, the return
  // value can be obsolete.
  DCHECK(!pagination_model_.has_transition());

  const int selected_page = pagination_model_.selected_page();
  if (selected_page < 0)
    return std::nullopt;

  // Get the selected page's item count.
  const int on_page_item_count = GetNumberOfItemsOnPage(selected_page);

  // Return early if the selected page is empty.
  if (!on_page_item_count)
    return std::nullopt;

  // Calculate the index of the first view on the selected page.
  int start_view_index = 0;
  for (int page_index = 0; page_index < selected_page; ++page_index)
    start_view_index += GetNumberOfItemsOnPage(page_index);

  return VisibleItemIndexRange(start_view_index,
                               start_view_index + on_page_item_count - 1);
}

////////////////////////////////////////////////////////////////////////////////
// PaginationModelObserver:

void PagedAppsGridView::SelectedPageChanged(int old_selected,
                                            int new_selected) {
  items_container()->layer()->SetTransform(gfx::Transform());
  if (IsDragging()) {
    DeprecatedLayoutImmediately();
    UpdateDropTargetRegion();
    MaybeStartPageFlipTimer(last_drag_point());
  } else {
    // If the selected view is no longer on the page, select the first item in
    // the page relative to the page swap in order to keep keyboard focus
    // movement predictable.
    if (selected_view() &&
        GetIndexOfView(selected_view()).page != new_selected) {
      GridIndex new_index(new_selected,
                          (old_selected < new_selected)
                              ? 0
                              : (GetNumberOfItemsOnPage(new_selected) - 1));
      GetViewAtIndex(new_index)->RequestFocus();
    } else {
      ClearSelectedView();
    }
    DeprecatedLayoutImmediately();
  }
}

void PagedAppsGridView::TransitionStarting() {
  // Drag ends and animation starts.
  presentation_time_recorder_.reset();

  CancelContextMenusOnCurrentPage();
}

void PagedAppsGridView::TransitionStarted() {
  if (abs(pagination_model_.transition().target_page -
          pagination_model_.selected_page()) > 1) {
    DeprecatedLayoutImmediately();
  }

  pagination_metrics_tracker_ =
      GetWidget()->GetCompositor()->RequestNewThroughputTracker();
  pagination_metrics_tracker_->Start(metrics_util::ForSmoothnessV3(
      base::BindRepeating(&ReportPaginationSmoothness)));
}

void PagedAppsGridView::TransitionChanged() {
  const PaginationModel::Transition& transition =
      pagination_model_.transition();
  if (!pagination_model_.is_valid_page(transition.target_page))
    return;

  // Sets the transform to locate the scrolled content.
  const gfx::Size page_size = GetPageSize();
  gfx::Vector2dF translate;
  const int dir =
      transition.target_page > pagination_model_.selected_page() ? -1 : 1;
  const int page_height = page_size.height() + GetPaddingBetweenPages();
  translate.set_y(page_height * transition.progress * dir);

  gfx::Transform transform;
  transform.Translate(translate);
  items_container()->layer()->SetTransform(transform);

  if (presentation_time_recorder_)
    presentation_time_recorder_->RequestNext();
}

void PagedAppsGridView::TransitionEnded() {
  pagination_metrics_tracker_->Stop();
}

void PagedAppsGridView::ScrollStarted() {
  DCHECK(!presentation_time_recorder_);

  presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
      GetWidget()->GetCompositor(), kPageDragScrollInTabletHistogram,
      kPageDragScrollInTabletMaxLatencyHistogram);
}

void PagedAppsGridView::ScrollEnded() {
  // Scroll can end without triggering state animation.
  presentation_time_recorder_.reset();
}

bool PagedAppsGridView::ShouldContainerHandleDragEvents() {
  return true;
}

bool PagedAppsGridView::IsAboveTheFold(AppListItemView* item_view) {
  // The first page is considered above the fold.
  return GetIndexOfView(item_view).page == 0;
}

bool PagedAppsGridView::DoesIntersectRect(const views::View* target,
                                          const gfx::Rect& rect) const {
  gfx::Rect target_bounds(target->GetLocalBounds());
  if (GetSelectedPage() == 0) {
    // Allow events to pass to the continue section and recent apps.
    target_bounds.Inset(gfx::Insets::TLBR(first_page_offset_, 0, 0, 0));
  }
  return target_bounds.Intersects(rect);
}

bool PagedAppsGridView::FirePageFlipTimerForTest() {
  if (!page_flip_timer_.IsRunning())
    return false;
  page_flip_timer_.FireNow();
  return true;
}

gfx::Rect PagedAppsGridView::GetBackgroundCardBoundsForTesting(
    size_t card_index) {
  DCHECK_LT(card_index, background_cards_.size());
  gfx::Rect bounds_in_items_container = items_container()->GetMirroredRect(
      background_cards_[card_index]->layer()->bounds());
  gfx::Point origin_in_apps_grid = bounds_in_items_container.origin();
  views::View::ConvertPointToTarget(items_container(), this,
                                    &origin_in_apps_grid);
  return gfx::Rect(origin_in_apps_grid, bounds_in_items_container.size());
}

ui::Layer* PagedAppsGridView::GetBackgroundCardLayerForTesting(
    size_t card_index) const {
  DCHECK_LT(card_index, background_cards_.size());
  return background_cards_[card_index]->layer();
}

////////////////////////////////////////////////////////////////////////////////
// private:

gfx::Size PagedAppsGridView::GetPageSize() const {
  // Calculate page size as the tile grid size on non-leading page (which may
  // have reduced number of tiles to accommodate continue section and recent
  // apps UI in the apps container).
  // NOTE: The the actual page size is always the same. To get the size of just
  // app grid tiles on the leading page, GetTileGridSizeForPage(0) can be
  // called.
  return GetTileGridSizeForPage(1);
}

gfx::Size PagedAppsGridView::GetTileGridSizeForPage(int page) const {
  gfx::Rect rect(GetTotalTileSize(page));
  const int rows = *TilesPerPage(page) / cols();
  rect.set_size(gfx::Size(rect.width() * cols(), rect.height() * rows));
  rect.Inset(-GetTilePadding(page));
  return rect.size();
}

bool PagedAppsGridView::IsValidPageFlipTarget(int page) const {
  return pagination_model_.is_valid_page(page);
}

int PagedAppsGridView::GetPageFlipTargetForDrag(const gfx::Point& drag_point) {
  int new_page_flip_target = -1;

  // Drag zones are at the edges of the scroll axis.
  if (background_cards_.empty() ||
      !container_delegate_->IsPointWithinPageFlipBuffer(drag_point)) {
    return new_page_flip_target;
  }

  gfx::RectF background_card_rect_in_grid(
      background_cards_[GetSelectedPage()]->layer()->bounds());
  View::ConvertRectToTarget(items_container(), this,
                            &background_card_rect_in_grid);

  // Set page flip target when the drag is above or below the background
  // card of the currently selected page.
  if (drag_point.y() < background_card_rect_in_grid.y()) {
    new_page_flip_target = pagination_model_.selected_page() - 1;
  } else if (drag_point.y() > background_card_rect_in_grid.bottom()) {
    new_page_flip_target = pagination_model_.selected_page() + 1;
  }
  return new_page_flip_target;
}

void PagedAppsGridView::MaybeStartPageFlipTimer(const gfx::Point& drag_point) {
  if (!container_delegate_->IsPointWithinPageFlipBuffer(drag_point))
    StopPageFlipTimer();
  int new_page_flip_target = GetPageFlipTargetForDrag(drag_point);

  if (new_page_flip_target == page_flip_target_)
    return;

  StopPageFlipTimer();
  if (IsValidPageFlipTarget(new_page_flip_target)) {
    page_flip_target_ = new_page_flip_target;

    if (page_flip_target_ != pagination_model_.selected_page()) {
      page_flip_timer_.Start(FROM_HERE, page_flip_delay_, this,
                             &PagedAppsGridView::OnPageFlipTimer);
    }
  }
}

void PagedAppsGridView::OnPageFlipTimer() {
  DCHECK(IsValidPageFlipTarget(page_flip_target_));

  if (pagination_model_.total_pages() == page_flip_target_) {
    // Create a new page because the user requests to put an item to a new page.
    extra_page_opened_ = true;
    pagination_model_.SetTotalPages(pagination_model_.total_pages() + 1);
  }

  pagination_model_.SelectPage(page_flip_target_, true);
  if (!IsInFolder())
    RecordPageSwitcherSource(kDragAppToBorder);

  BeginHideCurrentGhostImageView();
}

void PagedAppsGridView::StopPageFlipTimer() {
  page_flip_timer_.Stop();
  page_flip_target_ = -1;
}

void PagedAppsGridView::MaybeAbortExistingCardifiedAnimations() {
  if (cardified_animation_abort_handle_) {
    cardified_animation_abort_handle_.reset();
  }
}

void PagedAppsGridView::StartAppsGridCardifiedView() {
  if (IsInFolder())
    return;
  DCHECK(!cardified_state_);
  MaybeAbortExistingCardifiedAnimations();
  RemoveAllBackgroundCards();
  // Calculate background bounds for a normal grid so it animates from the
  // normal to the cardified bounds with the icons.
  for (int i = 0; i < pagination_model_.total_pages(); i++)
    AppendBackgroundCard();
  cardified_state_ = true;
  UpdateTilePadding();
  container_delegate_->OnCardifiedStateStarted();
  AnimateCardifiedState();
}

void PagedAppsGridView::EndAppsGridCardifiedView() {
  if (IsInFolder())
    return;
  DCHECK(cardified_state_);
  MaybeAbortExistingCardifiedAnimations();
  cardified_state_ = false;
  // Update the padding between tiles, so we can animate back the apps grid
  // elements to their original positions.
  UpdateTilePadding();
  AnimateCardifiedState();
  layer()->SetClipRect(gfx::Rect());
}

void PagedAppsGridView::AnimateCardifiedState() {
  if (GetWidget()) {
    // Normally layout cancels any animations. At this point there may be a
    // pending layout; force it now so that one isn't triggered part way through
    // the animation. Further, ignore this layout so that the position isn't
    // reset.
    DCHECK(!ignore_layout_);
    base::AutoReset<bool> auto_reset(&ignore_layout_, true);
    GetWidget()->LayoutRootViewIfNecessary();
  }
  is_animating_cardified_state_ = true;

  // Resizing of AppListItemView icons can invalidate layout and cause a layout
  // while exiting cardified state. Keep ignoring layouts when exiting
  // cardified state, so that any row change animations that need to take place
  // while exiting do not get interrupted by a layout.
  if (!cardified_state_)
    ignore_layout_ = true;

  CalculateIdealBounds();

  // Cache the current item container position, as RecenterItemsContainer() may
  // change it.
  gfx::Point start_position = items_container()->origin();
  RecenterItemsContainer();

  gfx::Vector2d translate_offset(
      0, start_position.y() - items_container()->origin().y());

  // Create background card animation metric reporters.
  std::vector<std::unique_ptr<ui::AnimationThroughputReporter>> reporters;
  for (auto& background_card : background_cards_) {
    reporters.push_back(std::make_unique<ui::AnimationThroughputReporter>(
        background_card->layer()->GetAnimator(),
        metrics_util::ForSmoothnessV3(base::BindRepeating(
            &ReportCardifiedSmoothness, cardified_state_))));
  }

  views::AnimationBuilder animations;
  cardified_animation_abort_handle_ = animations.GetAbortHandle();
  animations
      .OnEnded(base::BindOnce(&PagedAppsGridView::OnCardifiedStateAnimationDone,
                              weak_ptr_factory_.GetWeakPtr()))
      .OnAborted(
          base::BindOnce(&PagedAppsGridView::OnCardifiedStateAnimationDone,
                         weak_ptr_factory_.GetWeakPtr()))
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::Milliseconds(kDefaultAnimationDuration));

  AnimateAppListItemsForCardifiedState(&animations.GetCurrentSequence(),
                                       translate_offset);

  if (current_ghost_view_) {
    auto index = current_ghost_view_->index();
    gfx::Rect current_bounds = current_ghost_view_->bounds();
    current_bounds.Offset(translate_offset);
    gfx::Rect target_bounds = GetExpectedTileBounds(index);
    target_bounds.Offset(CalculateTransitionOffset(index.page));

    current_ghost_view_->SetBoundsRect(target_bounds);
    gfx::Transform transform = gfx::TransformBetweenRects(
        gfx::RectF(items_container()->GetMirroredRect(target_bounds)),
        gfx::RectF(items_container()->GetMirroredRect(current_bounds)));
    current_ghost_view_->layer()->SetTransform(transform);

    animations.GetCurrentSequence().SetTransform(current_ghost_view_->layer(),
                                                 gfx::Transform(),
                                                 kCardifiedStateTweenType);
  }

  for (size_t i = 0; i < background_cards_.size(); i++) {
    ui::Layer* const background_card = background_cards_[i]->layer();
    // Reposition card bounds to compensate for the translation offset.
    gfx::Rect background_bounds = background_card->bounds();
    background_bounds.Offset(translate_offset);
    background_card->SetBounds(background_bounds);
    if (cardified_state_) {
      const bool is_active_page =
          static_cast<int>(i) == pagination_model_.selected_page();
      background_cards_[i]->SetIsActivePage(is_active_page);
    } else {
      animations.GetCurrentSequence().SetOpacity(background_card,
                                                 kBackgroundCardOpacityHide);
    }
    animations.GetCurrentSequence().SetBounds(
        background_card, BackgroundCardBounds(i), kCardifiedStateTweenType);
  }
  highlighted_page_ = pagination_model_.selected_page();
}

void PagedAppsGridView::AnimateAppListItemsForCardifiedState(
    views::AnimationSequenceBlock* animation_sequence,
    const gfx::Vector2d& translate_offset) {
  // Check that at least one item needs animating to avoid creating an animation
  // builder when no views need animating.
  bool items_need_animating = false;
  for (size_t i = 0; i < view_model()->view_size(); ++i) {
    AppListItemView* item_view = view_model()->view_at(i);
    if (!IsViewExplicitlyHidden(item_view)) {
      items_need_animating = true;
      break;
    }
  }

  if (!items_need_animating)
    return;

  for (size_t i = 0; i < view_model()->view_size(); ++i) {
    AppListItemView* entry_view = view_model()->view_at(i);
    // Reposition view bounds to compensate for the translation offset.
    gfx::Rect current_bounds =
        items_container()->GetMirroredRect(entry_view->bounds());
    current_bounds.Offset(translate_offset);

    if (entry_view->layer()) {
      // Apply the current layer transform to current bounds, for the case where
      // `entry_view` is already transformed by a layer animation.
      current_bounds = ApplyTransformAtOrigin(current_bounds,
                                              entry_view->layer()->transform());
    }

    entry_view->EnsureLayer();

    if (cardified_state_)
      entry_view->EnterCardifyState();
    else
      entry_view->ExitCardifyState();

    gfx::Rect target_bounds(view_model()->ideal_bounds(i));

    if (entry_view->has_pending_row_change()) {
      entry_view->reset_has_pending_row_change();
      row_change_animator_->AnimateBetweenRows(
          entry_view, current_bounds, target_bounds, animation_sequence);
      continue;
    }

    entry_view->SetBoundsRect(target_bounds);

    // Skip animating the item view if it is already hidden.
    if (IsViewExplicitlyHidden(entry_view))
      continue;

    // View bounds are currently |target_bounds|. Transform the view so it
    // appears in |current_bounds|. Note that bounds are flipped by views in
    // RTL UI direction, which is not taken into account by
    // `gfx::TransformBetweenRects()` - use mirrored rects to calculate
    // transition transform when needed.
    gfx::Transform transform = gfx::TransformBetweenRects(
        gfx::RectF(items_container()->GetMirroredRect(target_bounds)),
        gfx::RectF(current_bounds));
    entry_view->layer()->SetTransform(transform);

    animation_sequence->SetTransform(entry_view->layer(), gfx::Transform(),
                                     kCardifiedStateTweenType);
  }
}

void PagedAppsGridView::OnCardifiedStateAnimationDone() {
  is_animating_cardified_state_ = false;

  DestroyLayerItemsIfNotNeeded();

  if (layer()->opacity() == 0.0f)
    SetVisible(false);

  if (cardified_state_) {
    MaskContainerToBackgroundBounds();
  } else {
    OnCardifiedStateEnded();
  }
}

void PagedAppsGridView::OnCardifiedStateEnded() {
  ignore_layout_ = false;
  RemoveAllBackgroundCards();

  if (cardified_state_ended_test_callback_)
    cardified_state_ended_test_callback_.Run();
  container_delegate_->OnCardifiedStateEnded();
}

void PagedAppsGridView::RecenterItemsContainer() {
  const int pages = pagination_model_.total_pages();
  const int current_page = pagination_model_.selected_page();
  const int page_height = GetPageSize().height() + GetPaddingBetweenPages();
  items_container()->SetBoundsRect(gfx::Rect(0, -page_height * current_page,
                                             GetContentsBounds().width(),
                                             page_height * pages));
}

gfx::Rect PagedAppsGridView::BackgroundCardBounds(int new_page_index) {
  // The size of the grid excluding the outer padding.
  const gfx::Size grid_size = GetTileGridSizeForPage(new_page_index);

  // The size for the background card that will be displayed. The outer padding
  // of the grid needs to be added.
  gfx::Size background_card_size;
  background_card_size =
      grid_size + gfx::Size(2 * kBackgroundCardHorizontalPadding,
                            2 * kBackgroundCardVerticalPadding);

  // Add a padding on the sides to make space for pagination preview, but make
  // sure the padding doesn't exceed the tile padding (otherwise the background
  // bounds would clip the apps grid bounds).
  const int horizontal_padding =
      (GetContentsBounds().width() - background_card_size.width()) / 2;

  int y_offset = new_page_index == 0 ? GetTotalTopPaddingOnFirstPage() : 0;
  // Subtract the amount that the apps grid view is offset by to accommodate
  // for the top gradient mask. This will visually center the background card
  // between the top and bottom gradient masks.
  y_offset -= margin_for_gradient_mask_;

  int vertical_padding = y_offset + (GetContentsBounds().height() - y_offset -
                                     background_card_size.height()) /
                                        2;

  const int padding_between_pages = GetPaddingBetweenPages();
  // The space that each page occupies in the items container. This is the size
  // of the grid without outer padding plus the padding between pages.
  const int total_page_height = GetPageSize().height() + padding_between_pages;
  // We position a new card in the last place in items container view.
  const int vertical_page_start_offset = total_page_height * new_page_index;

  return gfx::Rect(horizontal_padding,
                   vertical_padding + vertical_page_start_offset,
                   background_card_size.width(), background_card_size.height());
}

void PagedAppsGridView::AppendBackgroundCard() {
  background_cards_.push_back(std::make_unique<BackgroundCardLayer>(this));
  ui::Layer* current_layer = background_cards_.back()->layer();
  current_layer->SetBounds(BackgroundCardBounds(background_cards_.size() - 1));
  current_layer->SetVisible(true);
  items_container()->layer()->Add(current_layer);
}

void PagedAppsGridView::RemoveBackgroundCard() {
  items_container()->layer()->Remove(background_cards_.back()->layer());
  background_cards_.pop_back();
}

void PagedAppsGridView::MaskContainerToBackgroundBounds() {
  DCHECK(!background_cards_.empty());
  const gfx::Rect background_card_bounds =
      background_cards_[0]->layer()->bounds();
  // Mask apps grid container layer to the background card width. Optionally
  // also include extra height to ensure the top gradient mask is shown as well.
  layer()->SetClipRect(
      gfx::Rect(background_card_bounds.x(), -margin_for_gradient_mask_,
                background_card_bounds.width(),
                layer()->bounds().height() + margin_for_gradient_mask_));
}

void PagedAppsGridView::RemoveAllBackgroundCards() {
  for (auto& card : background_cards_)
    items_container()->layer()->Remove(card->layer());
  background_cards_.clear();
}

void PagedAppsGridView::SetHighlightedBackgroundCard(int new_highlighted_page) {
  if (!IsValidPageFlipTarget(new_highlighted_page))
    return;

  if (new_highlighted_page != highlighted_page_) {
    background_cards_[highlighted_page_]->SetIsActivePage(false);
    if (static_cast<int>(background_cards_.size()) == new_highlighted_page)
      AppendBackgroundCard();
    background_cards_[new_highlighted_page]->SetIsActivePage(true);

    highlighted_page_ = new_highlighted_page;
  }
}

void PagedAppsGridView::UpdateTilePadding() {
  if (has_fixed_tile_padding_) {
    // With fixed padding, all padding should remain constant. Set
    // 'first_page_vertical_tile_padding_' here because it is unique to this
    // class.
    first_page_vertical_tile_padding_ = vertical_tile_padding_;
    return;
  }

  gfx::Size content_size = GetContentsBounds().size();
  const gfx::Size tile_size = GetTileViewSize();
  if (cardified_state_) {
    content_size =
        gfx::ScaleToRoundedSize(content_size, GetAppsGridCardifiedScale()) -
        gfx::Size(2 * kCardifiedHorizontalPadding, 0);
    content_size.set_width(
        std::max(content_size.width(), cols() * tile_size.width()));
    content_size.set_height(
        std::max(content_size.height(), max_rows_ * tile_size.height()));
  }

  const auto calculate_tile_padding = [](int content_size, int num_tiles,
                                         int tile_size, int offset) -> int {
    return num_tiles > 1 ? (content_size - offset - num_tiles * tile_size) /
                               ((num_tiles - 1) * 2)
                         : 0;
  };

  // Item tiles should be evenly distributed in this view.
  horizontal_tile_padding_ = calculate_tile_padding(
      content_size.width(), cols(), tile_size.width(), 0);

  // Calculate padding for default page size, to ensure the spacing between
  // pages remains the same when the selected page changes from/to the first
  // page.
  vertical_tile_padding_ = calculate_tile_padding(
      content_size.height(), max_rows_, tile_size.height(), 0);

  // NOTE: The padding on the first page can be different than other pages
  // depending on `first_page_offset_` and `max_rows_on_first_page_`.
  // When shown under recent apps, assume an extra row when calculating padding,
  // as an extra leading tile padding will get added above the first row of apps
  // (as a margin to recent apps container).
  // Adjust `first_page_offset_` by removing space required for recent apps
  // (assumes that recent apps tile size matches the apps grid tile size, and
  // that recent apps container has a single row of apps) so padding is
  // calculated assuming recent apps container is part of the apps grid.
  if (shown_under_recent_apps_) {
    first_page_vertical_tile_padding_ = calculate_tile_padding(
        content_size.height(), max_rows_on_first_page_ + 1, tile_size.height(),
        std::max(0, first_page_offset_ - tile_size.height()));
  } else {
    first_page_vertical_tile_padding_ =
        calculate_tile_padding(content_size.height(), max_rows_on_first_page_,
                               tile_size.height(), first_page_offset_);
  }

  if (!cardified_state_ && unscaled_first_page_vertical_tile_padding_ !=
                               first_page_vertical_tile_padding_) {
    unscaled_first_page_vertical_tile_padding_ =
        first_page_vertical_tile_padding_;
  }
}

bool PagedAppsGridView::ConfigureFirstPagePadding(
    int offset,
    bool shown_under_recent_apps) {
  if (offset == first_page_offset_ &&
      shown_under_recent_apps == shown_under_recent_apps_) {
    return false;
  }
  first_page_offset_ = offset;
  shown_under_recent_apps_ = shown_under_recent_apps;
  return true;
}

int PagedAppsGridView::CalculateFirstPageMaxRows(int available_height,
                                                 int preferred_rows) {
  // When shown under recent apps, calculate max rows as if recent apps
  // container is part of the grid, i.e. calculate number of rows as if grid
  // allows for an extra row of apps.
  const int space_for_recent_apps =
      shown_under_recent_apps_ ? app_list_config()->grid_tile_height() : 0;
  const int max_rows = CalculateMaxRows(
      available_height -
          std::max(0, first_page_offset_ - space_for_recent_apps),
      preferred_rows);
  // If `shown_under_recent_apps_`, subtract a row from the result of
  // `CalculateMaxRows()` which was calculated assuming there's an extra row of
  // apps added for recent apps.
  return shown_under_recent_apps_ ? std::max(0, max_rows - 1) : max_rows;
}

int PagedAppsGridView::CalculateMaxRows(int available_height,
                                        int preferred_rows) {
  if (!app_list_config())
    return preferred_rows;

  const int tile_height = app_list_config()->grid_tile_height();
  const int padding_for_preferred_rows =
      (available_height - preferred_rows * tile_height) / (preferred_rows - 1);
  int final_row_count = preferred_rows;

  if (padding_for_preferred_rows < kMinVerticalPaddingBetweenTiles) {
    // The padding with the preferred number of rows is too small. So find the
    // max number of rows which will fit within the available space.
    // I.e. max n where:
    // n * tile_height + (n - 1) * min_padding <= available_height
    final_row_count = (available_height + kMinVerticalPaddingBetweenTiles) /
                      (tile_height + kMinVerticalPaddingBetweenTiles);

  } else if (padding_for_preferred_rows > kMaxVerticalPaddingBetweenTiles) {
    // The padding with the preferred number of rows is too large. So find the
    // min number of rows which will fit within the available space.
    // I.e. min n, with padding as close to max as possible where:
    // n* tile_height + (n - 1) * padding <= available_height
    // padding <= max_padding
    final_row_count = std::ceil(
        static_cast<float>(available_height + kMaxVerticalPaddingBetweenTiles) /
        (tile_height + kMaxVerticalPaddingBetweenTiles));
  }
  // Unit tests may create artificially small screens resulting in
  // `final_row_count` of 0. Return 1 row to avoid divide-by-zero in layout.
  return std::max(final_row_count, 1);
}

void PagedAppsGridView::AnimateOnNudgeRemoved() {
  UpdateTilePadding();

  if (GetWidget()) {
    // Normally layout cancels any animations. At this point there may be a
    // pending layout; force it now so that one isn't triggered part way through
    // the animation. Further, ignore this layout so that the position isn't
    // reset.
    base::AutoReset<bool> auto_reset(&ignore_layout_, true);
    GetWidget()->LayoutRootViewIfNecessary();
  }

  PrepareItemsForBoundsAnimation();
  AnimateToIdealBounds(/*top to bottom animation=*/true);
}

void PagedAppsGridView::SetCardifiedStateEndedTestCallback(
    base::RepeatingClosure cardified_ended_callback) {
  cardified_state_ended_test_callback_ = std::move(cardified_ended_callback);
}

int PagedAppsGridView::GetTotalTopPaddingOnFirstPage() const {
  // Add the page offset that accommodates continue section content, and if
  // shown under recent apps, additional tile padding above the first row of
  // apps.
  return first_page_offset_ +
         (shown_under_recent_apps_ ? 2 * first_page_vertical_tile_padding_ : 0);
}

void PagedAppsGridView::StackCardsAtBottom() {
  for (size_t i = 0; i < background_cards_.size(); ++i) {
    items_container()->layer()->StackAtBottom(background_cards_[i]->layer());
  }
}

BEGIN_METADATA(PagedAppsGridView)
END_METADATA

}  // namespace ash
