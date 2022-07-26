// Copyright 2021 The Chromium Authors. All rights reserved.
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
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/ghost_image_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/pagination/pagination_controller.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/cxx17_backports.h"
#include "base/metrics/histogram_macros.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/view.h"
#include "ui/views/view_model_utils.h"

namespace ash {
namespace {

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

// Delay in milliseconds to do the page flip in fullscreen app list.
constexpr base::TimeDelta kPageFlipDelay = base::Milliseconds(500);

// Duration for page transition.
constexpr base::TimeDelta kPageTransitionDuration = base::Milliseconds(250);

// The size of the zone within which app list item drag events will trigger a
// page flip.
constexpr int kPageFlipZoneSize = 20;

// The height of the fadeout mask at vertical edges of the apps grid view.
constexpr int kDefaultFadeoutMaskHeight = 16;

// Duration for overscroll page transition.
constexpr base::TimeDelta kOverscrollPageTransitionDuration =
    base::Milliseconds(50);

// Vertical padding between the apps grid pages.
constexpr int kPaddingBetweenPages = 48;

// Vertical padding between the apps grid pages in cardified state.
constexpr int kCardifiedPaddingBetweenPages = 12;
constexpr int kCardifiedPaddingBetweenPagesProdLauncher = 8;

// Horizontal padding of the apps grid page in cardified state.
constexpr int kCardifiedHorizontalPadding = 16;

// The radius of the corner of the background cards in the apps grid.
constexpr int kBackgroundCardCornerRadius = 12;
constexpr int kBackgroundCardCornerRadiusProdLauncher = 16;

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

int GetFadeoutMaskHeight() {
  // The fadeout mask layer is shown only if background blur is enabled - if
  // fadeout mask is not shown, return 0 here so the apps grid is not shown in
  // the fadeout zone during drag.
  return features::IsBackgroundBlurEnabled() ? kDefaultFadeoutMaskHeight : 0;
}

}  // namespace

class PagedAppsGridView::BackgroundCardLayer : public ui::Layer,
                                               public ui::LayerDelegate {
 public:
  BackgroundCardLayer() : Layer(ui::LAYER_TEXTURED) {
    SetFillsBoundsOpaquely(false);
    set_delegate(this);
  }

  BackgroundCardLayer(const BackgroundCardLayer&) = delete;
  BackgroundCardLayer& operator=(const BackgroundCardLayer&) = delete;
  ~BackgroundCardLayer() override = default;

  void SetIsActivePage(bool is_active_page) {
    is_active_page_ = is_active_page;
    SchedulePaint(parent()->bounds());
  }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, size());
    gfx::Canvas* canvas = recorder.canvas();
    gfx::RectF card_size((gfx::SizeF(size())));
    const int corner_radius = features::IsProductivityLauncherEnabled()
                                  ? kBackgroundCardCornerRadiusProdLauncher
                                  : kBackgroundCardCornerRadius;

    // Draw a solid rounded rect as the background.
    cc::PaintFlags flags;
    auto* color_provider = AppListColorProvider::Get();
    SkColor fill_color =
        is_active_page_ ? color_provider->GetGridBackgroundCardActiveColor()
                        : color_provider->GetGridBackgroundCardInactiveColor();
    flags.setColor(fill_color);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    canvas->DrawRoundRect(card_size, corner_radius, flags);

    if (features::IsProductivityLauncherEnabled() && is_active_page_) {
      // Draw a border around the active page.
      const bool dark_mode =
          DarkLightModeControllerImpl::Get()->IsDarkModeEnabled();
      flags.setColor(dark_mode ? SK_ColorWHITE : SK_ColorBLACK);
      flags.setAlpha(dark_mode ? 0x29 /*16%*/ : 0x1F /*12%*/);
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(kBackgroundCardBorderStrokeWidth);
      flags.setAntiAlias(true);
      card_size.Inset(gfx::InsetsF(kBackgroundCardBorderStrokeWidth / 2.0f));
      canvas->DrawRoundRect(card_size, corner_radius, flags);
    }
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  bool is_active_page_ = false;
};

