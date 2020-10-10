// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_session.h"

#include "ash/capture_mode/capture_label_view.h"
#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_window_observer.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/memory/ptr_util.h"
#include "cc/paint/paint_flags.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

constexpr int kCaptureRegionBorderStrokePx = 1;

// The visual radius of the drag affordance circles which are shown while
// resizing a drag region.
constexpr int kAffordanceCircleRadiusDp = 5;

// The hit radius of the drag affordance circles touch events.
constexpr int kAffordanceCircleTouchHitRadiusDp = 16;

constexpr int kSizeLabelBorderRadius = 4;

constexpr int kSizeLabelHorizontalPadding = 8;

constexpr SkColor kRegionBorderColor = SK_ColorWHITE;

// Blue300 at 30%.
constexpr SkColor kCaptureRegionColor = SkColorSetA(gfx::kGoogleBlue300, 77);

// Values for the shadows of the capture region components.
constexpr gfx::ShadowValue kRegionOutlineShadow(gfx::Vector2d(0, 0),
                                                2,
                                                SkColorSetARGB(41, 0, 0, 0));
constexpr gfx::ShadowValue kRegionAffordanceCircleShadow1(
    gfx::Vector2d(0, 1),
    2,
    SkColorSetARGB(76, 0, 0, 0));
constexpr gfx::ShadowValue kRegionAffordanceCircleShadow2(
    gfx::Vector2d(0, 2),
    6,
    SkColorSetARGB(38, 0, 0, 0));

// The minimum padding on each side of the capture region. If the capture button
// cannot be placed in the center of the capture region and maintain this
// padding, it will be placed below or above the capture region.
constexpr int kCaptureRegionMinimumPaddingDp = 16;

// Mouse cursor warping is disabled when the capture source is a custom region.
// Sets the mouse warp status to |enable| and return the original value.
bool SetMouseWarpEnabled(bool enable) {
  auto* mouse_cursor_filter = Shell::Get()->mouse_cursor_filter();
  const bool old_value = mouse_cursor_filter->mouse_warp_enabled();
  mouse_cursor_filter->set_mouse_warp_enabled(enable);
  return old_value;
}

// Gets the overlay container inside |root|.
aura::Window* GetParentContainer(aura::Window* root) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());
  return root->GetChildById(kShellWindowId_OverlayContainer);
}

// Retrieves the point on the |rect| associated with |position|.
gfx::Point GetLocationForPosition(const gfx::Rect& rect,
                                  FineTunePosition position) {
  switch (position) {
    case FineTunePosition::kTopLeft:
      return rect.origin();
    case FineTunePosition::kTopCenter:
      return rect.top_center();
    case FineTunePosition::kTopRight:
      return rect.top_right();
    case FineTunePosition::kRightCenter:
      return rect.right_center();
    case FineTunePosition::kBottomRight:
      return rect.bottom_right();
    case FineTunePosition::kBottomCenter:
      return rect.bottom_center();
    case FineTunePosition::kBottomLeft:
      return rect.bottom_left();
    case FineTunePosition::kLeftCenter:
      return rect.left_center();
    default:
      break;
  }

  NOTREACHED();
  return gfx::Point();
}

// Returns the smallest rect that contains all of |points|.
gfx::Rect GetRectEnclosingPoints(const std::vector<gfx::Point>& points) {
  DCHECK_GE(points.size(), 2u);

  int x = INT_MAX;
  int y = INT_MAX;
  int right = INT_MIN;
  int bottom = INT_MIN;
  for (const gfx::Point& point : points) {
    x = std::min(point.x(), x);
    y = std::min(point.y(), y);
    right = std::max(point.x(), right);
    bottom = std::max(point.y(), bottom);
  }
  return gfx::Rect(x, y, right - x, bottom - y);
}

