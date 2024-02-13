// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/background_mode_optimizer.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/keep_alive_registry/keep_alive_registry.h"

BackgroundModeOptimizer::~BackgroundModeOptimizer() {
  KeepAliveRegistry::GetInstance()->RemoveObserver(this);
  BrowserList::RemoveObserver(this);
}

// static
std::unique_ptr<BackgroundModeOptimizer> BackgroundModeOptimizer::Create() {
  // If the --keep-alive-for-test flag is passed, then always keep the browser
  // running in the background until the user explicitly terminates it.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kKeepAliveForTest)) {
    return nullptr;
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(features::kBackgroundModeAllowRestart)) {
    // Private ctor.
    return base::WrapUnique(new BackgroundModeOptimizer());
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
//  KeepAliveRegistry implementation

void BackgroundModeOptimizer::OnKeepAliveStateChanged(bool is_keeping_alive) {
  // Nothing to do
}

void BackgroundModeOptimizer::OnKeepAliveRestartStateChanged(bool can_restart) {
  if (can_restart) {
    TryBrowserRestart();
  }
}
///////////////////////////////////////////////////////////////////////////////
//  BrowserListObserver implementation

void BackgroundModeOptimizer::OnBrowserAdded(Browser* browser) {
  browser_was_added_ = true;
}

///////////////////////////////////////////////////////////////////////////////
//  private methods

BackgroundModeOptimizer::BackgroundModeOptimizer() {
  KeepAliveRegistry::GetInstance()->AddObserver(this);
  BrowserList::AddObserver(this);
}

void BackgroundModeOptimizer::TryBrowserRestart() {
  // Avoid unecessary restarts. Whether a browser window has been shown is
  // our current heuristic to determine if it's worth it.
  if (!browser_was_added_) {
    DVLOG(1) << "TryBrowserRestart: Cancelled because no browser was added "
             << "since the last restart";
    return;
  }

  // If the application is already shutting down, do not turn it into a restart.
  if (browser_shutdown::IsTryingToQuit() ||
      browser_shutdown::HasShutdownStarted()) {
    DVLOG(1) << "TryBrowserRestart: Cancelled because we are shutting down.";
    return;
  }

  DVLOG(1) << "TryBrowserRestart: Restarting.";

  DoRestart();
}

void BackgroundModeOptimizer::DoRestart() {
  BackgroundModeManager::set_should_restart_in_background(true);
  chrome::AttemptRestart();
}
