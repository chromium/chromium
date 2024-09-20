// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usage_scenario/tab_usage_scenario_tracker.h"

#include "base/containers/contains.h"
#include "base/not_fatal_until.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/script_injection_tracker.h"
#include "extensions/common/extension_id.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/display/screen.h"
#include "url/origin.h"

namespace metrics {

namespace {

std::pair<ukm::SourceId, url::Origin> GetNavigationInfoForContents(
    content::WebContents* contents) {
  auto* main_frame = contents->GetPrimaryMainFrame();
  if (!main_frame || main_frame->GetLastCommittedURL().is_empty())
    return std::make_pair(ukm::kInvalidSourceId, url::Origin());

  return std::make_pair(main_frame->GetPageUkmSourceId(),
                        main_frame->GetLastCommittedOrigin());
}

extensions::ExtensionIdSet GetExtensionsThatRanContentScriptsInWebContents(
    content::WebContents* contents) {
  content::RenderFrameHost* main_frame = contents->GetPrimaryMainFrame();
  if (!main_frame) {
    // WebContents is being destroyed.
    return {};
  }
  // Find the complete set of processes in the WebContents' frame tree first so
  // that each is only handled once.
  std::set<content::RenderProcessHost*> processes;
  processes.insert(main_frame->GetProcess());
  main_frame->ForEachRenderFrameHost(
      [&processes](content::RenderFrameHost* frame) {
        processes.insert(frame->GetProcess());
      });

  // Find the set of extensions that ran scripts in these processes.
  //
  // The result may include extra extensions for the following reasons:
  //
  // * ScriptInjectionTracker returns all extensions that have injected scripts
  //   into a process. Multiple pages on the same domain might share a process
  //   so we can't be sure which pages the extension affected.
  // * It also returns extensions that have EVER injected scripts into the
  //   process, even if the extension is no longer active (eg. after navigating
  //   to a new page or removing the extension).
  // * There are renderer-side permission checks that might cause a page to
  //   refuse to inject a script, which ScriptInjectionTracker isn't aware of so
  //   assumes the script was injected.
  //
  // In the first two points, the extension COULD still be affecting pages
  // loaded in the process (due to bugs or malicious code, or just because
  // changes the extension made to the page content had lingering effects).
  //
  // It may also miss extensions for the following reasons:
  //
  // * ScriptInjectionTracker only includes extensions that inject Javascript.
  //   It doesn't track injected CSS scripts, declarative web requests, or
  //   anything else an extension might do that could affect the page content.
  // * The set of processes for the page includes only those that hosted frames,
  //   not workers.
  extensions::ExtensionIdSet extensions;
  for (const auto* process : processes) {
    extensions.merge(extensions::ScriptInjectionTracker::
                         GetExtensionsThatRanContentScriptsInProcess(*process));
  }
  return extensions;
}

}  // namespace

TabUsageScenarioTracker::TabUsageScenarioTracker(
    UsageScenarioDataStoreImpl* usage_scenario_data_store)
    : usage_scenario_data_store_(usage_scenario_data_store) {
  // TODO(crbug.com/40158987): Owners of this class have to set the initial
  // state. Constructing the object like this starts off the state as empty. If
  // tabs/windows already exist when this object is created they need to be
  // added using the normal functions after creation.
}

TabUsageScenarioTracker::~TabUsageScenarioTracker() {
  // Make sure that this doesn't get destroyed after destroying the global
  // screen instance.
  DCHECK(display::Screen::GetScreen());
}

void TabUsageScenarioTracker::OnTabAdded(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  usage_scenario_data_store_->OnTabAdded();

  // Tab is added already visible. It will not get a separate visibility update
  // so we handle the visibility here.
  if (web_contents->GetVisibility() == content::Visibility::VISIBLE) {
    DCHECK(!base::Contains(visible_tabs_, web_contents));
    usage_scenario_data_store_->OnWindowVisible();
    InsertContentsInMapOfVisibleTabs(web_contents);
  }
}

void TabUsageScenarioTracker::OnTabRemoved(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnWebContentsRemoved(web_contents);
  usage_scenario_data_store_->OnTabClosed();
}

void TabUsageScenarioTracker::OnTabReplaced(
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnWebContentsRemoved(old_contents);
  DCHECK(!base::Contains(visible_tabs_, old_contents));
  DCHECK(!base::Contains(contents_playing_video_, old_contents));
  DCHECK(!base::Contains(contents_playing_video_fullscreen_, old_contents));

  // Start tracking |new_contents| if needed.
  if (new_contents->GetVisibility() == content::Visibility::VISIBLE)
    OnTabVisibilityChanged(new_contents);
  if (new_contents->IsCurrentlyAudible())
    usage_scenario_data_store_->OnAudioStarts();
}

void TabUsageScenarioTracker::OnTabVisibilityChanged(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto iter = visible_tabs_.find(web_contents);
  const bool was_visible = iter != visible_tabs_.end();
  const bool is_visible =
      web_contents->GetVisibility() == content::Visibility::VISIBLE;

  // The first content::Visibility::VISIBLE notification is always sent, even
  // if the tab starts in the visible state.
  if (!was_visible && is_visible) {
    usage_scenario_data_store_->OnWindowVisible();

    // If this tab is playing video then record that it became visible.
    if (base::Contains(contents_playing_video_, web_contents)) {
      usage_scenario_data_store_->OnVideoStartsInVisibleTab();
    }

    InsertContentsInMapOfVisibleTabs(web_contents);
  } else if (was_visible && !is_visible) {
    // The tab was previously visible and it's now hidden or occluded.
    OnTabBecameHidden(&iter);
  }
}

void TabUsageScenarioTracker::OnTabDiscarded(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Record that the ukm::SourceID associated with this tab isn't visible
  // anymore, if necessary.
  auto iter = visible_tabs_.find(web_contents);
  CHECK_EQ(iter != visible_tabs_.end(),
           web_contents->GetVisibility() == content::Visibility::VISIBLE);
  if (iter != visible_tabs_.end() &&
      iter->second.first != ukm::kInvalidSourceId) {
    usage_scenario_data_store_->OnUkmSourceBecameHidden(iter->second.first,
                                                        iter->second.second);
    // Discard may destroy the associated WebContents or replace the
    // WebContent's primary document with an empty docoument. For the latter
    // case the source id must be invalidated as the UKM source should no longer
    // be considered valid.
    iter->second.first = ukm::kInvalidSourceId;
  }
}

void TabUsageScenarioTracker::OnTabInteraction(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  usage_scenario_data_store_->OnUserInteraction();
}

void TabUsageScenarioTracker::OnTabIsAudibleChanged(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (web_contents->IsCurrentlyAudible()) {
    usage_scenario_data_store_->OnAudioStarts();
  } else {
    usage_scenario_data_store_->OnAudioStops();
  }
}

void TabUsageScenarioTracker::OnMediaEffectivelyFullscreenChanged(
    content::WebContents* web_contents,
    bool is_fullscreen) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!last_num_displays_.has_value()) {
    last_num_displays_ = GetNumDisplays();
  }

  // Use `last_num_displays_` instead of `GetNumDisplays()` below, to let
  // `OnNumDisplaysChanged()` handle changes in the number of displays.
  if (is_fullscreen) {
    auto [it, inserted] =
        contents_playing_video_fullscreen_.insert(web_contents);
    if (inserted && contents_playing_video_fullscreen_.size() == 1U &&
        last_num_displays_.value() == 1) {
      usage_scenario_data_store_->OnFullScreenVideoStartsOnSingleMonitor();
    }
  } else {
    auto num_removed = contents_playing_video_fullscreen_.erase(web_contents);
    if (num_removed == 1U && contents_playing_video_fullscreen_.empty() &&
        last_num_displays_.value() == 1) {
      usage_scenario_data_store_->OnFullScreenVideoEndsOnSingleMonitor();
    }
  }
}