// Returns the widget init params needed to create a widget associated with a
// capture session.
views::Widget::InitParams CreateWidgetParams(aura::Window* parent,
                                             const gfx::Rect& bounds,
                                             const std::string& name) {
  // Use a popup widget to get transient properties, such as not needing to
  // click on the widget first to get capture before receiving events.
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = parent;
  params.bounds = bounds;
  params.name = name;
  return params;
}

}  // namespace

CaptureModeSession::CaptureModeSession(CaptureModeController* controller,
                                       aura::Window* root)
    : controller_(controller),
      current_root_(root),
      capture_mode_bar_view_(new CaptureModeBarView()),
      old_mouse_warp_status_(SetMouseWarpEnabled(controller_->source() !=
                                                 CaptureModeSource::kRegion)) {
  Shell::Get()->AddPreTargetHandler(this);

  SetLayer(std::make_unique<ui::Layer>(ui::LAYER_TEXTURED));
  layer()->SetFillsBoundsOpaquely(false);
  layer()->set_delegate(this);
  auto* parent = GetParentContainer(current_root_);
  parent->layer()->Add(layer());
  layer()->SetBounds(parent->bounds());

  capture_mode_bar_widget_.Init(CreateWidgetParams(
      parent, CaptureModeBarView::GetBounds(root), "CaptureModeBarWidget"));
  capture_mode_bar_widget_.SetContentsView(
      base::WrapUnique(capture_mode_bar_view_));
  capture_mode_bar_widget_.Show();

  UpdateCaptureLabelWidget();
  RefreshStackingOrder(parent);

  if (controller_->source() == CaptureModeSource::kWindow) {
    capture_window_observer_ =
        std::make_unique<CaptureWindowObserver>(this, controller_->type());
  }
  TabletModeController::Get()->AddObserver(this);
}

CaptureModeSession::~CaptureModeSession() {
  Shell::Get()->RemovePreTargetHandler(this);
  TabletModeController::Get()->RemoveObserver(this);
  SetMouseWarpEnabled(old_mouse_warp_status_);
}

aura::Window* CaptureModeSession::GetSelectedWindow() const {
  return capture_window_observer_ ? capture_window_observer_->window()
                                  : nullptr;
}

void CaptureModeSession::OnCaptureSourceChanged(CaptureModeSource new_source) {
  if (new_source == CaptureModeSource::kWindow) {
    capture_window_observer_ =
        std::make_unique<CaptureWindowObserver>(this, controller_->type());
  } else {
    capture_window_observer_.reset();
  }

  capture_mode_bar_view_->OnCaptureSourceChanged(new_source);
  SetMouseWarpEnabled(new_source != CaptureModeSource::kRegion);
  UpdateDimensionsLabelWidget(/*is_resizing=*/false);
  layer()->SchedulePaint(layer()->bounds());
  UpdateCaptureLabelWidget();
}

void CaptureModeSession::OnCaptureTypeChanged(CaptureModeType new_type) {
  if (controller_->source() == CaptureModeSource::kWindow)
    capture_window_observer_->OnCaptureTypeChanged(new_type);
  capture_mode_bar_view_->OnCaptureTypeChanged(new_type);
  UpdateCaptureLabelWidget();
}

void CaptureModeSession::StartCountDown(
    base::OnceClosure countdown_finished_callback) {
  DCHECK(capture_label_widget_);

  CaptureLabelView* label_view =
      static_cast<CaptureLabelView*>(capture_label_widget_->GetContentsView());
  label_view->StartCountDown(std::move(countdown_finished_callback));
  UpdateCaptureLabelWidgetBounds();
}

void CaptureModeSession::OnPaintLayer(const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer()->size());

  auto* color_provider = AshColorProvider::Get();
  const SkColor dimming_color = color_provider->GetShieldLayerColor(
      AshColorProvider::ShieldLayerType::kShield40);
  recorder.canvas()->DrawColor(dimming_color);

  PaintCaptureRegion(recorder.canvas());
}

