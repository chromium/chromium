// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/launcher/glic_background_mode_manager.h"

#include <memory>

#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"

GlicBackgroundModeManager::GlicBackgroundModeManager() {
  configuration_ = std::make_unique<GlicConfiguration>(this);
  OnEnabledChanged(configuration_->IsEnabled());
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
}

void GlicBackgroundModeManager::ExitBackgroundMode() {
  keep_alive_.reset();
}

void GlicBackgroundModeManager::EnableLaunchOnStartup(bool should_launch) {}
