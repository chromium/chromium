// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/lacros_launcher.h"

#include "base/syslog_logging.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"

namespace {

crosapi::BrowserManager* browser_manager() {
  return crosapi::BrowserManager::Get();
}

bool IsLacrosEnabled() {
  return crosapi::browser_util::IsLacrosEnabledInChromeKioskSession() ||
         crosapi::browser_util::IsLacrosEnabledInWebKioskSession();
}

}  // namespace

namespace app_mode {

LacrosLauncher::LacrosLauncher() = default;
LacrosLauncher::~LacrosLauncher() = default;

void LacrosLauncher::Start(base::OnceClosure callback) {
  if (!IsLacrosEnabled()) {
    std::move(callback).Run();
    return;
  }

  SYSLOG(INFO) << "Launching Lacros for kiosk session";
  if (browser_manager()->IsRunning()) {
    // Nothing to do if lacros is already running.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  SYSLOG(INFO) << "Waiting for Lacros to start";
  on_launched_ = std::move(callback);
  browser_manager_observation_.Observe(browser_manager());

  browser_manager()->EnsureLaunch();
}

void LacrosLauncher::OnStateChanged() {
  if (browser_manager()->IsRunning()) {
    SYSLOG(INFO) << "Lacros started";
    browser_manager_observation_.Reset();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_launched_));
  }
}

}  // namespace app_mode