void CaptureModeSession::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() != ui::ET_KEY_PRESSED)
    return;

  if (event->key_code() == ui::VKEY_ESCAPE) {
    event->StopPropagation();
    controller_->Stop();  // |this| is destroyed here.
    return;
  }

  if (event->key_code() == ui::VKEY_RETURN) {
    event->StopPropagation();
    controller_->PerformCapture();  // |this| is destroyed here.
    return;
  }
}

void CaptureModeSession::OnMouseEvent(ui::MouseEvent* event) {
  OnLocatedEvent(event, /*is_touch=*/false);
}

void CaptureModeSession::OnTouchEvent(ui::TouchEvent* event) {
  OnLocatedEvent(event, /*is_touch=*/true);
}

void CaptureModeSession::OnTabletModeStarted() {
  UpdateCaptureLabelWidget();
}

void CaptureModeSession::OnTabletModeEnded() {
  UpdateCaptureLabelWidget();
}

gfx::Rect CaptureModeSession::GetSelectedWindowBounds() const {
  auto* window = GetSelectedWindow();
  return window ? window->bounds() : gfx::Rect();
}

void CaptureModeSession::RefreshStackingOrder(aura::Window* parent_container) {
  DCHECK(parent_container);
  auto* capture_mode_bar_layer = capture_mode_bar_widget_.GetLayer();
  auto* overlay_layer = layer();
  auto* parent_container_layer = parent_container->layer();

  parent_container_layer->StackAtTop(overlay_layer);
  parent_container_layer->StackAtTop(capture_label_widget_->GetLayer());
  parent_container_layer->StackAtTop(capture_mode_bar_layer);
}

void CaptureModeSession::PaintCaptureRegion(gfx::Canvas* canvas) {
  gfx::Rect region;
  bool adjustable_region = false;

  switch (controller_->source()) {
    case CaptureModeSource::kFullscreen:
      region = current_root_->bounds();
      break;

    case CaptureModeSource::kWindow:
      region = GetSelectedWindowBounds();
      break;

    case CaptureModeSource::kRegion:
      region = controller_->user_capture_region();
      adjustable_region = true;
      break;
  }

  if (region.IsEmpty())
    return;

  gfx::ScopedCanvas scoped_canvas(canvas);
  const float dsf = canvas->UndoDeviceScaleFactor();
  region = gfx::ScaleToEnclosingRect(region, dsf);

  if (!adjustable_region) {
    canvas->FillRect(region, SK_ColorTRANSPARENT, SkBlendMode::kClear);
    canvas->FillRect(region, kCaptureRegionColor);
    return;
  }

  region.Inset(-kCaptureRegionBorderStrokePx, -kCaptureRegionBorderStrokePx);
  canvas->FillRect(region, SK_ColorTRANSPARENT, SkBlendMode::kClear);

  // Draw the region border.
  cc::PaintFlags border_flags;
  border_flags.setColor(kRegionBorderColor);
  border_flags.setStyle(cc::PaintFlags::kStroke_Style);
  border_flags.setStrokeWidth(kCaptureRegionBorderStrokePx);
  border_flags.setLooper(gfx::CreateShadowDrawLooper({kRegionOutlineShadow}));
  canvas->DrawRect(gfx::RectF(region), border_flags);

  if (is_selecting_region_)
    return;

  // Do not show affordance circles when repositioning the whole region.
  if (fine_tune_position_ == FineTunePosition::kCenter)
    return;

  // Draw the drag affordance circles.
  cc::PaintFlags circle_flags;
  circle_flags.setColor(kRegionBorderColor);
  circle_flags.setStyle(cc::PaintFlags::kFill_Style);
  circle_flags.setLooper(gfx::CreateShadowDrawLooper(
      {kRegionAffordanceCircleShadow1, kRegionAffordanceCircleShadow2}));

  auto draw_circle = [&canvas, &circle_flags](const gfx::Point& location) {
    canvas->DrawCircle(location, kAffordanceCircleRadiusDp, circle_flags);
  };

  draw_circle(region.origin());
  draw_circle(region.top_center());
  draw_circle(region.top_right());
  draw_circle(region.right_center());
  draw_circle(region.bottom_right());
  draw_circle(region.bottom_center());
  draw_circle(region.bottom_left());
  draw_circle(region.left_center());
}

