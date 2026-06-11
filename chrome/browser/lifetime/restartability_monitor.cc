// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/restartability_monitor.h"

#include <algorithm>
#include <type_traits>
#include <utility>

#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace smart_restart {

namespace {

using Blocker = ExtendedRestartabilityState::SmartRestartBlocker;

using Level = ExtendedRestartabilityState::SmartRestartDisruptionLevel;

// Maps a specific blocker reason to its disruption level.
Level GetDisruptionLevel(Blocker blocker) {
  switch (blocker) {
    // --- Low Disruption ---
    case Blocker::kVisible:
    case Blocker::kActiveTab:
    case Blocker::kPinnedTab:
    case Blocker::kPdf:
    case Blocker::kNotWebOrInternal:
    case Blocker::kInvalidURL:
    case Blocker::kNoMainFrame:
    case Blocker::kNotificationsEnabled:
    case Blocker::kFormInteractions:
      return Level::kLowDisruption;

    // --- Medium Disruption ---
    case Blocker::kBackgroundActivity:
    case Blocker::kExtensionProtected:
      return Level::kMediumDisruption;

    // --- High Disruption ---
    case Blocker::kIncognito:
    case Blocker::kDownload:
    case Blocker::kMedia:
    case Blocker::kAppWindow:
    case Blocker::kAudible:
    case Blocker::kPictureInPicture:
    case Blocker::kCapturingVideo:
    case Blocker::kCapturingAudio:
    case Blocker::kBeingMirrored:
    case Blocker::kCapturingWindow:
    case Blocker::kCapturingDisplay:
    case Blocker::kConnectedToBluetooth:
    case Blocker::kConnectedToUSB:
    case Blocker::kDevToolsOpen:
    case Blocker::kGlicShared:
    case Blocker::kUserEdits:
    case Blocker::kWebApp:
    case Blocker::kVisiblePausedMedia:
    case Blocker::kLensShared:
      return Level::kHighDisruption;

    case Blocker::kBeforeUnloadHandler:
      return features::kSmartRestartLockBypassBeforeUnloadThreshold.Get() >= 0.0
                 ? Level::kMediumDisruption
                 : Level::kHighDisruption;
  }
}

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

// Logic specifically for Performance Manager signals.
void AddPageLiveStateBlockers(const performance_manager::PageNode* page_node,
                              ExtendedRestartabilityState& state) {
  using PLS = performance_manager::PageLiveStateDecorator;
  const auto* data = PLS::Data::FromPageNode(page_node);
  if (!data) {
    return;
  }

  if (data->IsCapturingVideo()) {
    state.AddBlocker(Blocker::kCapturingVideo);
  }
  if (data->IsCapturingAudio()) {
    state.AddBlocker(Blocker::kCapturingAudio);
  }
  if (data->IsBeingMirrored()) {
    state.AddBlocker(Blocker::kBeingMirrored);
  }
  if (data->IsCapturingWindow()) {
    state.AddBlocker(Blocker::kCapturingWindow);
  }
  if (data->IsCapturingDisplay()) {
    state.AddBlocker(Blocker::kCapturingDisplay);
  }
  if (data->IsConnectedToUSBDevice()) {
    state.AddBlocker(Blocker::kConnectedToUSB);
  }
  if (data->IsConnectedToBluetoothDevice()) {
    state.AddBlocker(Blocker::kConnectedToBluetooth);
  }
  if (data->IsDevToolsOpen()) {
    state.AddBlocker(Blocker::kDevToolsOpen);
  }
  if (data->IsActiveTab()) {
    state.AddBlocker(Blocker::kActiveTab);
  }
  if (data->IsPinnedTab()) {
    state.AddBlocker(Blocker::kPinnedTab);
  }
  if (!data->IsAutoDiscardable()) {
    state.AddBlocker(Blocker::kExtensionProtected);
  }
  if (data->UpdatedTitleOrFaviconInBackground()) {
    state.AddBlocker(Blocker::kBackgroundActivity);
  }
}

// Logic for Performance Manager PageNode signals.
void AddPageNodeBlockers(const performance_manager::PageNode* page_node,
                         ExtendedRestartabilityState& state) {
  if (page_node->HadFormInteraction()) {
    state.AddBlocker(Blocker::kFormInteractions);
  }
  if (page_node->HadUserEdits()) {
    state.AddBlocker(Blocker::kUserEdits);
  }
  if (page_node->GetNotificationPermissionStatus() ==
      blink::mojom::PermissionStatus::GRANTED) {
    state.AddBlocker(Blocker::kNotificationsEnabled);
  }
}

// Logic for WebContents, TabStrip and associated UI helpers.
void AddTabBlockers(content::WebContents* contents,
                    ExtendedRestartabilityState& state) {
  if (!contents->GetPrimaryMainFrame()) {
    state.AddBlocker(Blocker::kNoMainFrame);
  }

  // Visibility signals.
  if (contents->GetVisibility() == content::Visibility::VISIBLE) {
    state.AddBlocker(Blocker::kVisible);

    // Check for paused media sessions on visible tabs.
    if (auto* media_session = content::MediaSession::GetIfExists(contents)) {
      auto info = media_session->GetMediaSessionInfoSync();
      if (info && info->state == media_session::mojom::MediaSessionInfo::
                                     SessionState::kSuspended) {
        state.AddBlocker(Blocker::kVisiblePausedMedia);
      }
    }
  }
  if (contents->IsCurrentlyAudible()) {
    state.AddBlocker(Blocker::kAudible);
  }

  // Feature signals.
  if (contents->HasPictureInPictureVideo() ||
      contents->HasPictureInPictureDocument()) {
    state.AddBlocker(Blocker::kPictureInPicture);
  }

  const GURL& url = contents->GetLastCommittedURL();
  if (!url.is_valid() || url.is_empty()) {
    state.AddBlocker(Blocker::kInvalidURL);
  }

  bool is_web_page_or_internal_or_data_page = url.SchemeIsHTTPOrHTTPS() ||
                                              url.SchemeIs("chrome") ||
                                              url.SchemeIs(url::kDataScheme);
  if (!is_web_page_or_internal_or_data_page) {
    state.AddBlocker(Blocker::kNotWebOrInternal);
  }

  if (contents->GetContentsMimeType() == "application/pdf") {
    state.AddBlocker(Blocker::kPdf);
  }

  web_app::WebAppTabHelper* web_app_helper =
      web_app::WebAppTabHelper::FromWebContents(contents);
  if (web_app_helper && web_app_helper->is_in_app_window()) {
    state.AddBlocker(Blocker::kWebApp);
  }

  auto* glic_service = glic::GlicKeyedServiceFactory::GetGlicKeyedService(
      contents->GetBrowserContext());
  auto* tab_interface = tabs::TabInterface::MaybeGetFromContents(contents);
  if (glic_service && tab_interface &&
      glic_service->instance_coordinator().IsTabPinnedToAnyInstance(
          tab_interface->GetHandle())) {
    state.AddBlocker(Blocker::kGlicShared);
  }

  auto* lens_controller = LensOverlayController::FromTabWebContents(contents);
  if (lens_controller && lens_controller->IsOverlayActive()) {
    state.AddBlocker(Blocker::kLensShared);
  }
}

}  // namespace

