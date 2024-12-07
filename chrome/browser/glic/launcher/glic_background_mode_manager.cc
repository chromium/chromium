// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/launcher/glic_background_mode_manager.h"

#include <memory>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/launcher/glic_controller.h"
#include "chrome/browser/glic/launcher/glic_status_icon.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"

namespace glic {

GlicBackgroundModeManager::GlicBackgroundModeManager(StatusTray* status_tray)
    : status_tray_(status_tray) {
  configuration_ = std::make_unique<GlicConfiguration>(this);
  OnEnabledChanged(configuration_->IsEnabled());

  controller_ = std::make_unique<GlicController>();
}

GlicBackgroundModeManager::~GlicBackgroundModeManager() = default;

void GlicBackgroundModeManager::OnEnabledChanged(bool enabled) {
  if (enabled_ == enabled) {
    return;
  }

  enabled_ = enabled;
  if (enabled_) {
    EnterBackgroundMode();
  } else {
    ExitBackgroundMode();
  }
  EnableLaunchOnStartup(enabled_);
}

void GlicBackgroundModeManager::EnterBackgroundMode() {
  keep_alive_ = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::GLIC_LAUNCHER, KeepAliveRestartOption::ENABLED);

  status_icon_ =
      std::make_unique<GlicStatusIcon>(controller_.get(), status_tray_);
}

void GlicBackgroundModeManager::ExitBackgroundMode() {
  status_icon_.reset();

  keep_alive_.reset();
}

void GlicBackgroundModeManager::EnableLaunchOnStartup(bool should_launch) {}

}  // namespace glic