void CaptureModeSession::OnLocatedEvent(ui::LocatedEvent* event,
                                        bool is_touch) {
  // No need to handle events if the current source is kFullscreen.
  const CaptureModeSource capture_source = controller_->source();
  if (capture_source == CaptureModeSource::kFullscreen)
    return;

  gfx::Point location = event->location();
  aura::Window* event_target = static_cast<aura::Window*>(event->target());
  aura::Window::ConvertPointToTarget(event_target, current_root_, &location);
  const bool is_event_on_capture_bar =
      CaptureModeBarView::GetBounds(current_root_).Contains(location);

  if (capture_source == CaptureModeSource::kWindow) {
    // Do not handle any event located on the capture mode bar.
    if (is_event_on_capture_bar)
      return;

    event->SetHandled();
    event->StopPropagation();

    switch (event->type()) {
      case ui::ET_MOUSE_MOVED:
      case ui::ET_TOUCH_PRESSED:
      case ui::ET_TOUCH_MOVED: {
        gfx::Point screen_location(event->location());
        ::wm::ConvertPointToScreen(event_target, &screen_location);
        capture_window_observer_->UpdateSelectedWindowAtPosition(
            screen_location);
        break;
      }
      case ui::ET_MOUSE_RELEASED:
      case ui::ET_TOUCH_RELEASED:
        if (GetSelectedWindow())
          controller_->PerformCapture();
        break;
      default:
        break;
    }
    return;
  }

  // Let the capture button handle any events it can handle first.
  if (ShouldCaptureLabelHandleEvent(event_target))
    return;

  // Allow events that are located on the capture mode bar to pass through so we
  // can click the buttons.
  if (!is_event_on_capture_bar) {
    event->SetHandled();
    event->StopPropagation();
  }

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_TOUCH_PRESSED:
      OnLocatedEventPressed(location, is_touch);
      break;
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_TOUCH_MOVED:
      OnLocatedEventDragged(location);
      break;
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_TOUCH_RELEASED:
      OnLocatedEventReleased(location);
      break;
    default:
      break;
  }
}

void CaptureModeSession::OnLocatedEventPressed(
    const gfx::Point& location_in_root,
    bool is_touch) {
  initial_location_in_root_ = location_in_root;
  previous_location_in_root_ = location_in_root;

  if (is_selecting_region_)
    return;

  // Calculate the position and anchor points of the current pressed event.
  fine_tune_position_ = FineTunePosition::kNone;
  // In the case of overlapping affordances, prioritize the bottomm right
  // corner, then the rest of the corners, then the edges.
  static const std::vector<FineTunePosition> drag_positions = {
      FineTunePosition::kBottomRight,  FineTunePosition::kBottomLeft,
      FineTunePosition::kTopLeft,      FineTunePosition::kTopRight,
      FineTunePosition::kBottomCenter, FineTunePosition::kLeftCenter,
      FineTunePosition::kTopCenter,    FineTunePosition::kRightCenter};

  const int hit_radius =
      is_touch ? kAffordanceCircleTouchHitRadiusDp : kAffordanceCircleRadiusDp;
  const int hit_radius_squared = hit_radius * hit_radius;
  for (FineTunePosition position : drag_positions) {
    const gfx::Point position_location =
        GetLocationForPosition(controller_->user_capture_region(), position);
    // If |location_in_root| is within |hit_radius| of |position_location| for
    // both x and y, then |position| is the current pressed down affordance.
    if ((position_location - location_in_root).LengthSquared() <=
        hit_radius_squared) {
      fine_tune_position_ = position;
      break;
    }
  }

  if (fine_tune_position_ == FineTunePosition::kNone) {
    // If the point is outside the capture region and not on the capture bar,
    // restart to the select phase.
    if (controller_->user_capture_region().Contains(location_in_root)) {
      fine_tune_position_ = FineTunePosition::kCenter;
    } else if (!CaptureModeBarView::GetBounds(current_root_)
                    .Contains(location_in_root)) {
      is_selecting_region_ = true;
      UpdateCaptureRegion(gfx::Rect(), /*is_resizing=*/true);
    }
    return;
  }

  anchor_points_ = GetAnchorPointsForPosition(fine_tune_position_);
}

