// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rounded_display/rounded_display_provider.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "ash/display/window_tree_host_manager.h"
#include "ash/rounded_display/rounded_display_gutter.h"
#include "ash/rounded_display/rounded_display_gutter_factory.h"
#include "ash/rounded_display/rounded_display_host.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace {

using Gutters = std::vector<RoundedDisplayGutter*>;

bool IsRadiiHorizontallyUniform(const gfx::RoundedCornersF& radii) {
  return radii.upper_left() == radii.upper_right() &&
         radii.lower_left() == radii.lower_right();
}

bool IsRadiiVerticallyUniform(const gfx::RoundedCornersF& radii) {
  return radii.upper_left() == radii.lower_left() &&
         radii.upper_right() == radii.lower_right();
}

bool IsRadiiValid(const gfx::RoundedCornersF radii) {
  return !radii.IsEmpty() &&
         (IsRadiiHorizontallyUniform(radii) || IsRadiiVerticallyUniform(radii));
}

// Returns the display's panel size in pixels.
gfx::Size GetPanelSizeInPixels(const display::Display& display) {
  gfx::Size display_size_in_pixels = display.GetSizeInPixel();

  if (display.panel_rotation() == display::Display::ROTATE_90 ||
      display.panel_rotation() == display::Display::ROTATE_270) {
    display_size_in_pixels.Transpose();
  }

  return display_size_in_pixels;
}

const display::Display& GetDisplay(int64_t display_id) {
  return Shell::Get()->display_manager()->GetDisplayForId(display_id);
}

aura::Window* GetRootWindow(int64_t display_id) {
  return Shell::GetRootWindowForDisplayId(display_id);
}

}  // namespace

// static
std::unique_ptr<RoundedDisplayProvider> RoundedDisplayProvider::Create(
    int64_t display_id) {
  auto gutter_factory = std::make_unique<RoundedDisplayGutterFactory>();

  return std::make_unique<RoundedDisplayProvider>(display_id,
                                                  std::move(gutter_factory));
}

RoundedDisplayProvider::RoundedDisplayProvider(
    int64_t display_id,
    std::unique_ptr<RoundedDisplayGutterFactory> gutter_factory)
    : display_id_(display_id), gutter_factory_(std::move(gutter_factory)) {}

RoundedDisplayProvider::~RoundedDisplayProvider() {
  if (host_) {
    aura::Window* root_window = GetRootWindow(display_id_);
    DCHECK(root_window) << "The provider needs to be destroyed first before "
                           "the root window is destroyed";

    // `host_window_` needs to outlive the `host_`.
    DCHECK(root_window->Contains(host_window_.get()));
    root_window->RemoveChild(host_window_.get());
  }
}

void RoundedDisplayProvider::Init(const gfx::RoundedCornersF& panel_radii,
                                  Strategy strategy) {
  if (host_) {
    NOTREACHED() << "Provider is already initialized";
  }

  DCHECK(IsRadiiValid(panel_radii));

  current_panel_radii_ = panel_radii;
  strategy_ = strategy;

  const display::Display& display = GetDisplay(display_id_);
  CreateGutters(display, panel_radii);

  InitializeHost();
}

void RoundedDisplayProvider::InitializeHost() {
  host_ = std::make_unique<RoundedDisplayHost>(
      base::BindRepeating(&RoundedDisplayProvider::GetGuttersInDrawOrder,
                          weak_ptr_factory_.GetWeakPtr()));

  // TODO(zoraiznaeem): Change the default color to transparent when we fail to
  // identify surface.
  host_window_ = std::make_unique<aura::Window>(/*delegate=*/nullptr);
  host_window_->set_owned_by_parent(false);
  host_window_->Init(ui::LAYER_SOLID_COLOR);
  host_window_->SetName("RoundedDisplayHost");
  host_window_->SetEventTargetingPolicy(aura::EventTargetingPolicy::kNone);
  host_window_->SetTransparent(true);
  host_window_->Show();

  aura::Window* root_window = GetRootWindow(display_id_);
  root_window->AddChild(host_window_.get());

  host_->Init(host_window_.get());
}

void RoundedDisplayProvider::UpdateHostParent() {
  DCHECK(host_) << "Call Init() before calling UpdateHostParent";

  aura::Window* new_display_root = GetRootWindow(display_id_);
  aura::Window* current_display_root = host_window_->GetRootWindow();

  if (new_display_root == current_display_root) {
    return;
  }

  current_display_root->RemoveChild(host_window_.get());
  new_display_root->AddChild(host_window_.get());
}

bool RoundedDisplayProvider::UpdateRoundedDisplaySurface() {
  DCHECK(host_) << "Call Init() before calling UpdateRoundedDisplay";

  const display::Display& display = GetDisplay(display_id_);

  if (!ShouldSubmitNewCompositorFrame(display)) {
    return false;
  }

  // We need to adjust the bounds of host_window to account for the 1px offset
  // introduced for certain device scale factor values due to conversion between
  // dip and pixel values.
  host_window_->SetBounds(screen_util::SnapBoundsToDisplayEdge(
      gfx::Rect(display.bounds().size()), GetRootWindow(display_id_)));

  gfx::Rect content_rect(host_window_->bounds());
  gfx::Rect damage_rect;

  // Submit a compositor frame to update the surface. Textures will be reused,
  // unless we decide to create new gutters, otherwise the positions of the
  // existing textures will be updated.
  host_->UpdateSurface(content_rect, damage_rect, /*synchronous_draw=*/true);

  current_device_scale_factor_ = display.device_scale_factor();
  current_logical_rotation_ = display.rotation();

  return true;
}

bool RoundedDisplayProvider::ShouldSubmitNewCompositorFrame(
    const display::Display& display) const {
  return display.device_scale_factor() != current_device_scale_factor_ ||
         display.rotation() != current_logical_rotation_;
}

void RoundedDisplayProvider::GetGuttersInDrawOrder(Gutters& gutters) const {
  for (const auto& gutter : overlay_gutters_) {
    gutters.push_back(gutter.get());
  }
}

bool RoundedDisplayProvider::CreateGutters(
    const display::Display& display,
    const gfx::RoundedCornersF& panel_radii) {
  gfx::Size panel_size = GetPanelSizeInPixels(display);

  // Scanout direction is left to right wrt to the panel. Therefore horizontal
  // gutters are in the direction of scanout and vertical gutters are in the
  // other direction.
  bool create_vertical_gutters = strategy_ != Strategy::kScanout;

  overlay_gutters_ = gutter_factory_->CreateOverlayGutters(
      panel_size, panel_radii, create_vertical_gutters);

  return true;
}

}  // namespace ash