PagedAppsGridView::PagedAppsGridView(
    ContentsView* contents_view,
    AppListA11yAnnouncer* a11y_announcer,
    AppsGridViewFolderDelegate* folder_delegate,
    AppListFolderController* folder_controller,
    ContainerDelegate* container_delegate,
    AppListKeyboardController* keyboard_controller)
    : AppsGridView(a11y_announcer,
                   contents_view->GetAppListMainView()->view_delegate(),
                   folder_delegate,
                   folder_controller,
                   keyboard_controller),
      contents_view_(contents_view),
      container_delegate_(container_delegate),
      page_flip_delay_(kPageFlipDelay),
      is_productivity_launcher_enabled_(
          features::IsProductivityLauncherEnabled()) {
  DCHECK(contents_view_);

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  view_structure_.Init(
      (IsInFolder() || features::IsProductivityLauncherEnabled())
          ? PagedViewStructure::Mode::kFullPages
          : PagedViewStructure::Mode::kPartialPages);

  pagination_model_.SetTransitionDurations(kPageTransitionDuration,
                                           kOverscrollPageTransitionDuration);
  pagination_model_.AddObserver(this);

  pagination_controller_ = std::make_unique<PaginationController>(
      &pagination_model_,
      IsInFolder() ? PaginationController::SCROLL_AXIS_HORIZONTAL
                   : PaginationController::SCROLL_AXIS_VERTICAL,
      IsInFolder()
          ? base::DoNothing()
          : base::BindRepeating(&AppListRecordPageSwitcherSourceByEventType),
      IsTabletMode());
}

PagedAppsGridView::~PagedAppsGridView() {
  pagination_model_.RemoveObserver(this);
}

void PagedAppsGridView::OnTabletModeChanged(bool started) {
  pagination_controller_->set_is_tablet_mode(started);

  // Enable/Disable folder icons's background blur based on tablet mode.
  for (const auto& entry : view_model()->entries()) {
    auto* item_view = static_cast<AppListItemView*>(entry.view);
    if (item_view->item() && item_view->item()->is_folder())
      item_view->SetBackgroundBlurEnabled(started);
  }

  // Prevent context menus from remaining open after a transition
  CancelContextMenusOnCurrentPage();

  // Abort the running reorder animation when the tablet mode updates.
  MaybeAbortWholeGridAnimation();
}

void PagedAppsGridView::HandleScrollFromParentView(const gfx::Vector2d& offset,
                                                   ui::EventType type) {
  // If |pagination_model_| is empty, don't handle scroll events.
  if (pagination_model_.total_pages() <= 0)
    return;

  // Maybe switch pages.
  pagination_controller_->OnScroll(offset, type);
}

