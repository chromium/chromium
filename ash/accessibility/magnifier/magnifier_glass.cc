// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/magnifier/magnifier_glass.h"

#include "ash/shell.h"
#include "base/check_op.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Inset on the zoom filter.
constexpr int kZoomInset = 0;
// Vertical offset between the center of the magnifier and the tip of the
// pointer. TODO(jdufault): The vertical offset should only apply to the window
// location, not the magnified contents. See crbug.com/637617.
constexpr int kVerticalOffset = 0;

// Name of the magnifier window.
constexpr char kMagniferGlassWindowName[] = "MagnifierGlassWindow";

int GetShadowOffset(const MagnifierGlass::Params& params) {
  return std::max(params.bottom_shadow.y(), params.top_shadow.y());
}

int GetShadowThickness(const MagnifierGlass::Params& params) {
  return std::max(params.bottom_shadow.blur(), params.top_shadow.blur());
}

gfx::Size GetWindowSize(const MagnifierGlass::Params& params) {
  // The diameter of the window is the diameter of the magnifier, border and
  // shadow combined. We apply the larger shadow offset on all sides, despite
  // the shadow offsets potentially being unequal, so as to keep the circle
  // centered in the view and keep calculations (border rendering and content
  // masking) simpler.
  int window_diameter = (params.radius + params.border_size +
                         GetShadowThickness(params) + GetShadowOffset(params)) *
                        2;
  return gfx::Size(window_diameter, window_diameter);
}

gfx::Rect GetBounds(const MagnifierGlass::Params& params,
                    const gfx::Point& point) {
  gfx::Size size = GetWindowSize(params);
  gfx::Point origin(point.x() - (size.width() / 2),
                    point.y() - (size.height() / 2) - kVerticalOffset);
  return gfx::Rect(origin, size);
}

}  // namespace

// The border renderer draws the border as well as outline on both the outer and
// inner radius to increase visibility. The border renderer also handles drawing
// the shadow.
class MagnifierGlass::BorderRenderer : public ui::LayerDelegate {
 public:
  BorderRenderer(const gfx::Rect& window_bounds,
                 const MagnifierGlass::Params& params)
      : magnifier_window_bounds_(window_bounds), params_(params) {
    magnifier_shadows_.push_back(params_.bottom_shadow);
    magnifier_shadows_.push_back(params_.top_shadow);
  }
  BorderRenderer(const BorderRenderer&) = delete;
  BorderRenderer& operator=(const BorderRenderer&) = delete;
  ~BorderRenderer() override = default;

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, magnifier_window_bounds_.size());

    // Draw the shadow.
    cc::PaintFlags shadow_flags;
    shadow_flags.setAntiAlias(true);
    shadow_flags.setColor(SK_ColorTRANSPARENT);
    shadow_flags.setLooper(gfx::CreateShadowDrawLooper(magnifier_shadows_));
    gfx::Rect shadow_bounds(magnifier_window_bounds_.size());
    recorder.canvas()->DrawCircle(shadow_bounds.CenterPoint(),
                                  shadow_bounds.width() / 2 -
                                      GetShadowThickness(params_) -
                                      GetShadowOffset(params_),
                                  shadow_flags);

    // The radius of the magnifier and its border.
    const int magnifier_radius = params_.radius + params_.border_size;

    // Clear the shadow for the magnified area.
    cc::PaintFlags mask_flags;
    mask_flags.setAntiAlias(true);
    mask_flags.setBlendMode(SkBlendMode::kClear);
    mask_flags.setStyle(cc::PaintFlags::kFill_Style);
    recorder.canvas()->DrawCircle(
        magnifier_window_bounds_.CenterPoint(),
        magnifier_radius - params_.border_outline_thickness / 2, mask_flags);

    cc::PaintFlags border_flags;
    border_flags.setAntiAlias(true);
    border_flags.setStyle(cc::PaintFlags::kStroke_Style);

    // Draw the inner border.
    border_flags.setStrokeWidth(params_.border_size);
    border_flags.setColor(params_.border_color);
    recorder.canvas()->DrawCircle(magnifier_window_bounds_.CenterPoint(),
                                  magnifier_radius - params_.border_size / 2,
                                  border_flags);

    // Draw border outer outline and then draw the border inner outline.
    border_flags.setStrokeWidth(params_.border_outline_thickness);
    border_flags.setColor(params_.border_outline_color);
    recorder.canvas()->DrawCircle(
        magnifier_window_bounds_.CenterPoint(),
        magnifier_radius - params_.border_outline_thickness / 2, border_flags);
    recorder.canvas()->DrawCircle(magnifier_window_bounds_.CenterPoint(),
                                  magnifier_radius - params_.border_size +
                                      params_.border_outline_thickness / 2,
                                  border_flags);
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  const gfx::Rect magnifier_window_bounds_;
  const Params params_;
  std::vector<gfx::ShadowValue> magnifier_shadows_;
};

