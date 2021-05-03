// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_data_service.h"

#include "base/bind.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_data_deleter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"

SessionDataService::SessionDataService(Profile* profile) : profile_(profile) {
  DCHECK(profile_);
  DCHECK(!profile_->IsOffTheRecord());
  for (Browser* browser : *BrowserList::GetInstance())
    OnBrowserAdded(browser);

  BrowserList::AddObserver(this);
}

SessionDataService::~SessionDataService() {
  BrowserList::RemoveObserver(this);
}

void SessionDataService::OnBrowserAdded(Browser* browser) {
  if (browser->profile() != profile_)
    return;

  // A window was opened. Ensure that we run another cleanup the next time
  // all windows are closed.
  cleanup_started_ = false;
}

void SessionDataService::OnBrowserRemoved(Browser* browser) {
  if (browser->profile() != profile_)
    return;

  // Don't try anything if we're testing.  The browser_process is not fully
  // created and DeleteSession will crash if we actually attempt it.
  if (profile_->AsTestingProfile())
    return;

  // Clear session data if the last window for a profile has been closed and
  // closing the last window would normally close Chrome, unless background mode
  // is active.  Tests don't have a background_mode_manager.
  if (browser_defaults::kBrowserAliveWithNoWindows ||
      g_browser_process->background_mode_manager()->IsBackgroundModeActive()) {
    return;
  }

  // Check for any open windows for the current profile that we aren't tracking.
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile() == profile_)
      return;
  }
  StartCleanup();
}

void SessionDataService::StartCleanup() {
  if (cleanup_started_)
    return;

  cleanup_started_ = true;
  DeleteSessionOnlyData(profile_);
}
