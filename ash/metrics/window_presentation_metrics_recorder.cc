// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/window_presentation_metrics_recorder.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/viz/common/frame_timing_details.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"

namespace ash {

namespace {

constexpr char kHistogramPrefix[] = "Ash.Window.InitToFirstPresentation.";

std::optional<std::string_view> GetAppTypeSuffix(chromeos::AppType app_type) {
  switch (app_type) {
    case chromeos::AppType::NON_APP:
      return std::nullopt;
    case chromeos::AppType::BROWSER:
      return "Browser";
    case chromeos::AppType::CHROME_APP:
      return "ChromeApp";
    case chromeos::AppType::ARC_APP:
      return "ArcApp";
    case chromeos::AppType::CROSTINI_APP:
      return "CrostiniApp";
    case chromeos::AppType::SYSTEM_APP:
      return "SystemApp";
  }
  NOTREACHED();
}

}  // namespace

WindowPresentationMetricsRecorder::WindowPresentationMetricsRecorder() {
  env_observation_.Observe(aura::Env::GetInstance());
}

WindowPresentationMetricsRecorder::~WindowPresentationMetricsRecorder() =
    default;

void WindowPresentationMetricsRecorder::OnWindowInitialized(
    aura::Window* window) {
  // The window type is set before Init(); the app type may be set later, so
  // it's read on show.
  if (window->GetType() != aura::client::WINDOW_TYPE_NORMAL) {
    return;
  }
  init_times_.emplace(window, base::TimeTicks::Now());
  window_observations_.AddObservation(window);
}

void WindowPresentationMetricsRecorder::OnWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  // Also called for ancestors and descendants of observed windows; only the
  // first show of a tracked window matters.
  auto it = init_times_.find(window);
  if (!visible || it == init_times_.end()) {
    return;
  }
  const base::TimeTicks init_time = it->second;
  StopTracking(window);

  const std::optional<std::string_view> suffix =
      GetAppTypeSuffix(window->GetProperty(chromeos::kAppTypeKey));
  aura::WindowTreeHost* host = window->GetHost();
  // A window shown into a hidden container (e.g. an inactive desk) isn't
  // presented, so its next frame would be misleading.
  if (!suffix || !host || !window->IsVisible()) {
    return;
  }
  host->compositor()->RequestSuccessfulPresentationTimeForNextFrame(
      base::BindOnce(&WindowPresentationMetricsRecorder::OnFramePresented,
                     weak_ptr_factory_.GetWeakPtr(), *suffix, init_time));
}

void WindowPresentationMetricsRecorder::OnWindowDestroying(
    aura::Window* window) {
  StopTracking(window);
}

void WindowPresentationMetricsRecorder::StopTracking(aura::Window* window) {
  init_times_.erase(window);
  window_observations_.RemoveObservation(window);
}

void WindowPresentationMetricsRecorder::OnFramePresented(
    std::string_view app_type_suffix,
    base::TimeTicks init_time,
    const viz::FrameTimingDetails& details) {
  base::UmaHistogramTimes(base::StrCat({kHistogramPrefix, app_type_suffix}),
                          details.presentation_feedback.timestamp - init_time);
}

}  // namespace ash