void PagedAppsGridView::UpdateOpacity(bool restore_opacity,
                                      float apps_opacity_change_start,
                                      float apps_opacity_change_end) {
  if (view_structure_.pages().empty())
    return;

  // Do not update opacity when reorder animation is running.
  if (IsUnderWholeGridAnimation())
    return;

  // Return early if the opacity is locked.
  if (lock_opacity_)
    return;

  // App list view state animations animate the apps grid view opacity rather
  // than individual items' opacity. This method (used during app list view
  // drag) sets up opacity for individual grid item, and assumes that the apps
  // grid view is fully opaque.
  layer()->SetOpacity(1.0f);

  // First it should prepare the layers for all of the app items in the current
  // page when necessary, or destroy all of the layers when they become
  // unnecessary. Do not dynamically ensure/destroy layers of individual items
  // since the creation/destruction of the layer requires to repaint the parent
  // view (i.e. this class).
  if (restore_opacity) {
    // If item layers are still required (e.g. during drag), only update the
    // opacity (the layers will be deleted when they are no longer needed).
    if (ItemViewsRequireLayers()) {
      for (const auto& entry : view_model()->entries()) {
        if (!IsViewExplicitlyHidden(entry.view) && entry.view->layer())
          entry.view->layer()->SetOpacity(1.0f);
      }
      return;
    }

    // Layers are not necessary. Destroy them, and return. No need to update
    // opacity. This needs to be done on all views within |view_model_| because
    // some item view might have been moved out from the current page. See also
    // https://crbug.com/990529.
    for (const auto& entry : view_model()->entries())
      entry.view->DestroyLayer();
    return;
  }

  // Updates the opacity of the apps in current page. The opacity of the app
  // starting at 0.f when the centerline of the app is |kAllAppsOpacityStartPx|
  // above the bottom of work area and transitioning to 1.0f by the time the
  // centerline reaches |kAllAppsOpacityEndPx| above the work area bottom.
  AppListView* app_list_view = contents_view_->app_list_view();
  const int selected_page = pagination_model_.selected_page();

  // `pagination_model_` may have an extra page during app list item drag (if
  // the user is trying to add the app to a new page).
  if (selected_page == static_cast<int>(view_structure_.pages().size())) {
    CHECK(extra_page_opened_);
    return;
  }

  auto current_page = view_structure_.pages()[selected_page];

  // Ensure layers and update their opacity.
  for (AppListItemView* item_view : current_page)
    item_view->EnsureLayer();

  float centerline_above_work_area = 0.f;
  float opacity = 0.f;
  for (size_t i = 0; i < current_page.size(); i += cols()) {
    AppListItemView* item_view = current_page[i];
    gfx::Rect view_bounds = item_view->GetLocalBounds();
    views::View::ConvertRectToScreen(item_view, &view_bounds);
    centerline_above_work_area = std::max<float>(
        app_list_view->GetScreenBottom() - view_bounds.CenterPoint().y(), 0.f);
    opacity =
        base::clamp((centerline_above_work_area - apps_opacity_change_start) /
                        (apps_opacity_change_end - apps_opacity_change_start),
                    0.f, 1.0f);

    if (opacity == item_view->layer()->opacity())
      continue;

    const size_t end_index = std::min(current_page.size() - 1, i + cols() - 1);
    for (size_t j = i; j <= end_index; ++j) {
      if (!IsViewHiddenForDrag(current_page[j]))
        current_page[j]->layer()->SetOpacity(opacity);
    }
  }
}

