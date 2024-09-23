// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_H_
#define ASH_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_H_

#include <memory>
#include <optional>

#include "ash/wm/overview/overview_types.h"
#include "ui/compositor/animation_throughput_reporter.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class ImplicitAnimationObserver;
class LayerAnimator;
class ScopedLayerAnimationSettings;
}  // namespace ui

namespace ash {

// ScopedOverviewAnimationSettings correctly configures the animation
// settings for an aura::Window or ui::LayerAnimator given an
// OverviewAnimationType.
class ScopedOverviewAnimationSettings {
 public:
  ScopedOverviewAnimationSettings(OverviewAnimationType animation_type,
                                  aura::Window* window);
  ScopedOverviewAnimationSettings(OverviewAnimationType animation_type,
                                  ui::LayerAnimator* animator);

  ScopedOverviewAnimationSettings(const ScopedOverviewAnimationSettings&) =
      delete;
  ScopedOverviewAnimationSettings& operator=(
      const ScopedOverviewAnimationSettings&) = delete;

  ~ScopedOverviewAnimationSettings();

  void AddObserver(ui::ImplicitAnimationObserver* observer);
  void CacheRenderSurface();
  void DeferPaint();
  void TrilinearFiltering();
  ui::LayerAnimator* GetAnimator();

 private:
  // The managed animation settings.
  std::unique_ptr<ui::ScopedLayerAnimationSettings> animation_settings_;

  // Report smoothness of close animation.
  std::optional<ui::AnimationThroughputReporter> close_reporter_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_H_