void TabUsageScenarioTracker::OnMediaDestroyed(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Return early if the tab is being destroyed. It will be removed from
  // `contents_playing_video_fullscreen_` by
  // TabUsageScenarioTracker::OnWebContentsRemoved().
  //
  // This is an unfortunate workaround for a crash (crbug.com/1393544) that
  // occurs when a Browser that still contains WebContents is destroyed,
  // resulting in:
  // 1. The Browser destroys its `exclusive_access_manager_`.
  // 2. The Browser destroys its WebContents.
  // 3. The WebContents destroys its frames.
  // 4. The frame deletion calls TabUsageScenarioTracker::OnMediaDestroyed
  //    (this method).
  // 5. This method calls HasActiveEffectivelyFullscreenVideo(), which ends
  //    up calling Browser::IsFullscreenForTabOrPending().
  // 6. Browser::IsFullscreenForTabOrPending() accesses a deleted
  //    `exclusive_access_manager_`.
  // According to a DCHECK in ~Browser
  // (https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/browser.cc;l=575;drc=60e7a86ffafb2aafa40cb00214d6b813b41c6f75),
  // a Browser's WebContents should be deleted before the Browser itself is
  // deleted, but it looks like it's not always the case.
  if (web_contents->IsBeingDestroyed())
    return;

  // Destroying a media may cause the WebContents to no longer have a fullscreen
  // media.
  OnMediaEffectivelyFullscreenChanged(
      web_contents, web_contents->HasActiveEffectivelyFullscreenVideo());
}

void TabUsageScenarioTracker::OnPrimaryMainFrameNavigationCommitted(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  usage_scenario_data_store_->OnTopLevelNavigation();

  if (web_contents->GetVisibility() == content::Visibility::VISIBLE) {
    auto iter = visible_tabs_.find(web_contents);
    CHECK(iter != visible_tabs_.end(), base::NotFatalUntil::M130);

    // If there's already an entry with a valid SourceID for this in
    // |visible_tabs_| then it means that there's been a main frame navigation
    // for a visible tab. Records that the SourceID previously associated with
    // this tab isn't visible anymore.
    if (iter->second.first != ukm::kInvalidSourceId) {
      usage_scenario_data_store_->OnUkmSourceBecameHidden(iter->second.first,
                                                          iter->second.second);
    }

    iter->second = GetNavigationInfoForContents(web_contents);
    if (iter->second.first != ukm::kInvalidSourceId) {
      usage_scenario_data_store_->OnUkmSourceBecameVisible(
          iter->second.first, iter->second.second,
          GetExtensionsThatRanContentScriptsInWebContents(web_contents));
    }
  }
}

