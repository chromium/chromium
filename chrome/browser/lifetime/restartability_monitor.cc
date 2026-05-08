// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/restartability_monitor.h"

#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace smart_restart {

namespace {

// Returns true if the tab is in a state where it could display a "Leave site?"
// prompt (i.e. it has both a beforeunload handler and sticky user activation).
bool CouldTabDisplayBeforeUnloadDialog(content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }

  // Check if any frame in the primary page could display a beforeunload dialog.
  // This requires both a registered beforeunload handler and sticky user
  // activation for that specific frame.
  bool could_display_beforeunload = false;
  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHostWithAction(
      [&could_display_beforeunload](content::RenderFrameHost* rfh) {
        if (rfh->CouldDisplayBeforeUnloadDialog()) {
          could_display_beforeunload = true;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });

  return could_display_beforeunload;
}

}  // namespace

uint32_t RestartabilityState::GetRestartabilityStateFactor() const {
  uint32_t mask = SmartRestartStateFactor::kNone;

  if (total_browser_count_is_zero) {
    mask |= SmartRestartStateFactor::kTotalBrowserCountZero;
  }

  if (has_incognito) {
    mask |= SmartRestartStateFactor::kIncognito;
  }

  if (has_dirty_tabs) {
    mask |= SmartRestartStateFactor::kBeforeUnloadHandler;
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

bool RestartabilityState::HasAnyActiveBlockers() const {
  // A blocker is any bit EXCEPT the 'TotalBrowserCountZero' bit.
  uint32_t active_blockers = GetRestartabilityStateFactor() &
                             ~SmartRestartStateFactor::kTotalBrowserCountZero;
  return active_blockers != SmartRestartStateFactor::kNone;
}

// static
RestartabilityState RestartabilityMonitor::ComputeCurrentState() {
  RestartabilityState state;

  if (GlobalBrowserCollection::GetInstance()->IsEmpty()) {
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
          if (CouldTabDisplayBeforeUnloadDialog(
                  tab_strip_model->GetWebContentsAt(i))) {
            state.has_dirty_tabs = true;
            break;
          }
        }
        return true;
      });

  return state;
}

}  // namespace smart_restart
