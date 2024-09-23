// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_mirror_view.h"

#include <algorithm>

#include "ash/wm/desks/desks_util.h"
#include "ash/wm/raster_scale/raster_scale_layer_observer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

void EnsureAllChildrenAreVisible(ui::Layer* layer) {
  for (ui::Layer* child : layer->children()) {
    EnsureAllChildrenAreVisible(child);
  }

  layer->SetVisible(true);
  layer->SetOpacity(1);
}

}  // namespace

WindowMirrorView::WindowMirrorView(aura::Window* source,
                                   bool show_non_client_view,
                                   bool sync_bounds)
    : source_(source),
      show_non_client_view_(show_non_client_view),
      sync_bounds_(sync_bounds) {
  source_->AddObserver(this);
  DCHECK(source);
}

WindowMirrorView::~WindowMirrorView() {
  // Make sure |source_| has outlived |this|. See crbug.com/681207
  if (source_) {
    DCHECK(source_->layer());
    source_->RemoveObserver(this);
  }
}

void WindowMirrorView::RecreateMirrorLayers() {
  if (layer_owner_)
    layer_owner_.reset();

  InitLayerOwner();
}

void WindowMirrorView::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(source_, window);
  if (source_ == window) {
    source_->RemoveObserver(this);
    source_ = nullptr;
  }
}

gfx::Size WindowMirrorView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return show_non_client_view_ ? source_->bounds().size()
                               : GetClientAreaBounds().size();
}

void WindowMirrorView::Layout(PassKey) {
  // If |layer_owner_| hasn't been initialized (|this| isn't on screen), no-op.
  if (!layer_owner_ || !source_)
    return;

  // Position at 0, 0.
  GetMirrorLayer()->SetBounds(gfx::Rect(GetMirrorLayer()->bounds().size()));

  if (show_non_client_view_) {
    GetMirrorLayer()->SetTransform(gfx::Transform());
    return;
  }

  gfx::Transform transform;
  gfx::Rect client_area_bounds = GetClientAreaBounds();
  // Scale if necessary.
  if (size() != source_->bounds().size()) {
    const float scale_x =
        width() / static_cast<float>(client_area_bounds.width());
    const float scale_y =
        height() / static_cast<float>(client_area_bounds.height());
    transform.Scale(scale_x, scale_y);
  }
  // Reposition such that the client area is the only part visible.
  transform.Translate(-client_area_bounds.x(), -client_area_bounds.y());
  GetMirrorLayer()->SetTransform(transform);
}

bool WindowMirrorView::GetNeedsNotificationWhenVisibleBoundsChange() const {
  return true;
}

void WindowMirrorView::OnVisibleBoundsChanged() {
  if (!layer_owner_ && !GetVisibleBounds().IsEmpty())
    InitLayerOwner();
}

void WindowMirrorView::AddedToWidget() {
  // Set and insert the new target window associated with this mirror view.
  target_ = GetWidget()->GetNativeWindow();
  target_->TrackOcclusionState();

  if (source_) {
    // Force the occlusion tracker to treat the source as visible.
    force_occlusion_tracker_visible_ =
        std::make_unique<aura::WindowOcclusionTracker::ScopedForceVisible>(
            source_);
  } else {
    force_occlusion_tracker_visible_.reset();
  }
}

void WindowMirrorView::RemovedFromWidget() {
  target_ = nullptr;
}

ui::Layer* WindowMirrorView::GetMirrorLayerForTesting() {
  return GetMirrorLayer();
}

void WindowMirrorView::InitLayerOwner() {
  layer_owner_ = wm::MirrorLayers(source_, sync_bounds_);
  layer_owner_->root()->SetOpacity(1.f);

  SetPaintToLayer();

  ui::Layer* mirror_layer = GetMirrorLayer();
  layer()->Add(mirror_layer);
  // This causes us to clip the non-client areas of the window.
  layer()->SetMasksToBounds(true);

  // Some extra work is needed when the source window is minimized, tucked
  // offscreen or is on an inactive desk.
  if (window_util::IsMinimizedOrTucked(source_) ||
      !desks_util::BelongsToActiveDesk(source_)) {
    // If the window is already visible, don't set its raster scale. This means
    // that, for example, alt-tab window cycling won't update the raster scale
    // of visible windows.
    // TODO(crbug.com/40279149): Consider using compositor occlusion information
    // to determine if raster scale can be set lower, e.g. on alt-tab.
    raster_scale_observer_lock_.emplace(
        (new RasterScaleLayerObserver(target_, mirror_layer, source_))->Lock());

    EnsureAllChildrenAreVisible(mirror_layer);
  }

  DeprecatedLayoutImmediately();
}

ui::Layer* WindowMirrorView::GetMirrorLayer() {
  return layer_owner_->root();
}

gfx::Rect WindowMirrorView::GetClientAreaBounds() const {
  DCHECK(!show_non_client_view_);

  const int inset = source_->GetProperty(aura::client::kTopViewInset);
  if (inset > 0) {
    gfx::Rect bounds(source_->bounds().size());
    bounds.Inset(gfx::Insets::TLBR(inset, 0, 0, 0));
    return bounds;
  }
  // The source window may not have a widget in unit tests.
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(source_);
  if (!widget || !widget->client_view())
    return gfx::Rect();
  views::View* client_view = widget->client_view();
  return client_view->ConvertRectToWidget(client_view->GetLocalBounds());
}

BEGIN_METADATA(WindowMirrorView)
END_METADATA

}  // namespace ash
