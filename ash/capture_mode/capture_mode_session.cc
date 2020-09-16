// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_session.h"

#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/mru_window_tracker.h"
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
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

constexpr int kBorderStrokePx = 2;

// The visual radius of the drag affordance circles which are shown while
// resizing a drag region.
constexpr int kAffordanceCircleRadiusDp = 5;

// The hit radius of the drag affordance circles touch events.
constexpr int kAffordanceCircleTouchHitRadiusDp = 16;

constexpr int kSizeLabelYDistanceFromRegionDp = 8;

constexpr SkColor kRegionBorderColor = SK_ColorWHITE;

// Blue300 at 30%.
constexpr SkColor kCaptureRegionColor = SkColorSetA(gfx::kGoogleBlue300, 77);

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
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
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

  RefreshStackingOrder(parent);
}

CaptureModeSession::~CaptureModeSession() {
  Shell::Get()->RemovePreTargetHandler(this);
  SetMouseWarpEnabled(old_mouse_warp_status_);
}

aura::Window* CaptureModeSession::GetSelectedWindow() const {
  // Note that the capture bar widget is activatable, so we can't use
  // window_util::GetActiveWindow(). Instead, we use the MRU window tracker and
  // get the top-most window if any.
  auto mru_windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  return mru_windows.empty() ? nullptr : mru_windows[0];
}

void CaptureModeSession::OnCaptureSourceChanged(CaptureModeSource new_source) {
  capture_mode_bar_view_->OnCaptureSourceChanged(new_source);
  SetMouseWarpEnabled(new_source != CaptureModeSource::kRegion);
  UpdateCaptureRegionWidgets();
  layer()->SchedulePaint(layer()->bounds());
}

void CaptureModeSession::OnCaptureTypeChanged(CaptureModeType new_type) {
  capture_mode_bar_view_->OnCaptureTypeChanged(new_type);
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

void CaptureModeSession::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (!capture_button_widget_)
    return;

  DCHECK_EQ(static_cast<views::LabelButton*>(
                capture_button_widget_->GetContentsView()),
            sender);
  controller_->PerformCapture();  // |this| is destroyed here.
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

  region.Inset(-kBorderStrokePx, -kBorderStrokePx);
  canvas->FillRect(region, SK_ColorTRANSPARENT, SkBlendMode::kClear);
  canvas->DrawRect(gfx::RectF(region), kRegionBorderColor);

  if (is_select_phase_)
    return;

  // Do not show affordance circles when repositioning the whole region.
  if (fine_tune_position_ == FineTunePosition::kCenter)
    return;

  // Draw the drag affordance circles.
  // TODO(sammiequon): Draw a drop shadow for the affordance circles and the
  // border.
  cc::PaintFlags flags;
  flags.setColor(kRegionBorderColor);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawCircle(region.origin(), kAffordanceCircleRadiusDp, flags);
  canvas->DrawCircle(region.top_center(), kAffordanceCircleRadiusDp, flags);
  canvas->DrawCircle(region.top_right(), kAffordanceCircleRadiusDp, flags);
  canvas->DrawCircle(region.left_center(), kAffordanceCircleRadiusDp, flags);
  canvas->DrawCircle(region.right_center(), kAffordanceCircleRadiusDp, flags);
  canvas->DrawCircle(region.bottom_left(), kAffordanceCircleRadiusDp, flags);
  canvas->DrawCircle(region.bottom_center(), kAffordanceCircleRadiusDp, flags);
  canvas->DrawCircle(region.bottom_right(), kAffordanceCircleRadiusDp, flags);
}

