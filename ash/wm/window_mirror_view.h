// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_MIRROR_VIEW_H_
#define ASH_WM_WINDOW_MIRROR_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/raster_scale/raster_scale_layer_observer.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace aura {
class Window;
}

namespace ui {
class LayerTreeOwner;
}

namespace ash {

// A view that mirrors the client area of a single (source) window.
class ASH_EXPORT WindowMirrorView : public views::View,
                                    public aura::WindowObserver {
  METADATA_HEADER(WindowMirrorView, views::View)

 public:
  explicit WindowMirrorView(aura::Window* source,
                            bool show_non_client_view = false,
                            bool sync_bounds = true);

  WindowMirrorView(const WindowMirrorView&) = delete;
  WindowMirrorView& operator=(const WindowMirrorView&) = delete;

  ~WindowMirrorView() override;

  // Returns the source of the mirror.
  aura::Window* source() { return source_; }

  // Recreates |layer_owner_|.
  void RecreateMirrorLayers();

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;
  bool GetNeedsNotificationWhenVisibleBoundsChange() const override;
  void OnVisibleBoundsChanged() override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  ui::Layer* GetMirrorLayerForTesting();

 protected:
  virtual void InitLayerOwner();

  // Gets the root of the layer tree that was lifted from |source_| (and is now
  // a child of |this->layer()|).
  virtual ui::Layer* GetMirrorLayer();

 private:
  // Calculates the bounds of the client area of the Window in the widget
  // coordinate space.
  gfx::Rect GetClientAreaBounds() const;

  // The original window that is being represented by |this|.
  raw_ptr<aura::Window> source_;

  // The window which contains this mirror view.
  raw_ptr<aura::Window, DanglingUntriaged> target_ = nullptr;

  // Retains ownership of the mirror layer tree. This is lazily initialized
  // the first time the view becomes visible.
  std::unique_ptr<ui::LayerTreeOwner> layer_owner_;

  // If true, shows the non client view in the mirror.
  const bool show_non_client_view_;

  // If true, synchronize the bounds from the source to the mirrored layers.
  const bool sync_bounds_;

  std::unique_ptr<aura::WindowOcclusionTracker::ScopedForceVisible>
      force_occlusion_tracker_visible_;

  // While a window is mirrored, apply dynamic raster scale to the underlying
  // window. This is used in e.g. alt-tab and overview mode.
  std::optional<ScopedRasterScaleLayerObserverLock> raster_scale_observer_lock_;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_MIRROR_VIEW_H_
