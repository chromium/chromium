// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_session.h"

#include <memory>

#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/mru_window_tracker.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_canvas.h"

namespace ash {

namespace {

constexpr int kBorderStrokePx = 2;

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

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = parent;
  params.bounds = CaptureModeBarView::GetBounds(root);
  params.name = "CaptureModeBarWidget";

  capture_mode_bar_widget_.Init(std::move(params));
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
  layer()->SchedulePaint(layer()->bounds());
}

void CaptureModeSession::OnCaptureTypeChanged(CaptureModeType new_type) {
  capture_mode_bar_view_->OnCaptureTypeChanged(new_type);
}

void CaptureModeSession::OnPaintLayer(const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer()->size());

  auto* color_provider = AshColorProvider::Get();
  const SkColor dimming_color = color_provider->GetShieldLayerColor(
      AshColorProvider::ShieldLayerType::kShield40,
      AshColorProvider::AshColorMode::kDark);
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
  // TODO(afakhry): Fill in here.
}

void CaptureModeSession::OnTouchEvent(ui::TouchEvent* event) {
  // TODO(afakhry): Fill in here.
}

gfx::Rect CaptureModeSession::GetSelectedWindowBounds() const {
  auto* window = GetSelectedWindow();
  return window ? window->bounds() : gfx::Rect();
}

void CaptureModeSession::RefreshStackingOrder(aura::Window* parent_container) {
  DCHECK(parent_container);
  auto* widget_layer = capture_mode_bar_widget_.GetNativeWindow()->layer();
  auto* overlay_layer = layer();
  auto* parent_container_layer = parent_container->layer();

  DCHECK_EQ(parent_container_layer, overlay_layer->parent());
  DCHECK_EQ(parent_container_layer, widget_layer->parent());

  parent_container_layer->StackAtTop(overlay_layer);
  parent_container_layer->StackAtTop(widget_layer);
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

  canvas->FillRect(region, SK_ColorBLACK, SkBlendMode::kClear);
  canvas->FillRect(region, kCaptureRegionColor);

  if (!adjustable_region)
    return;

  // TODO(afakhry): For adjustable regions, we may change the colors. Also,
  // paint the drag points at the corners.
  region.Inset(-kBorderStrokePx, -kBorderStrokePx);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  // TODO(afakhry): Update to match the specs.
  flags.setColor(gfx::kGoogleBlue200);
  flags.setStrokeWidth(SkIntToScalar(kBorderStrokePx));
  canvas->DrawRect(region, flags);
}

}  // namespace ash
