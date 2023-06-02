// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_MIRROR_VIEW_H_
#define ASH_WM_WINDOW_MIRROR_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_occlusion_tracker.h"
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
 public:
  explicit WindowMirrorView(aura::Window* source,
                            bool show_non_client_view = false,
                            bool sync_bounds = false);

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
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
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
  raw_ptr<aura::Window, ExperimentalAsh> source_;

  // The window which contains this mirror view.
  raw_ptr<aura::Window, DanglingUntriaged | ExperimentalAsh> target_ = nullptr;

  // Retains ownership of the mirror layer tree. This is lazily initialized
  // the first time the view becomes visible.
  std::unique_ptr<ui::LayerTreeOwner> layer_owner_;

  // If true, shows the non client view in the mirror.
  const bool show_non_client_view_;

  // If true, synchronize the bounds from the source to the mirrored layers.
  const bool sync_bounds_;

  std::unique_ptr<aura::WindowOcclusionTracker::ScopedForceVisible>
      force_occlusion_tracker_visible_;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_MIRROR_VIEW_H_