void CaptureModeSession::OnLocatedEventDragged(
    const gfx::Point& location_in_root) {
  const gfx::Point previous_location_in_root = previous_location_in_root_;
  previous_location_in_root_ = location_in_root;

  // For the select phase, the select region is the rectangle formed by the
  // press location and the current location.
  if (is_selecting_region_) {
    UpdateCaptureRegion(
        GetRectEnclosingPoints({initial_location_in_root_, location_in_root}),
        /*is_resizing=*/true);
    return;
  }

  if (fine_tune_position_ == FineTunePosition::kNone)
    return;

  // For a reposition, offset the old select region by the difference between
  // the current location and the previous location, but do not let the select
  // region go offscreen.
  if (fine_tune_position_ == FineTunePosition::kCenter) {
    gfx::Rect new_capture_region = controller_->user_capture_region();
    new_capture_region.Offset(location_in_root - previous_location_in_root);
    new_capture_region.AdjustToFit(current_root_->bounds());
    UpdateCaptureRegion(new_capture_region, /*is_resizing=*/false);
    return;
  }

  // The new region is defined by the rectangle which encloses the anchor
  // point(s) and |location_in_root|.
  std::vector<gfx::Point> points = anchor_points_;
  DCHECK(!points.empty());
  points.push_back(location_in_root);
  UpdateCaptureRegion(GetRectEnclosingPoints(points), /*is_resizing=*/true);
}

void CaptureModeSession::OnLocatedEventReleased(
    const gfx::Point& location_in_root) {
  fine_tune_position_ = FineTunePosition::kNone;
  anchor_points_.clear();

  // Do a repaint to show the affordance circles. See UpdateCaptureRegion to see
  // how damage is calculated.
  gfx::Rect damage_region = controller_->user_capture_region();
  damage_region.Inset(
      gfx::Insets(-kAffordanceCircleRadiusDp - kCaptureRegionBorderStrokePx));
  layer()->SchedulePaint(damage_region);

  UpdateDimensionsLabelWidget(/*is_resizing=*/false);

  if (!is_selecting_region_)
    return;

  // After first release event, we advance to the next phase.
  is_selecting_region_ = false;
  UpdateCaptureLabelWidget();
}

void CaptureModeSession::UpdateCaptureRegion(
    const gfx::Rect& new_capture_region,
    bool is_resizing) {
  const gfx::Rect old_capture_region = controller_->user_capture_region();
  if (old_capture_region == new_capture_region)
    return;

  // Calculate the region that has been damaged and repaint the layer. Add some
  // extra padding to make sure the border and affordance circles are also
  // repainted.
  gfx::Rect damage_region = old_capture_region;
  damage_region.Union(new_capture_region);
  damage_region.Inset(
      gfx::Insets(-kAffordanceCircleRadiusDp - kCaptureRegionBorderStrokePx));
  layer()->SchedulePaint(damage_region);

  controller_->set_user_capture_region(new_capture_region);
  UpdateDimensionsLabelWidget(is_resizing);
  UpdateCaptureLabelWidget();
}

