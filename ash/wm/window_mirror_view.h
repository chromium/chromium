// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_MIRROR_VIEW_H_
#define ASH_WM_WINDOW_MIRROR_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
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
  WindowMirrorView(aura::Window* source, bool trilinear_filtering_on_init);
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
  aura::Window* source_;

  // The window which contains this mirror view.
  aura::Window* target_ = nullptr;

  // Retains ownership of the mirror layer tree. This is lazily initialized
  // the first time the view becomes visible.
  std::unique_ptr<ui::LayerTreeOwner> layer_owner_;

  // True if trilinear filtering should be performed on the layer in
  // InitLayerOwner().
  bool trilinear_filtering_on_init_;

  std::unique_ptr<aura::WindowOcclusionTracker::ScopedForceVisible>
      force_occlusion_tracker_visible_;

  DISALLOW_COPY_AND_ASSIGN(WindowMirrorView);
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_MIRROR_VIEW_H_
