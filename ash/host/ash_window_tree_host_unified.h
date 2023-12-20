// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_ASH_WINDOW_TREE_HOST_UNIFIED_H_
#define ASH_HOST_ASH_WINDOW_TREE_HOST_UNIFIED_H_

#include <vector>

#include "ash/host/ash_window_tree_host_platform.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"

namespace ash {

class AshWindowTreeHostDelegate;

// A WTH used for unified desktop mode. This creates an offscreen
// compositor whose texture will be copied into each displays'
// compositor.
class AshWindowTreeHostUnified : public AshWindowTreeHostPlatform,
                                 public aura::WindowObserver {
 public:
  AshWindowTreeHostUnified(const gfx::Rect& initial_bounds,
                           AshWindowTreeHostDelegate* delegate,
                           size_t compositor_memory_limit_mb = 0);

  AshWindowTreeHostUnified(const AshWindowTreeHostUnified&) = delete;
  AshWindowTreeHostUnified& operator=(const AshWindowTreeHostUnified&) = delete;

  ~AshWindowTreeHostUnified() override;

 private:
  // AshWindowTreeHost:
  void PrepareForShutdown() override;
  void RegisterMirroringHost(AshWindowTreeHost* mirroring_ash_host) override;
  void UpdateCursorConfig() override;
  void ClearCursorConfig() override;

  // aura::WindowTreeHost:
  void SetCursorNative(gfx::NativeCursor cursor) override;
  void OnCursorVisibilityChangedNative(bool show) override;

  // ui::PlatformWindow:
  void OnBoundsChanged(const BoundsChange& bounds) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  std::vector<raw_ptr<AshWindowTreeHost, VectorExperimental>> mirroring_hosts_;
};

}  // namespace ash

#endif  // ASH_HOST_ASH_WINDOW_TREE_HOST_UNIFIED_H_