MagnifierGlass::MagnifierGlass(Params params) : params_(std::move(params)) {}

MagnifierGlass::~MagnifierGlass() {
  CloseMagnifierWindow();
  CHECK(!views::WidgetObserver::IsInObserverList());
}

void MagnifierGlass::ShowFor(aura::Window* root_window,
                             const gfx::Point& location_in_root) {
  if (!host_widget_) {
    CreateMagnifierWindow(root_window, location_in_root);
    return;
  }

  if (root_window != host_widget_->GetNativeView()->GetRootWindow()) {
    CloseMagnifierWindow();
    CreateMagnifierWindow(root_window, location_in_root);
    return;
  }

  host_widget_->SetBounds(GetBounds(params_, location_in_root));
}

void MagnifierGlass::Close() {
  CloseMagnifierWindow();
}

void MagnifierGlass::OnWindowDestroying(aura::Window* window) {
  CloseMagnifierWindow();
}

void MagnifierGlass::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget, host_widget_);
  RemoveZoomWidgetObservers();
  host_widget_ = nullptr;
}

void MagnifierGlass::CreateMagnifierWindow(aura::Window* root_window,
                                           const gfx::Point& location_in_root) {
  if (host_widget_ || !root_window)
    return;

  root_window->AddObserver(this);

  host_widget_ = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.accept_events = false;
  params.bounds = GetBounds(params_, location_in_root);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.name = kMagniferGlassWindowName;
  params.parent = root_window;
  host_widget_->Init(std::move(params));
  host_widget_->set_focus_on_creation(false);
  host_widget_->Show();

  ui::Layer* root_layer = host_widget_->GetNativeView()->layer();

  const gfx::Size window_size = GetWindowSize(params_);
  const gfx::Rect window_bounds = gfx::Rect(window_size);

  zoom_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  zoom_layer_->SetBounds(window_bounds);
  zoom_layer_->SetBackgroundZoom(params_.scale, kZoomInset);
  zoom_layer_->SetFillsBoundsOpaquely(false);
  root_layer->Add(zoom_layer_.get());

  // Create a rounded rect clip, so that only we see a circle of the zoomed
  // content. This circle radius should match that of the drawn border.
  const gfx::RoundedCornersF kRoundedCorners{
      static_cast<float>(params_.radius)};
  zoom_layer_->SetRoundedCornerRadius(kRoundedCorners);
  gfx::Rect clip_rect = window_bounds;
  clip_rect.ClampToCenteredSize(
      gfx::Size(params_.radius * 2, params_.radius * 2));
  zoom_layer_->SetClipRect(clip_rect);

  border_layer_ = std::make_unique<ui::Layer>();
  border_layer_->SetBounds(window_bounds);
  border_renderer_ = std::make_unique<BorderRenderer>(window_bounds, params_);
  border_layer_->set_delegate(border_renderer_.get());
  border_layer_->SetFillsBoundsOpaquely(false);
  root_layer->Add(border_layer_.get());

  host_widget_->AddObserver(this);
}

void MagnifierGlass::CloseMagnifierWindow() {
  if (host_widget_) {
    RemoveZoomWidgetObservers();
    host_widget_->Close();
    host_widget_ = nullptr;
  }
}

void MagnifierGlass::RemoveZoomWidgetObservers() {
  DCHECK(host_widget_);
  host_widget_->RemoveObserver(this);
  aura::Window* root_window = host_widget_->GetNativeView()->GetRootWindow();
  DCHECK(root_window);
  root_window->RemoveObserver(this);
}

}  // namespace ash
