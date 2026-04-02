// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/restartability_monitor.h"

#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

namespace smart_restart {

uint32_t RestartabilityState::GetRestartabilityStateFactor() const {
  uint32_t mask = SmartRestartStateFactor::kNone;

  if (total_browser_count_is_zero) {
    mask |= SmartRestartStateFactor::kTotalBrowserCountZero;
  }

  if (has_incognito) {
    mask |= SmartRestartStateFactor::kIncognito;
  }

  if (has_dirty_tabs) {
    mask |= SmartRestartStateFactor::kUnloadHandler;
  }

  if (download_count > 0) {
    mask |= SmartRestartStateFactor::kDownload;
  }

  if (is_audio_playing) {
    mask |= SmartRestartStateFactor::kMedia;
  }

  if (has_app_windows) {
    mask |= SmartRestartStateFactor::kAppWindow;
  }

  return mask;
}

// static
RestartabilityState RestartabilityMonitor::ComputeCurrentState() {
  RestartabilityState state;

  if (chrome::GetTotalBrowserCount() == 0) {
    state.total_browser_count_is_zero = true;
  }

  state.download_count =
      DownloadCoreService::BlockingShutdownCountAllProfiles();

  if (metrics::DesktopSessionDurationTracker::IsInitialized()) {
    state.is_audio_playing =
        metrics::DesktopSessionDurationTracker::Get()->is_audio_playing();
  }

  GlobalBrowserCollection::GetInstance()->ForEach(
      [&state](BrowserWindowInterface* browser_interface) {
        if (browser_interface->GetProfile()->IsOffTheRecord()) {
          state.has_incognito = true;
        }

        auto type = browser_interface->GetType();
        if (type == BrowserWindowInterface::Type::TYPE_APP ||
            type == BrowserWindowInterface::Type::TYPE_APP_POPUP) {
          state.has_app_windows = true;
        }

        TabStripModel* tab_strip_model = browser_interface->GetTabStripModel();
        for (int i = 0; i < tab_strip_model->count(); ++i) {
          content::WebContents* web_contents =
              tab_strip_model->GetWebContentsAt(i);
          if (web_contents &&
              web_contents->NeedToFireBeforeUnloadOrUnloadEvents()) {
            state.has_dirty_tabs = true;
            break;
          }
        }
        return true;
      });

  return state;
}

}  // namespace smart_restart
