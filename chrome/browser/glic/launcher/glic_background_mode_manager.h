// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_LAUNCHER_GLIC_BACKGROUND_MODE_MANAGER_H_
#define CHROME_BROWSER_GLIC_LAUNCHER_GLIC_BACKGROUND_MODE_MANAGER_H_

#include "chrome/browser/glic/launcher/glic_configuration.h"

class ScopedKeepAlive;

class GlicBackgroundModeManager : public GlicConfiguration::Observer {
 public:
  GlicBackgroundModeManager();
  ~GlicBackgroundModeManager() override;

  // GlicConfiguration::Observer
  void OnEnabledChanged(bool enabled) override;

 private:
  void EnterBackgroundMode();
  void ExitBackgroundMode();

  void EnableLaunchOnStartup(bool should_launch);

  std::unique_ptr<GlicConfiguration> configuration_;
  bool enabled_ = false;

  std::unique_ptr<ScopedKeepAlive> keep_alive_;
};

#endif  // CHROME_BROWSER_GLIC_LAUNCHER_GLIC_BACKGROUND_MODE_MANAGER_H_