ExtendedRestartabilityState::ExtendedRestartabilityState() = default;
ExtendedRestartabilityState::ExtendedRestartabilityState(
    const ExtendedRestartabilityState&) = default;
ExtendedRestartabilityState::ExtendedRestartabilityState(
    ExtendedRestartabilityState&&) = default;
ExtendedRestartabilityState& ExtendedRestartabilityState::operator=(
    const ExtendedRestartabilityState&) = default;
ExtendedRestartabilityState& ExtendedRestartabilityState::operator=(
    ExtendedRestartabilityState&&) = default;
ExtendedRestartabilityState::~ExtendedRestartabilityState() = default;

void ExtendedRestartabilityState::AddBlocker(SmartRestartBlocker blocker) {
  blockers.Put(blocker);
  SmartRestartDisruptionLevel level = GetDisruptionLevel(blocker);
  if (std::to_underlying(max_disruption_level) < std::to_underlying(level)) {
    max_disruption_level = level;
  }
}

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

// static
ExtendedRestartabilityState
RestartabilityMonitor::ComputeExtendedRestartabilityState() {
  ExtendedRestartabilityState state;

  // 1. Gather global data.
  if (GlobalBrowserCollection::GetInstance()->IsEmpty()) {
    state.baseline.total_browser_count_is_zero = true;
  }
  state.baseline.download_count =
      DownloadCoreService::BlockingShutdownCountAllProfiles();
  if (state.baseline.download_count > 0) {
    state.AddBlocker(Blocker::kDownload);
  }

  if (metrics::DesktopSessionDurationTracker::IsInitialized()) {
    state.baseline.is_audio_playing =
        metrics::DesktopSessionDurationTracker::Get()->is_audio_playing();
    if (state.baseline.is_audio_playing) {
      state.AddBlocker(Blocker::kMedia);
    }
  }

  // 2. Single traversal for windows and tabs.
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&state](BrowserWindowInterface* browser_interface) {
        if (!state.baseline.has_incognito &&
            browser_interface->GetProfile()->IsOffTheRecord()) {
          state.baseline.has_incognito = true;
          state.AddBlocker(Blocker::kIncognito);
        }

        auto type = browser_interface->GetType();
        if (!state.baseline.has_app_windows &&
            (type == BrowserWindowInterface::Type::TYPE_APP ||
             type == BrowserWindowInterface::Type::TYPE_APP_POPUP)) {
          state.baseline.has_app_windows = true;
          state.AddBlocker(Blocker::kAppWindow);
        }

        TabStripModel* tab_strip_model = browser_interface->GetTabStripModel();
        state.total_tab_count += tab_strip_model->count();

        for (int i = 0; i < tab_strip_model->count(); ++i) {
          content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
          if (!contents) {
            continue;
          }

          if (CouldTabDisplayBeforeUnloadDialog(contents)) {
            state.beforeunload_tab_count++;
            if (!state.baseline.has_dirty_tabs) {
              state.baseline.has_dirty_tabs = true;
              state.AddBlocker(Blocker::kBeforeUnloadHandler);
            }
            Profile* profile = browser_interface->GetProfile();
            if (profile) {
              double score =
                  site_engagement::SiteEngagementService::Get(profile)
                      ->GetScore(contents->GetLastCommittedURL());
              state.beforeunload_scores.push_back(score);
            }
          }

          auto page_node = performance_manager::PerformanceManager::
              GetPrimaryPageNodeForWebContents(contents);
          if (page_node) {
            AddPageLiveStateBlockers(page_node.get(), state);
            AddPageNodeBlockers(page_node.get(), state);
          }
          AddTabBlockers(contents, state);
        }
        return true;
      });

  return state;
}

}  // namespace smart_restart