void TabUsageScenarioTracker::OnVideoStartedPlaying(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(contents_playing_video_, web_contents));
  contents_playing_video_.insert(web_contents);
  if (base::Contains(visible_tabs_, web_contents))
    usage_scenario_data_store_->OnVideoStartsInVisibleTab();
}

void TabUsageScenarioTracker::OnVideoStoppedPlaying(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(contents_playing_video_, web_contents));
  contents_playing_video_.erase(web_contents);
  if (base::Contains(visible_tabs_, web_contents))
    usage_scenario_data_store_->OnVideoStopsInVisibleTab();
}

void TabUsageScenarioTracker::OnDisplayAdded(const display::Display&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnNumDisplaysChanged();
}

void TabUsageScenarioTracker::OnDisplaysRemoved(const display::Displays&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnNumDisplaysChanged();
}

int TabUsageScenarioTracker::GetNumDisplays() {
  auto* screen = display::Screen::GetScreen();
  DCHECK(screen);
  return screen->GetNumDisplays();
}

void TabUsageScenarioTracker::OnTabBecameHidden(
    VisibleTabsMap::iterator* visible_tab_iter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If this tab is playing video then record that it became non visible.
  content::WebContents* const web_contents = (*visible_tab_iter)->first;
  if (base::Contains(contents_playing_video_, web_contents))
    usage_scenario_data_store_->OnVideoStopsInVisibleTab();

  // Record that the ukm::SourceID associated with this tab isn't visible
  // anymore if necessary.
  if ((*visible_tab_iter)->second.first != ukm::kInvalidSourceId) {
    usage_scenario_data_store_->OnUkmSourceBecameHidden(
        (*visible_tab_iter)->second.first, (*visible_tab_iter)->second.second);
  }

  visible_tabs_.erase(*visible_tab_iter);
  usage_scenario_data_store_->OnWindowHidden();
}

void TabUsageScenarioTracker::OnWebContentsRemoved(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iter = visible_tabs_.find(web_contents);
  DCHECK_EQ(iter != visible_tabs_.end(),
            web_contents->GetVisibility() == content::Visibility::VISIBLE);
  // If |web_contents| is tracked in the list of visible WebContents then a
  // synthetic visibility change event should be emitted.
  if (iter != visible_tabs_.end())
    OnTabBecameHidden(&iter);

  // Remove |web_contents| from the list of contents playing video. If
  // necessary, the data store was already informed that a video stopped playing
  // in a visible tab in the OnTabBecameHidden() call above.
  contents_playing_video_.erase(web_contents);

  {
    // Remove |web_contents| from the list of contents will fullscreen media and
    // if necessary, inform the data store that there is no more fullscreen
    // video playing on a single monitor.
    size_t num_removed = contents_playing_video_fullscreen_.erase(web_contents);
    if (num_removed == 1 && contents_playing_video_fullscreen_.empty() &&
        last_num_displays_.has_value() && last_num_displays_.value() == 1) {
      usage_scenario_data_store_->OnFullScreenVideoEndsOnSingleMonitor();
    }
  }

  // If necessary, inform the data store that audio stopped.
  if (web_contents->IsCurrentlyAudible())
    usage_scenario_data_store_->OnAudioStops();
}

void TabUsageScenarioTracker::InsertContentsInMapOfVisibleTabs(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(visible_tabs_, web_contents));
  auto iter = visible_tabs_.emplace(web_contents,
                                    GetNavigationInfoForContents(web_contents));
  if (iter.first->second.first != ukm::kInvalidSourceId) {
    usage_scenario_data_store_->OnUkmSourceBecameVisible(
        iter.first->second.first, iter.first->second.second,
        GetExtensionsThatRanContentScriptsInWebContents(web_contents));
  }
}

void TabUsageScenarioTracker::OnNumDisplaysChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Multiple displays can be added or removed before OnDisplayAdded and
  // OnDidRemoveDisplays are dispatched. It is therefore incorrect to assume
  // that the number of displays has increased / decreased compared to
  // `last_num_displays_` following a call to OnDisplayAdded/ OnDisplaysRemoved.

  const int num_displays = GetNumDisplays();

  if (!contents_playing_video_fullscreen_.empty()) {
    // `last_num_displays_` is set when `contents_playing_video_fullscreen_`
    // becomes non-empty.
    CHECK(last_num_displays_.has_value());

    if (num_displays == 1 && last_num_displays_ != 1) {
      usage_scenario_data_store_->OnFullScreenVideoStartsOnSingleMonitor();
    } else if (num_displays != 1 && last_num_displays_ == 1) {
      usage_scenario_data_store_->OnFullScreenVideoEndsOnSingleMonitor();
    }
  }

  last_num_displays_ = num_displays;
}

}  // namespace metrics
