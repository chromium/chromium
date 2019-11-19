// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CUSTOM_TAB_ARC_CUSTOM_TAB_VIEW_H_
#define ASH_CUSTOM_TAB_ARC_CUSTOM_TAB_VIEW_H_

#include <memory>

#include "ash/public/cpp/arc_custom_tab.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/views/view.h"

namespace exo {
class Surface;
}

namespace views {
class NativeViewHost;
}

namespace ash {

// A view-based implementation of ArcCustomTab which works in the classic
// environment.
class ArcCustomTabView : public ArcCustomTab,
                         public views::View,
                         public aura::WindowObserver {
 public:
  ArcCustomTabView(aura::Window* arc_app_window,
                   int32_t surface_id,
                   int32_t top_margin);
  ~ArcCustomTabView() override;

 private:
  // ArcCustomTab:
  void Attach(gfx::NativeView view) override;
  gfx::NativeView GetHostView() override;

  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void Layout() override;

  // aura::WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;

  // Ensures the window/layer orders for the NativeViewHost.
  void EnsureWindowOrders();

  // Converts the point from the given window to this view.
  void ConvertPointFromWindow(aura::Window* window, gfx::Point* point);

  // Tries to find the surface.
  exo::Surface* FindSurface();

  views::NativeViewHost* const host_;
  aura::Window* const arc_app_window_;
  const int32_t surface_id_, top_margin_;
  aura::Window* surface_window_ = nullptr;
  base::flat_set<aura::Window*> observed_surfaces_;
  aura::Window* native_view_container_ = nullptr;

  bool reorder_scheduled_ = false;

  base::WeakPtrFactory<ArcCustomTabView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcCustomTabView);
};

}  // namespace ash

#endif  // ASH_CUSTOM_TAB_ARC_CUSTOM_TAB_VIEW_H_