void CaptureModeSession::OnLocatedEvent(ui::LocatedEvent* event,
                                        bool is_touch) {
  // No need to handle events if the current source is not region.
  if (controller_->source() != CaptureModeSource::kRegion)
    return;

  gfx::Point location = event->location();
  aura::Window* source = static_cast<aura::Window*>(event->target());
  aura::Window::ConvertPointToTarget(source, current_root_, &location);

  // Let the capture button handle any events within its bounds.
  if (capture_button_widget_ &&
      capture_button_widget_->GetNativeWindow()->bounds().Contains(location)) {
    return;
  }

  // Allow events that are located on the capture mode bar to pass through so we
  // can click the buttons.
  if (!CaptureModeBarView::GetBounds(current_root_).Contains(location)) {
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

  if (is_select_phase_)
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
      is_select_phase_ = true;
      UpdateCaptureRegion(gfx::Rect());
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
  // press location and the current locatiion.
  if (is_select_phase_) {
    UpdateCaptureRegion(
        GetRectEnclosingPoints({initial_location_in_root_, location_in_root}));
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
    UpdateCaptureRegion(new_capture_region);
    return;
  }

  // The new region is defined by the rectangle which encloses the anchor
  // point(s) and |location_in_root|.
  std::vector<gfx::Point> points = anchor_points_;
  DCHECK(!points.empty());
  points.push_back(location_in_root);
  UpdateCaptureRegion(GetRectEnclosingPoints(points));
}

void CaptureModeSession::OnLocatedEventReleased(
    const gfx::Point& location_in_root) {
  fine_tune_position_ = FineTunePosition::kNone;
  anchor_points_.clear();

  // Do a repaint to show the affordance circles. See UpdateCaptureRegion to see
  // how damage is calculated.
  gfx::Rect damage_region = controller_->user_capture_region();
  damage_region.Inset(
      gfx::Insets(-kAffordanceCircleRadiusDp - kBorderStrokePx));
  layer()->SchedulePaint(damage_region);

  if (!is_select_phase_)
    return;

  // After first release event, we advance to the next phase.
  is_select_phase_ = false;
  UpdateCaptureRegionWidgets();
}

void CaptureModeSession::UpdateCaptureRegion(
    const gfx::Rect& new_capture_region) {
  const gfx::Rect old_capture_region = controller_->user_capture_region();
  if (old_capture_region == new_capture_region)
    return;

  // Calculate the region that has been damaged and repaint the layer. Add some
  // extra padding to make sure the border and affordance circles are also
  // repainted.
  gfx::Rect damage_region = old_capture_region;
  damage_region.Union(new_capture_region);
  damage_region.Inset(
      gfx::Insets(-kAffordanceCircleRadiusDp - kBorderStrokePx));
  layer()->SchedulePaint(damage_region);

  controller_->set_user_capture_region(new_capture_region);
  UpdateCaptureRegionWidgets();
}

void CaptureModeSession::UpdateCaptureRegionWidgets() {
  // TODO(sammiequon): The dimensons label is always shown and the capture
  // button label is always shown in the fine tune stage. Update this to match
  // the specs.
  const bool show = controller_->source() == CaptureModeSource::kRegion;
  if (!show) {
    dimensions_label_widget_.reset();
    capture_button_widget_.reset();
    return;
  }

  // TODO(sammiequon): Add styling to the two widget content views. Also, the
  // widgets should be repositioned if the region is too small or too close to
  // the edge.
  const gfx::Rect capture_region = controller_->user_capture_region();
  if (!dimensions_label_widget_) {
    auto* parent = GetParentContainer(current_root_);
    dimensions_label_widget_ = std::make_unique<views::Widget>();
    dimensions_label_widget_->Init(
        CreateWidgetParams(parent, gfx::Rect(), "CaptureModeSizeLabel"));
    dimensions_label_widget_->SetContentsView(std::make_unique<views::Label>());
    dimensions_label_widget_->Show();
    parent->StackChildBelow(dimensions_label_widget_->GetNativeWindow(),
                            capture_mode_bar_widget_.GetNativeWindow());
  }

  // Update the location of the size label. It is in the center of the region
  // horizontally and slightly below the region vertically.
  views::Label* size_label =
      static_cast<views::Label*>(dimensions_label_widget_->GetContentsView());
  size_label->SetText(base::UTF8ToUTF16(base::StringPrintf(
      "%d x %d", capture_region.width(), capture_region.height())));
  gfx::Rect dimensions_label_widget_bounds(size_label->GetPreferredSize());
  dimensions_label_widget_bounds.set_x(capture_region.CenterPoint().x() -
                                       dimensions_label_widget_bounds.width() /
                                           2);
  dimensions_label_widget_bounds.set_y(capture_region.bottom() +
                                       kSizeLabelYDistanceFromRegionDp);
  dimensions_label_widget_->SetBounds(dimensions_label_widget_bounds);

  if (!capture_button_widget_ && !is_select_phase_) {
    auto* parent = GetParentContainer(current_root_);
    capture_button_widget_ = std::make_unique<views::Widget>();
    capture_button_widget_->Init(
        CreateWidgetParams(parent, gfx::Rect(), "CaptureModeButton"));
    // TODO(sammiequon): Add the localized label.
    auto label_button =
        std::make_unique<views::LabelButton>(this, base::string16());
    label_button->SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(controller_->type() == CaptureModeType::kImage
                                  ? kCaptureModeImageIcon
                                  : kCaptureModeVideoIcon,
                              SK_ColorBLACK));
    capture_button_widget_->SetContentsView(std::move(label_button));
    capture_button_widget_->Show();
    parent->StackChildBelow(capture_button_widget_->GetNativeWindow(),
                            capture_mode_bar_widget_.GetNativeWindow());
  }

  if (!capture_button_widget_)
    return;

  // Update the location of the capture button.
  views::LabelButton* capture_button = static_cast<views::LabelButton*>(
      capture_button_widget_->GetContentsView());
  gfx::Rect capture_button_widget_bounds = capture_region;
  capture_button_widget_bounds.ClampToCenteredSize(
      capture_button->GetPreferredSize());
  capture_button_widget_->SetBounds(capture_button_widget_bounds);
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

}  // namespace ash