void PagedAppsGridView::SetMaxColumnsAndRows(int max_columns,
                                             int max_rows_on_first_page,
                                             int max_rows) {
  DCHECK_LE(max_rows_on_first_page, max_rows);
  const int first_page_size = TilesPerPage(0);
  const int default_page_size = TilesPerPage(1);

  max_rows_on_first_page_ = max_rows_on_first_page;
  max_rows_ = max_rows;
  SetMaxColumnsInternal(max_columns);

  // Update paging and pulsing blocks if the page sizes have changed.
  if (item_list() && (TilesPerPage(0) != first_page_size ||
                      TilesPerPage(1) != default_page_size)) {
    view_structure_.LoadFromMetadata();
    UpdatePaging();
    UpdatePulsingBlockViews();
    PreferredSizeChanged();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler:

void PagedAppsGridView::OnGestureEvent(ui::GestureEvent* event) {
  // If a tap/long-press occurs within a valid tile, it is usually a mistake and
  // should not close the launcher in clamshell mode. Otherwise, we should let
  // those events pass to the ancestor views.
  if (!IsTabletMode() && (event->type() == ui::ET_GESTURE_TAP ||
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

void PagedAppsGridView::OnMouseEvent(ui::MouseEvent* event) {
  if (IsTabletMode() || !event->IsLeftMouseButton())
    return;

  gfx::PointF point_in_root = event->root_location_f();

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      if (!EventIsBetweenOccupiedTiles(event))
        break;
      event->SetHandled();
      mouse_drag_start_point_ = point_in_root;
      last_mouse_drag_point_ = point_in_root;
      // Manually send the press event to the AppListView to update drag root
      // location
      contents_view_->app_list_view()->OnMouseEvent(event);
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

////////////////////////////////////////////////////////////////////////////////
// views::View:

void PagedAppsGridView::Layout() {
  if (ignore_layout())
    return;

  if (bounds_animator()->IsAnimating())
    bounds_animator()->Cancel();

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
  if (pagination_controller_->scroll_axis() ==
      PaginationController::SCROLL_AXIS_HORIZONTAL) {
    const int page_width = page_size.width() + GetPaddingBetweenPages();
    items_container()->SetBoundsRect(gfx::Rect(-page_width * current_page, 0,
                                               page_width * pages,
                                               GetContentsBounds().height()));
  } else {
    const int page_height = page_size.height() + GetPaddingBetweenPages();
    items_container()->SetBoundsRect(gfx::Rect(0, -page_height * current_page,
                                               GetContentsBounds().width(),
                                               page_height * pages));
  }

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
      ui::Layer* const background_card = background_cards_[i].get();
      background_card->SetBounds(BackgroundCardBounds(i));
      items_container()->layer()->StackAtBottom(background_card);
    }
    MaskContainerToBackgroundBounds();
  }
  views::ViewModelUtils::SetViewBoundsToIdealBounds(pulsing_blocks_model());
}

void PagedAppsGridView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kClipsChildren, true);
  AppsGridView::GetAccessibleNodeData(node_data);
}

void PagedAppsGridView::OnThemeChanged() {
  views::View::OnThemeChanged();

  for (auto& card : background_cards_)
    card.get()->SchedulePaint(card->parent()->bounds());
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
  if (features::IsProductivityLauncherEnabled()) {
    return cardified_state_ ? kCardifiedPaddingBetweenPagesProdLauncher +
                                  2 * kBackgroundCardVerticalPadding
                            : kPaddingBetweenPages;
  }

  // In cardified state, padding between pages should be fixed and it should
  // include background card padding.
  return cardified_state_
             ? kCardifiedPaddingBetweenPages + 2 * vertical_tile_padding_
             : kPaddingBetweenPages;
}

int PagedAppsGridView::GetTotalPages() const {
  return pagination_model_.total_pages();
}

int PagedAppsGridView::GetSelectedPage() const {
  return pagination_model_.selected_page();
}

bool PagedAppsGridView::IsScrollAxisVertical() const {
  return pagination_controller_->scroll_axis() ==
         PaginationController::SCROLL_AXIS_VERTICAL;
}

void PagedAppsGridView::UpdateBorder() {
  if (IsInFolder())
    return;

  if (!features::IsProductivityLauncherEnabled())
    SetBorder(
        views::CreateEmptyBorder(gfx::Insets::VH(GetFadeoutMaskHeight(), 0)));
}

void PagedAppsGridView::MaybeStartCardifiedView() {
  if (!cardified_state_)
    StartAppsGridCardifiedView();
}

void PagedAppsGridView::MaybeEndCardifiedView() {
  if (cardified_state_)
    EndAppsGridCardifiedView();
}

void PagedAppsGridView::MaybeStartPageFlip() {
  MaybeStartPageFlipTimer(last_drag_point());

  if (cardified_state_) {
    int hovered_page = GetPageFlipTargetForDrag(last_drag_point());
    if (hovered_page == -1)
      hovered_page = pagination_model_.selected_page();

    SetHighlightedBackgroundCard(hovered_page);
  }
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

int PagedAppsGridView::GetMaxRowsInPage(int page) const {
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
  if (!IsInFolder() && !features::IsProductivityLauncherEnabled()) {
    pagination_model_.SetTotalPages(view_structure_.total_pages());
    return;
  }

  // Folders have the same number of tiles on every page, while the root
  // level grid can have a different number of tiles per page.
  size_t tiles = view_model()->view_size();
  int total_pages = 1;
  size_t tiles_on_page = TilesPerPage(0);
  while (tiles > tiles_on_page) {
    tiles -= tiles_on_page;
    ++total_pages;
    tiles_on_page = TilesPerPage(total_pages - 1);
  }
  pagination_model_.SetTotalPages(total_pages);
}

void PagedAppsGridView::RecordPageMetrics() {
  DCHECK(!IsInFolder());
  UMA_HISTOGRAM_COUNTS_100("Apps.NumberOfPages", GetTotalPages());

  // There are no empty slots with ProductivityLauncher enabled.
  if (features::IsProductivityLauncherEnabled())
    return;

  // Calculate the number of pages that have empty slots.
  int page_count = 0;
  const auto& pages = view_structure_.pages();
  for (size_t i = 0; i < pages.size(); ++i) {
    if (static_cast<int>(pages[i].size()) < TilesPerPage(i))
      ++page_count;
  }
  UMA_HISTOGRAM_COUNTS_100("Apps.NumberOfPagesNotFull", page_count);
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
  if (IsScrollAxisVertical()) {
    const int page_height = page_size.height() + GetPaddingBetweenPages();
    return gfx::Vector2d(0, page_height * multiplier);
  }

  // Page size including padding pixels. A tile.x + page_width means the same
  // tile slot in the next page.
  const int page_width = page_size.width() + GetPaddingBetweenPages();
  return gfx::Vector2d(page_width * multiplier, 0);
}

void PagedAppsGridView::EnsureViewVisible(const GridIndex& index) {
  if (pagination_model_.has_transition())
    return;

  if (IsValidIndex(index))
    pagination_model_.SelectPage(index.page, false);
}

absl::optional<PagedAppsGridView::VisibleItemIndexRange>
PagedAppsGridView::GetVisibleItemIndexRange() const {
  // Expect that there is no active page transitions. Otherwise, the return
  // value can be obsolete.
  DCHECK(!pagination_model_.has_transition());

  const int selected_page = pagination_model_.selected_page();
  if (selected_page < 0)
    return absl::nullopt;

  // Get the selected page's item count.
  const int on_page_item_count = GetNumberOfItemsOnPage(selected_page);

  // Return early if the selected page is empty.
  if (!on_page_item_count)
    return absl::nullopt;

  // Calculate the index of the first view on the selected page.
  int start_view_index = 0;
  for (int page_index = 0; page_index < selected_page; ++page_index)
    start_view_index += GetNumberOfItemsOnPage(page_index);

  return VisibleItemIndexRange(start_view_index,
                               start_view_index + on_page_item_count - 1);
}

base::ScopedClosureRunner PagedAppsGridView::LockAppsGridOpacity() {
  lock_opacity_ = true;

  base::OnceClosure reset_closure = base::BindOnce(
      [](base::WeakPtr<PagedAppsGridView> weak_ptr) {
        if (!weak_ptr)
          return;

        weak_ptr->lock_opacity_ = false;
      },
      weak_ptr_factory_.GetWeakPtr());
  return base::ScopedClosureRunner(std::move(reset_closure));
}

////////////////////////////////////////////////////////////////////////////////
// PaginationModelObserver:

void PagedAppsGridView::TotalPagesChanged(int previous_page_count,
                                          int new_page_count) {
  // Don't record from folder.
  if (IsInFolder())
    return;

  // Initial setup for the AppList starts with -1 pages. Ignore the page count
  // change resulting from the initialization of the view.
  if (previous_page_count <= 0)
    return;

  // Ignore page count changes after item list has been reset (e.g. during
  // shutdown).
  if (!item_list() || !item_list()->item_count())
    return;

  if (previous_page_count < new_page_count) {
    AppListPageCreationType type = AppListPageCreationType::kSyncOrInstall;
    if (handling_keyboard_move())
      type = AppListPageCreationType::kMovingAppWithKeyboard;
    else if (IsDragging())
      type = AppListPageCreationType::kDraggingApp;
    UMA_HISTOGRAM_ENUMERATION("Apps.AppList.AppsGridAddPage", type);
  }
}

void PagedAppsGridView::SelectedPageChanged(int old_selected,
                                            int new_selected) {
  items_container()->layer()->SetTransform(gfx::Transform());
  if (IsDragging()) {
    Layout();
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
    Layout();
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
    Layout();
  }

  pagination_metrics_tracker_ =
      GetWidget()->GetCompositor()->RequestNewThroughputTracker();
  pagination_metrics_tracker_->Start(metrics_util::ForSmoothness(
      base::BindRepeating(&ReportPaginationSmoothness, IsTabletMode())));
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
  if (pagination_controller_->scroll_axis() ==
      PaginationController::SCROLL_AXIS_HORIZONTAL) {
    const int page_width = page_size.width() + GetPaddingBetweenPages();
    translate.set_x(page_width * transition.progress * dir);
  } else {
    const int page_height = page_size.height() + GetPaddingBetweenPages();
    translate.set_y(page_height * transition.progress * dir);
  }
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

void PagedAppsGridView::ScrollEnded() {
  // Scroll can end without triggering state animation.
  presentation_time_recorder_.reset();
}

bool PagedAppsGridView::DoesIntersectRect(const views::View* target,
                                          const gfx::Rect& rect) const {
  gfx::Rect target_bounds(target->GetLocalBounds());
  if (features::IsProductivityLauncherEnabled() && GetSelectedPage() == 0) {
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
      background_cards_[card_index]->bounds());
  gfx::Point origin_in_apps_grid = bounds_in_items_container.origin();
  views::View::ConvertPointToTarget(items_container(), this,
                                    &origin_in_apps_grid);
  return gfx::Rect(origin_in_apps_grid, bounds_in_items_container.size());
}

ui::Layer* PagedAppsGridView::GetBackgroundCardLayerForTesting(
    size_t card_index) const {
  DCHECK_LT(card_index, background_cards_.size());
  return background_cards_[card_index].get();
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
  const int rows = TilesPerPage(page) / cols();
  rect.set_size(gfx::Size(rect.width() * cols(), rect.height() * rows));
  rect.Inset(-GetTilePadding(page));
  return rect.size();
}

bool PagedAppsGridView::ShouldHandleDragEvent(const ui::LocatedEvent& event) {
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
  if (!IsInFolder() &&
      (event.IsMouseEvent() || event.type() == ui::ET_GESTURE_SCROLL_BEGIN) &&
      !IsTabletMode() &&
      ((pagination_model_.selected_page() == 0 &&
        calculate_offset(event) > 0) ||
       contents_view_->app_list_view()->is_in_drag())) {
    return false;
  }

  return true;
}

bool PagedAppsGridView::IsValidPageFlipTarget(int page) const {
  if (pagination_model_.is_valid_page(page))
    return true;

  // If the user wants to drag an app to the next new page and has not done so
  // during the dragging session, then it is the right target because a new page
  // will be created in OnPageFlipTimer().
  return !features::IsProductivityLauncherEnabled() && !IsInFolder() &&
         !extra_page_opened_ && pagination_model_.total_pages() == page;
}

int PagedAppsGridView::GetPageFlipTargetForDrag(const gfx::Point& drag_point) {
  int new_page_flip_target = -1;

  // Drag zones are at the edges of the scroll axis.
  if (IsScrollAxisVertical()) {
    if (features::IsProductivityLauncherEnabled()) {
      if (background_cards_.empty() ||
          !container_delegate_->IsPointWithinPageFlipBuffer(drag_point)) {
        return new_page_flip_target;
      }

      gfx::RectF background_card_rect_in_grid(
          background_cards_[GetSelectedPage()]->bounds());
      View::ConvertRectToTarget(items_container(), this,
                                &background_card_rect_in_grid);

      // Set page flip target when the drag is above or below the background
      // card of the currently selected page.
      if (drag_point.y() < background_card_rect_in_grid.y()) {
        new_page_flip_target = pagination_model_.selected_page() - 1;
      } else if (drag_point.y() > background_card_rect_in_grid.bottom()) {
        new_page_flip_target = pagination_model_.selected_page() + 1;
      }
    } else {
      if (drag_point.y() < kPageFlipZoneSize + GetInsets().top()) {
        new_page_flip_target = pagination_model_.selected_page() - 1;
      } else if (container_delegate_->IsPointWithinBottomDragBuffer(
                     drag_point, kPageFlipZoneSize)) {
        // If the drag point is within the drag buffer, but not over the shelf.
        new_page_flip_target = pagination_model_.selected_page() + 1;
      }
    }
  } else {
    // TODO(xiyuan): Fix this for RTL.
    if (new_page_flip_target == -1 && drag_point.x() < kPageFlipZoneSize)
      new_page_flip_target = pagination_model_.selected_page() - 1;

    if (new_page_flip_target == -1 &&
        drag_point.x() > width() - kPageFlipZoneSize) {
      new_page_flip_target = pagination_model_.selected_page() + 1;
    }
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
    RecordPageSwitcherSource(kDragAppToBorder, IsTabletMode());

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
  // Add an extra card for the peeking page in the last page if item drag is
  // allowed to create new pages. This hints users that apps can be dragged past
  // the last existing page.
  const int peeking_card_count =
      features::IsProductivityLauncherEnabled() ? 0 : 1;
  for (int i = 0; i < pagination_model_.total_pages() + peeking_card_count; i++)
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
    // Normally Layout() cancels any animations. At this point there may be a
    // pending Layout(), force it now so that one isn't triggered part way
    // through the animation. Further, ignore this layout so that the position
    // isn't reset.
    DCHECK(!ignore_layout_);
    base::AutoReset<bool> auto_reset(&ignore_layout_, true);
    GetWidget()->LayoutRootViewIfNecessary();
  }

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

  // Apps might be animating due to drag reorder. Cancel any active animations
  // so that the cardified state animation can be applied.
  bounds_animator()->Cancel();

  gfx::Vector2d translate_offset(
      0, start_position.y() - items_container()->origin().y());

  // Create background card animation metric reporters.
  std::vector<std::unique_ptr<ui::AnimationThroughputReporter>> reporters;
  for (auto& background_card : background_cards_) {
    reporters.push_back(std::make_unique<ui::AnimationThroughputReporter>(
        background_card->GetAnimator(),
        metrics_util::ForSmoothness(base::BindRepeating(
            &ReportCardifiedSmoothness, cardified_state_))));
  }

  views::AnimationBuilder animations;
  cardified_animation_abort_handle_ = animations.GetAbortHandle();
  animations
      .OnEnded(base::BindOnce(&PagedAppsGridView::MaybeCallOnBoundsAnimatorDone,
                              weak_ptr_factory_.GetWeakPtr()))
      .OnAborted(
          base::BindOnce(&PagedAppsGridView::MaybeCallOnBoundsAnimatorDone,
                         weak_ptr_factory_.GetWeakPtr()))
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::Milliseconds(kDefaultAnimationDuration));

  DCHECK(!bounds_animation_for_cardified_state_in_progress_);
  bounds_animation_for_cardified_state_in_progress_ = true;

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
    auto& background_card = background_cards_[i];
    // Reposition card bounds to compensate for the translation offset.
    gfx::Rect background_bounds = background_card->bounds();
    background_bounds.Offset(translate_offset);
    background_card->SetBounds(background_bounds);
    if (cardified_state_) {
      const bool is_active_page =
          background_cards_[pagination_model_.selected_page()] ==
          background_card;
      background_card->SetIsActivePage(is_active_page);
    } else {
      animations.GetCurrentSequence().SetOpacity(background_card.get(),
                                                 kBackgroundCardOpacityHide);
    }
    animations.GetCurrentSequence().SetBounds(background_card.get(),
                                              BackgroundCardBounds(i),
                                              kCardifiedStateTweenType);
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
    gfx::Rect current_bounds = entry_view->bounds();
    current_bounds.Offset(translate_offset);

    entry_view->EnsureLayer();

    if (cardified_state_)
      entry_view->EnterCardifyState();
    else
      entry_view->ExitCardifyState();

    gfx::Rect target_bounds(view_model()->ideal_bounds(i));

    if (entry_view->has_pending_row_change()) {
      entry_view->reset_has_pending_row_change();
      row_change_animator_->AnimateBetweenRows(entry_view, current_bounds,
                                               target_bounds);
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
        gfx::RectF(items_container()->GetMirroredRect(current_bounds)));
    entry_view->layer()->SetTransform(transform);

    animation_sequence->SetTransform(entry_view->layer(), gfx::Transform(),
                                     kCardifiedStateTweenType);
  }
}

void PagedAppsGridView::MaybeCallOnBoundsAnimatorDone() {
  DCHECK(bounds_animation_for_cardified_state_in_progress_);
  bounds_animation_for_cardified_state_in_progress_ = false;

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
  if (features::IsProductivityLauncherEnabled()) {
    background_card_size =
        grid_size + gfx::Size(2 * kBackgroundCardHorizontalPadding,
                              2 * kBackgroundCardVerticalPadding);
  } else {
    const int vertical_tile_padding = new_page_index == 0
                                          ? first_page_vertical_tile_padding_
                                          : vertical_tile_padding_;
    background_card_size =
        grid_size + gfx::Size(2 * horizontal_tile_padding_,
                              2 * std::max(kMinVerticalPaddingBetweenTiles,
                                           vertical_tile_padding));
  }

  // Add a padding on the sides to make space for pagination preview, but make
  // sure the padding doesn't exceed the tile padding (otherwise the background
  // bounds would clip the apps grid bounds).
  const int extra_padding_for_cardified_state =
      features::IsProductivityLauncherEnabled()
          ? 0
          : std::min(horizontal_tile_padding_, kCardifiedHorizontalPadding);
  const int horizontal_padding =
      (GetContentsBounds().width() - background_card_size.width()) / 2 +
      extra_padding_for_cardified_state;

  int y_offset = new_page_index == 0 ? GetTotalTopPaddingOnFirstPage() : 0;
  if (features::IsProductivityLauncherEnabled()) {
    // Subtract the amount that the apps grid view is offset by to accommodate
    // for the top gradient mask. This will visually center the background card
    // between the top and bottom gradient masks.
    y_offset -= margin_for_gradient_mask_;
  } else {
    y_offset = std::max(y_offset, GetFadeoutMaskHeight());
  }

  int vertical_padding = y_offset + (GetContentsBounds().height() - y_offset -
                                     background_card_size.height()) /
                                        2;

  const int padding_between_pages = GetPaddingBetweenPages();
  // The space that each page occupies in the items container. This is the size
  // of the grid without outer padding plus the padding between pages.
  const int total_page_height = GetPageSize().height() + padding_between_pages;
  // We position a new card in the last place in items container view.
  const int vertical_page_start_offset = total_page_height * new_page_index;

  return gfx::Rect(
      horizontal_padding, vertical_padding + vertical_page_start_offset,
      background_card_size.width() - 2 * extra_padding_for_cardified_state,
      background_card_size.height());
}

void PagedAppsGridView::AppendBackgroundCard() {
  background_cards_.push_back(std::make_unique<BackgroundCardLayer>());
  ui::Layer* current_layer = background_cards_.back().get();
  current_layer->SetBounds(BackgroundCardBounds(background_cards_.size() - 1));
  current_layer->SetVisible(true);
  items_container()->layer()->Add(current_layer);
}

void PagedAppsGridView::RemoveBackgroundCard() {
  items_container()->layer()->Remove(background_cards_.back().get());
  background_cards_.pop_back();
}

void PagedAppsGridView::MaskContainerToBackgroundBounds() {
  DCHECK(!background_cards_.empty());
  // Mask apps grid container layer to the background card width. Optionally
  // also include extra height to ensure the top gradient mask is shown as well.
  layer()->SetClipRect(
      gfx::Rect(background_cards_[0]->bounds().x(), -margin_for_gradient_mask_,
                background_cards_[0]->bounds().width(),
                layer()->bounds().height() + margin_for_gradient_mask_));
}

void PagedAppsGridView::RemoveAllBackgroundCards() {
  for (auto& card : background_cards_)
    items_container()->layer()->Remove(card.get());
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
  if (!features::IsProductivityLauncherEnabled() || !app_list_config())
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
    // Normally Layout() cancels any animations. At this point there may be a
    // pending Layout(), force it now so that one isn't triggered part way
    // through the animation. Further, ignore this layout so that the position
    // isn't reset.
    base::AutoReset<bool> auto_reset(&ignore_layout_, true);
    GetWidget()->LayoutRootViewIfNecessary();
  }

  PrepareItemsForBoundsAnimation();
  AnimateToIdealBounds();
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
    items_container()->layer()->StackAtBottom(background_cards_[i].get());
  }
}

}  // namespace ash
