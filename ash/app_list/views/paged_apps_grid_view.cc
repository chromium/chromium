// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/paged_apps_grid_view.h"

#include <algorithm>
#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/pagination/pagination_controller.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
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
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/paint_info.h"
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
constexpr base::TimeDelta kPageFlipDelay =
    base::TimeDelta::FromMilliseconds(500);

// Vertical padding between the apps grid pages in cardified state.
constexpr int kCardifiedPaddingBetweenPages = 12;

// Horizontal padding of the apps grid page in cardified state.
constexpr int kCardifiedHorizontalPadding = 16;

// The radius of the corner of the background cards in the apps grid.
constexpr int kBackgroundCardCornerRadius = 12;

// The opacity for the background cards when hidden.
constexpr float kBackgroundCardOpacityHide = 0.0f;

// Animation curve used for entering and exiting cardified state.
constexpr gfx::Tween::Type kCardifiedStateTweenType =
    gfx::Tween::LINEAR_OUT_SLOW_IN;

// CardifiedAnimationObserver is used to observe the animation for toggling the
// cardified state of the apps grid view. We used this to ensure app icons are
// repainted with the correct bounds and scale.
class CardifiedAnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  explicit CardifiedAnimationObserver(base::OnceClosure callback)
      : callback_(std::move(callback)) {}
  CardifiedAnimationObserver(const CardifiedAnimationObserver&) = delete;
  CardifiedAnimationObserver& operator=(const CardifiedAnimationObserver&) =
      delete;
  ~CardifiedAnimationObserver() override = default;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    if (callback_)
      std::move(callback_).Run();
    delete this;
  }

 private:
  base::OnceClosure callback_;
};

}  // namespace

// A layer delegate used for PagedAppsGridView's mask layer, with top and bottom
// gradient fading out zones.
class PagedAppsGridView::FadeoutLayerDelegate : public ui::LayerDelegate {
 public:
  explicit FadeoutLayerDelegate(int fadeout_mask_height)
      : layer_(ui::LAYER_TEXTURED), fadeout_mask_height_(fadeout_mask_height) {
    layer_.set_delegate(this);
    layer_.SetFillsBoundsOpaquely(false);
  }
  FadeoutLayerDelegate(const FadeoutLayerDelegate&) = delete;
  FadeoutLayerDelegate& operator=(const FadeoutLayerDelegate&) = delete;

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
};

PagedAppsGridView::PagedAppsGridView(
    ContentsView* contents_view,
    AppListA11yAnnouncer* a11y_announcer,
    AppsGridViewFolderDelegate* folder_delegate)
    : AppsGridView(contents_view,
                   a11y_announcer,
                   contents_view->GetAppListMainView()->view_delegate(),
                   folder_delegate),
      contents_view_(contents_view),
      page_flip_delay_(kPageFlipDelay) {
  DCHECK(contents_view_);
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
    if (item_view->item()->is_folder())
      item_view->SetBackgroundBlurEnabled(started);
  }

  // Prevent context menus from remaining open after a transition
  CancelContextMenusOnCurrentPage();
}

void PagedAppsGridView::HandleScrollFromAppListView(const gfx::Vector2d& offset,
                                                    ui::EventType type) {
  // If |pagination_model_| is empty, don't handle scroll events.
  if (pagination_model_.total_pages() <= 0)
    return;

  // Maybe switch pages.
  pagination_controller_->OnScroll(offset, type);
}

