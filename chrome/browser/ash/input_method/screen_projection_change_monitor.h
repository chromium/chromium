// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_SCREEN_PROJECTION_CHANGE_MONITOR_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_SCREEN_PROJECTION_CHANGE_MONITOR_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/cast_config_controller.h"
#include "ash/system/privacy/screen_security_observer.h"
#include "ui/display/display_observer.h"

namespace ash::input_method {

// Monitors changes to the screen projection (casting or mirroring) status.
// Assumes the default state is no casting nor mirroring.
class ASH_EXPORT ScreenProjectionChangeMonitor
    : public display::DisplayObserver,
      public CastConfigController::Observer,
      public ScreenSecurityObserver {
 public:
  using OnScreenProjectionChangedCallback =
      base::RepeatingCallback<void(bool is_projected)>;

  explicit ScreenProjectionChangeMonitor(
      OnScreenProjectionChangedCallback callback);
  ~ScreenProjectionChangeMonitor() override;

  bool is_mirroring() const;

 private:
  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // CastConfigController::Observer:
  void OnDevicesUpdated(const std::vector<SinkAndRoute>& devices) override;

  // ScreenSecurityObserver:
  void OnScreenAccessStart(base::OnceClosure stop_callback,
                           const base::RepeatingClosure& source_callback,
                           const std::u16string& access_app_name) override;
  void OnScreenAccessStop() override;

  bool IsProjecting() const;

  void UpdateCastingAndMirroringState(bool is_casting, bool is_mirroring);

  OnScreenProjectionChangedCallback callback_;

  bool is_casting_ = false;
  bool is_mirroring_ = false;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_SCREEN_PROJECTION_CHANGE_MONITOR_H_
