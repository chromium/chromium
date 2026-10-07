// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_WINDOW_PRESENTATION_METRICS_RECORDER_H_
#define ASH_METRICS_WINDOW_PRESENTATION_METRICS_RECORDER_H_

#include <string_view>

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace viz {
class FrameTimingDetails;
}  // namespace viz

namespace ash {

// Records Ash.Window.InitToFirstPresentation.{AppType}: the time from an app
// window's aura::Window initialization to the first frame presented after it
// is shown. For browser windows this includes building the browser UI.
class ASH_EXPORT WindowPresentationMetricsRecorder
    : public aura::EnvObserver,
      public aura::WindowObserver {
 public:
  WindowPresentationMetricsRecorder();
  WindowPresentationMetricsRecorder(const WindowPresentationMetricsRecorder&) =
      delete;
  WindowPresentationMetricsRecorder& operator=(
      const WindowPresentationMetricsRecorder&) = delete;
  ~WindowPresentationMetricsRecorder() override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroying(aura::Window* window) override;

 private:
  void StopTracking(aura::Window* window);
  void OnFramePresented(std::string_view app_type_suffix,
                        base::TimeTicks init_time,
                        const viz::FrameTimingDetails& details);

  // Init times of normal windows that haven't been shown yet.
  base::flat_map<aura::Window*, base::TimeTicks> init_times_;

  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observation_{this};
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};

  base::WeakPtrFactory<WindowPresentationMetricsRecorder> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_METRICS_WINDOW_PRESENTATION_METRICS_RECORDER_H_