void PagedAppsGridView::UpdateOpacity(bool restore_opacity) {
  if (view_structure_.pages().empty())
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
    // If drag is in progress, layers are still required, so just update the
    // opacity (the layers will be deleted when drag operation completes).
    if (items_need_layer_for_drag_) {
      for (const auto& entry : view_model()->entries()) {
        if (drag_view() != entry.view && entry.view->layer())
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
  // Logging for https://crbug.com/1194639. We suspect |selected_page| is
  // sometimes off the end of the view structure pages array.
  if (selected_page >= static_cast<int>(view_structure_.pages().size())) {
    // Use concise log so it fits in a crash key.
    LOG(FATAL) << "crbug.com/1194639 " << pagination_model_.total_pages() << " "
               << selected_page << " "
               << static_cast<int>(view_structure_.pages().size());
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
    const float start_px = GetAppListConfig().all_apps_opacity_start_px();
    opacity = base::ClampToRange(
        (centerline_above_work_area - start_px) /
            (GetAppListConfig().all_apps_opacity_end_px() - start_px),
        0.f, 1.0f);

    if (opacity == item_view->layer()->opacity())
      continue;

    const size_t end_index = std::min(current_page.size() - 1, i + cols() - 1);
    for (size_t j = i; j <= end_index; ++j) {
      if (current_page[j] != drag_view())
        current_page[j]->layer()->SetOpacity(opacity);
    }
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
  const gfx::Size page_size = GetTileGridSize();
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

  if (fadeout_layer_delegate_)
    fadeout_layer_delegate_->layer()->SetBounds(layer()->bounds());

  CalculateIdealBoundsForFolder();
  for (int i = 0; i < view_model()->view_size(); ++i) {
    AppListItemView* view = GetItemViewAt(i);
    if (view != drag_view_) {
      view->SetBoundsRect(view_model()->ideal_bounds(i));
    } else {
      // If the drag view size changes, make sure it has the same center.
      gfx::Rect bounds = view->bounds();
      bounds.ClampToCenteredSize(GetTileViewSize());
      view->SetBoundsRect(bounds);
    }
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
    MaybeCreateGradientMask();
  }
  views::ViewModelUtils::SetViewBoundsToIdealBounds(pulsing_blocks_model());
}

////////////////////////////////////////////////////////////////////////////////
// AppsGridView:

gfx::Size PagedAppsGridView::GetTileViewSize() const {
  const AppListConfig& config = GetAppListConfig();
  return gfx::ScaleToRoundedSize(
      gfx::Size(config.grid_tile_width(), config.grid_tile_height()),
      (cardified_state_ ? kCardifiedScale : 1.0f));
}

gfx::Insets PagedAppsGridView::GetTilePadding() const {
  if (IsInFolder()) {
    const int tile_padding_in_folder =
        GetAppListConfig().grid_tile_spacing_in_folder() / 2;
    return gfx::Insets(-tile_padding_in_folder, -tile_padding_in_folder);
  }
  return gfx::Insets(-vertical_tile_padding_, -horizontal_tile_padding_);
}

gfx::Size PagedAppsGridView::GetTileGridSize() const {
  gfx::Rect rect(GetTotalTileSize());
  rect.set_size(
      gfx::Size(rect.width() * cols(), rect.height() * rows_per_page()));
  rect.Inset(-GetTilePadding());
  return rect.size();
}

int PagedAppsGridView::GetPaddingBetweenPages() const {
  // In cardified state, padding between pages should be fixed  and it should
  // include background card padding.
  return cardified_state_
             ? kCardifiedPaddingBetweenPages + 2 * vertical_tile_padding_
             : GetAppListConfig().page_spacing();
}

bool PagedAppsGridView::IsScrollAxisVertical() const {
  return pagination_controller_->scroll_axis() ==
         PaginationController::SCROLL_AXIS_VERTICAL;
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

void PagedAppsGridView::RecordAppMovingTypeMetrics(AppListAppMovingType type) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppListAppMovingType", type,
                            kMaxAppListAppMovingType);
}

void PagedAppsGridView::OnAppListItemViewActivated(
    AppListItemView* pressed_item_view,
    const ui::Event& event) {
  if (IsDragging())
    return;

  if (contents_view_->apps_container_view()
          ->app_list_folder_view()
          ->IsAnimationRunning()) {
    return;
  }

  // Always set the previous `activated_folder_item_view_` to be visible. This
  // prevents a case where the item would remain hidden due the
  // `activated_folder_item_view_` changing during the animation. We only
  // need to track `activated_folder_item_view_` in the root level grid view.
  if (!folder_delegate()) {
    if (activated_folder_item_view())
      activated_folder_item_view()->SetVisible(true);
    set_activated_folder_item_view(
        IsFolderItem(pressed_item_view->item()) ? pressed_item_view : nullptr);
  }
  contents_view_->GetAppListMainView()->ActivateApp(pressed_item_view->item(),
                                                    event.flags());
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
  if (previous_page_count == -1)
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
                              : (GetItemsNumOfPage(new_selected) - 1));
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

  MaybeCreateGradientMask();
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
  items_container()->layer()->SetTransform(transform);

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

void PagedAppsGridView::TransitionEnded() {
  pagination_metrics_tracker_->Stop();

  // Gradient mask is no longer necessary once transition is finished.
  if (layer()->layer_mask_layer())
    layer()->SetMaskLayer(nullptr);
}

void PagedAppsGridView::ScrollStarted() {
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

void PagedAppsGridView::ScrollEnded() {
  // Scroll can end without triggering state animation.
  presentation_time_recorder_.reset();
  // Need to reset the mask because transition will not happen in some
  // cases. (See https://crbug.com/1049275)
  layer()->SetMaskLayer(nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// ui::ImplicitAnimationObserver:

void PagedAppsGridView::OnImplicitAnimationsCompleted() {
  if (layer()->opacity() == 0.0f)
    SetVisible(false);
  if (cardified_state_) {
    MaskContainerToBackgroundBounds();
    return;
  }
  RemoveAllBackgroundCards();
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
  gfx::Rect bounds_in_items_container = background_cards_[card_index]->bounds();
  gfx::Point origin_in_apps_grid = bounds_in_items_container.origin();
  views::View::ConvertPointToTarget(items_container(), this,
                                    &origin_in_apps_grid);
  return gfx::Rect(origin_in_apps_grid, bounds_in_items_container.size());
}

////////////////////////////////////////////////////////////////////////////////
// private:

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

void PagedAppsGridView::MaybeCreateGradientMask() {
  if (!IsInFolder() && features::IsBackgroundBlurEnabled()) {
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

bool PagedAppsGridView::IsValidPageFlipTarget(int page) const {
  if (pagination_model_.is_valid_page(page))
    return true;

  // If the user wants to drag an app to the next new page and has not done so
  // during the dragging session, then it is the right target because a new page
  // will be created in OnPageFlipTimer().
  return !IsInFolder() && !extra_page_opened_ &&
         pagination_model_.total_pages() == page;
}

bool PagedAppsGridView::IsPointWithinPageFlipBuffer(
    const gfx::Point& point) const {
  // The page flip buffer is the work area bounds excluding shelf bounds, which
  // is the same as AppsContainerView's bounds.
  gfx::Point point_in_parent = point;
  ConvertPointToTarget(this, parent(), &point_in_parent);
  return parent()->GetContentsBounds().Contains(point_in_parent);
}

bool PagedAppsGridView::IsPointWithinBottomDragBuffer(
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

int PagedAppsGridView::GetPageFlipTargetForDrag(const gfx::Point& drag_point) {
  int new_page_flip_target = -1;

  // Drag zones are at the edges of the scroll axis.
  if (IsScrollAxisVertical()) {
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

void PagedAppsGridView::MaybeStartPageFlipTimer(const gfx::Point& drag_point) {
  if (!IsPointWithinPageFlipBuffer(drag_point))
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

void PagedAppsGridView::StartAppsGridCardifiedView() {
  if (!app_list_features::IsNewDragSpecInLauncherEnabled())
    return;
  if (IsInFolder())
    return;
  DCHECK(!cardified_state_);
  StopObservingImplicitAnimations();
  RemoveAllBackgroundCards();
  // Calculate background bounds for a normal grid so it animates from the
  // normal to the cardified bounds with the icons.
  // Add an extra card for the peeking page in the last page. This hints users
  // that apps can be dragged past the last existing page.
  for (int i = 0; i < pagination_model_.total_pages() + 1; i++)
    AppendBackgroundCard();
  cardified_state_ = true;
  UpdateTilePadding();
  MaybeCreateGradientMask();
  AnimateCardifiedState();
}

void PagedAppsGridView::EndAppsGridCardifiedView() {
  if (!app_list_features::IsNewDragSpecInLauncherEnabled())
    return;
  if (IsInFolder())
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

  CalculateIdealBounds();
  // Cache the current item container position, as RecenterItemsContainer() may
  // change it.
  gfx::Point start_position = items_container()->origin();
  RecenterItemsContainer();
  gfx::Vector2d translate_offset(
      0, start_position.y() - items_container()->origin().y());
  if (cardified_state_) {
    // The drag view is translated when the items container is recentered.
    // Reposition the drag view to compensate for the translation offset.
    drag_view_start_ += translate_offset;
    drag_view_->SetPosition(drag_view_start_);
  }
  // Drag view can be nullptr or moved from the model by EndDrag.
  const bool model_contains_drag_view =
      drag_view_ && (view_model()->GetIndexOfView(drag_view_) != -1);
  const int number_of_views_to_animate =
      view_model()->view_size() - (model_contains_drag_view ? 1 : 0);

  base::RepeatingClosure on_bounds_animator_callback;
  if (number_of_views_to_animate > 0) {
    on_bounds_animator_callback = base::BarrierClosure(
        number_of_views_to_animate,
        base::BindOnce(&PagedAppsGridView::MaybeCallOnBoundsAnimatorDone,
                       weak_ptr_factory_.GetWeakPtr()));
    bounds_animation_for_cardified_state_in_progress_++;
  }

  for (int i = 0; i < view_model()->view_size(); ++i) {
    AppListItemView* entry_view = view_model()->view_at(i);
    // We don't animate bounds for the dragged view.
    if (entry_view == drag_view_)
      continue;
    // Reposition view bounds to compensate for the translation offset.
    gfx::Rect current_bounds = entry_view->bounds();
    current_bounds.Offset(translate_offset);

    entry_view->EnsureLayer();

    if (cardified_state_)
      entry_view->EnterCardifyState();
    else
      entry_view->ExitCardifyState();

    gfx::Rect target_bounds(view_model()->ideal_bounds(i));
    entry_view->SetBoundsRect(target_bounds);

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

  for (size_t i = 0; i < background_cards_.size(); i++) {
    auto& background_card = background_cards_[i];
    // Reposition card bounds to compensate for the translation offset.
    gfx::Rect background_bounds = background_card->bounds();
    background_bounds.Offset(translate_offset);
    background_card->SetBounds(background_bounds);
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
    background_card->SetBounds(BackgroundCardBounds(i));
  }
  highlighted_page_ = pagination_model_.selected_page();
}

void PagedAppsGridView::MaybeCallOnBoundsAnimatorDone() {
  --bounds_animation_for_cardified_state_in_progress_;
  if (bounds_animation_for_cardified_state_in_progress_ == 0)
    OnBoundsAnimatorDone(/*animator=*/nullptr);
}

void PagedAppsGridView::RecenterItemsContainer() {
  const int pages = pagination_model_.total_pages();
  const int current_page = pagination_model_.selected_page();
  const int page_height = GetTileGridSize().height() + GetPaddingBetweenPages();
  items_container()->SetBoundsRect(gfx::Rect(0, -page_height * current_page,
                                             GetContentsBounds().width(),
                                             page_height * pages));
}

gfx::Rect PagedAppsGridView::BackgroundCardBounds(int new_page_index) {
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
  return gfx::Rect(
      horizontal_padding, vertical_padding + vertical_page_start_offset,
      background_card_size.width() - 2 * kCardifiedHorizontalPadding,
      background_card_size.height());
}

void PagedAppsGridView::AppendBackgroundCard() {
  background_cards_.push_back(
      std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR));
  ui::Layer* current_layer = background_cards_.back().get();
  current_layer->SetBounds(BackgroundCardBounds(background_cards_.size() - 1));
  current_layer->SetVisible(true);
  current_layer->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kBackgroundCardCornerRadius));
  items_container()->layer()->Add(current_layer);
}

void PagedAppsGridView::RemoveBackgroundCard() {
  items_container()->layer()->Remove(background_cards_.back().get());
  background_cards_.pop_back();
}

void PagedAppsGridView::MaskContainerToBackgroundBounds() {
  DCHECK(!background_cards_.empty());
  // Mask apps grid container layer to the background card width.
  layer()->SetClipRect(gfx::Rect(background_cards_[0]->bounds().x(), 0,
                                 background_cards_[0]->bounds().width(),
                                 layer()->bounds().height()));
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

void PagedAppsGridView::UpdateTilePadding() {
  gfx::Size content_size = GetContentsBounds().size();
  const gfx::Size tile_size = GetTileViewSize();
  if (cardified_state_)
    content_size = gfx::ScaleToRoundedSize(content_size, kCardifiedScale);

  // Item tiles should be evenly distributed in this view.
  horizontal_tile_padding_ =
      cols() > 1 ? (content_size.width() - cols() * tile_size.width()) /
                       ((cols() - 1) * 2)
                 : 0;
  vertical_tile_padding_ =
      rows_per_page() > 1
          ? (content_size.height() - rows_per_page() * tile_size.height()) /
                ((rows_per_page() - 1) * 2)
          : 0;
}

}  // namespace ash