void CaptureModeSession::UpdateDimensionsLabelWidget(bool is_resizing) {
  const bool should_not_show =
      !is_resizing || controller_->source() != CaptureModeSource::kRegion ||
      controller_->user_capture_region().IsEmpty();
  if (should_not_show) {
    dimensions_label_widget_.reset();
    return;
  }

  if (!dimensions_label_widget_) {
    auto* parent = GetParentContainer(current_root_);
    dimensions_label_widget_ = std::make_unique<views::Widget>();
    dimensions_label_widget_->Init(
        CreateWidgetParams(parent, gfx::Rect(), "CaptureModeDimensionsLabel"));

    auto size_label = std::make_unique<views::Label>();
    auto* color_provider = AshColorProvider::Get();
    size_label->SetEnabledColor(color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
    size_label->SetBackground(views::CreateRoundedRectBackground(
        color_provider->GetBaseLayerColor(
            AshColorProvider::BaseLayerType::kTransparent80),
        kSizeLabelBorderRadius));
    size_label->SetAutoColorReadabilityEnabled(false);
    dimensions_label_widget_->SetContentsView(std::move(size_label));

    dimensions_label_widget_->Show();
    parent->StackChildBelow(dimensions_label_widget_->GetNativeWindow(),
                            capture_mode_bar_widget_.GetNativeWindow());
  }

  views::Label* size_label =
      static_cast<views::Label*>(dimensions_label_widget_->GetContentsView());

  const gfx::Rect capture_region = controller_->user_capture_region();
  size_label->SetText(base::UTF8ToUTF16(base::StringPrintf(
      "%d x %d", capture_region.width(), capture_region.height())));

  UpdateDimensionsLabelBounds();
}

void CaptureModeSession::UpdateDimensionsLabelBounds() {
  DCHECK(dimensions_label_widget_ &&
         dimensions_label_widget_->GetContentsView());

  gfx::Rect bounds(
      dimensions_label_widget_->GetContentsView()->GetPreferredSize());
  const gfx::Rect capture_region = controller_->user_capture_region();
  gfx::Rect screen_region = current_root_->bounds();

  bounds.set_width(bounds.width() + 2 * kSizeLabelHorizontalPadding);
  bounds.set_x(capture_region.CenterPoint().x() - bounds.width() / 2);
  bounds.set_y(capture_region.bottom() + kSizeLabelYDistanceFromRegionDp);

  // The dimension label should always be within the screen and at the bottom of
  // the capture region. If it does not fit below the bottom edge fo the region,
  // move it above the bottom edge into the capture region.
  screen_region.Inset(0, 0, 0, kSizeLabelYDistanceFromRegionDp);
  bounds.AdjustToFit(screen_region);

  dimensions_label_widget_->SetBounds(bounds);
}

std::vector<gfx::Point> CaptureModeSession::GetAnchorPointsForPosition(
    FineTunePosition position) {
  std::vector<gfx::Point> anchor_points;
  // For a vertex, the anchor point is the opposite vertex on the rectangle
  // (ex. bottom left vertex -> top right vertex anchor point). For an edge, the
  // anchor points are the two vertices of the opposite edge (ex. bottom edge ->
  // top left and top right anchor points).
  const gfx::Rect rect = controller_->user_capture_region();
  switch (position) {
    case FineTunePosition::kNone:
    case FineTunePosition::kCenter:
      break;
    case FineTunePosition::kTopLeft:
      anchor_points.push_back(rect.bottom_right());
      break;
    case FineTunePosition::kTopCenter:
      anchor_points.push_back(rect.bottom_left());
      anchor_points.push_back(rect.bottom_right());
      break;
    case FineTunePosition::kTopRight:
      anchor_points.push_back(rect.bottom_left());
      break;
    case FineTunePosition::kLeftCenter:
      anchor_points.push_back(rect.top_right());
      anchor_points.push_back(rect.bottom_right());
      break;
    case FineTunePosition::kRightCenter:
      anchor_points.push_back(rect.origin());
      anchor_points.push_back(rect.bottom_left());
      break;
    case FineTunePosition::kBottomLeft:
      anchor_points.push_back(rect.top_right());
      break;
    case FineTunePosition::kBottomCenter:
      anchor_points.push_back(rect.origin());
      anchor_points.push_back(rect.top_right());
      break;
    case FineTunePosition::kBottomRight:
      anchor_points.push_back(rect.origin());
      break;
  }
  DCHECK(!anchor_points.empty());
  DCHECK_LE(anchor_points.size(), 2u);
  return anchor_points;
}

void CaptureModeSession::UpdateCaptureLabelWidget() {
  if (!capture_label_widget_) {
    capture_label_widget_ = std::make_unique<views::Widget>();
    auto* parent = GetParentContainer(current_root_);
    capture_label_widget_->Init(
        CreateWidgetParams(parent, gfx::Rect(), "CaptureLabel"));
    capture_label_widget_->SetContentsView(
        std::make_unique<CaptureLabelView>(this));
    capture_label_widget_->Show();
  }

  CaptureLabelView* label_view =
      static_cast<CaptureLabelView*>(capture_label_widget_->GetContentsView());
  label_view->UpdateIconAndText();
  UpdateCaptureLabelWidgetBounds();
}

void CaptureModeSession::UpdateCaptureLabelWidgetBounds() {
  DCHECK(capture_label_widget_);

  // For fullscreen and window capture mode, the capture label is placed in the
  // middle of the screen. For region capture mode, if it's in select phase, the
  // capture label is also placed in the middle of the screen, and if it's in
  // fine tune phase, the capture label is ideally placed in the middle of the
  // capture region. If it cannot fit, then it will be placed slightly above or
  // below the capture region.
  gfx::Rect bounds(current_root_->bounds());
  const gfx::Rect capture_region = controller_->user_capture_region();
  const gfx::Size preferred_size =
      capture_label_widget_->GetContentsView()->GetPreferredSize();
  if (controller_->source() == CaptureModeSource::kRegion &&
      !is_selecting_region_ && !capture_region.IsEmpty()) {
    bounds = capture_region;

    // The capture region must be at least the size of |preferred_size| plus
    // some padding for the capture label to be centered inside it.
    gfx::Size capture_region_min_size = preferred_size;
    capture_region_min_size.Enlarge(kCaptureRegionMinimumPaddingDp,
                                    kCaptureRegionMinimumPaddingDp);
    if (bounds.width() > capture_region_min_size.width() &&
        bounds.height() > capture_region_min_size.height()) {
      bounds.ClampToCenteredSize(preferred_size);
    } else {
      // The capture region is too small for the capture label to be inside it.
      // Align |bounds| so that its horizontal centerpoint aligns with the
      // capture regions centerpoint.
      bounds.set_size(preferred_size);
      bounds.set_x(capture_region.CenterPoint().x() -
                   preferred_size.width() / 2);

      // Try to put the capture label slightly below the capture region. If it
      // does not fully fit in the root window bounds, place the capture label
      // slightly above.
      const int under_region_label_y =
          capture_region.bottom() + kCaptureButtonDistanceFromRegionDp;
      if (under_region_label_y + preferred_size.height() <
          current_root_->bounds().bottom()) {
        bounds.set_y(under_region_label_y);
      } else {
        bounds.set_y(capture_region.y() - kCaptureButtonDistanceFromRegionDp -
                     preferred_size.height());
      }
    }
  } else {
    bounds.ClampToCenteredSize(preferred_size);
  }

  capture_label_widget_->SetBounds(bounds);
}

bool CaptureModeSession::ShouldCaptureLabelHandleEvent(
    aura::Window* event_target) {
  if (!capture_label_widget_ ||
      capture_label_widget_->GetNativeWindow() != event_target) {
    return false;
  }

  CaptureLabelView* label_view =
      static_cast<CaptureLabelView*>(capture_label_widget_->GetContentsView());
  return label_view->ShouldHandleEvent();
}

}  // namespace ash
