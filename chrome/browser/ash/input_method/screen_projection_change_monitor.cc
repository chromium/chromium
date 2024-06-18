// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/screen_projection_change_monitor.h"

#include "ash/public/cpp/cast_config_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ui/display/manager/display_manager.h"

namespace ash::input_method {

ScreenProjectionChangeMonitor::ScreenProjectionChangeMonitor(
    OnScreenProjectionChangedCallback callback)
    : callback_(std::move(callback)) {
  // Unfortunately, the lifetimes of the observees are tricky so we cannot use a
  // ScopedObservation here. Manually manage the observer lifetimes.
  // Shell::Get() and CastConfigController::Get() might be null in tests.
  if (Shell::HasInstance()) {
    Shell::Get()->display_manager()->AddDisplayObserver(this);
    Shell::Get()->system_tray_notifier()->AddScreenSecurityObserver(this);
  }

  if (CastConfigController::Get()) {
    CastConfigController::Get()->AddObserver(this);
  }
}

ScreenProjectionChangeMonitor::~ScreenProjectionChangeMonitor() {
  if (CastConfigController::Get()) {
    CastConfigController::Get()->RemoveObserver(this);
  }

  if (Shell::HasInstance()) {
    Shell::Get()->system_tray_notifier()->RemoveScreenSecurityObserver(this);
    Shell::Get()->display_manager()->RemoveDisplayObserver(this);
  }
}

bool ScreenProjectionChangeMonitor::is_mirroring() const {
  return is_mirroring_;
}

void ScreenProjectionChangeMonitor::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (changed_metrics & display::DisplayObserver::DISPLAY_METRIC_MIRROR_STATE) {
    UpdateCastingAndMirroringState(
        is_casting_, Shell::Get()->display_manager()->IsInMirrorMode());
  }
}

void ScreenProjectionChangeMonitor::OnDevicesUpdated(
    const std::vector<SinkAndRoute>& devices) {
  bool casting_desktop = false;
  for (const auto& receiver : devices) {
    if (receiver.route.content_source == ContentSource::kDesktop) {
      casting_desktop = true;
      break;
    }
  }
  UpdateCastingAndMirroringState(casting_desktop, is_mirroring_);
}

void ScreenProjectionChangeMonitor::OnScreenAccessStart(
    base::OnceClosure stop_callback,
    const base::RepeatingClosure& source_callback,
    const std::u16string& access_app_name) {
  UpdateCastingAndMirroringState(true, is_mirroring_);
}

void ScreenProjectionChangeMonitor::OnScreenAccessStop() {
  UpdateCastingAndMirroringState(false, is_mirroring_);
}

bool ScreenProjectionChangeMonitor::IsProjecting() const {
  return is_casting_ || is_mirroring_;
}

void ScreenProjectionChangeMonitor::UpdateCastingAndMirroringState(
    bool is_casting,
    bool is_mirroring) {
  bool was_projecting = IsProjecting();
  is_casting_ = is_casting;
  is_mirroring_ = is_mirroring;
  bool is_projecting = IsProjecting();
  if (was_projecting != is_projecting) {
    callback_.Run(is_projecting);
  }
}

}  // namespace ash::input_method
