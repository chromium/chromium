// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usage_scenario/tab_usage_scenario_tracker.h"

#include "base/containers/contains.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/display/screen.h"
#include "url/origin.h"

namespace metrics {

namespace {

std::pair<ukm::SourceId, url::Origin> GetNavigationInfoForContents(
    content::WebContents* contents) {
  auto* main_frame = contents->GetMainFrame();
  if (!main_frame || main_frame->GetLastCommittedURL().is_empty())
    return std::make_pair(ukm::kInvalidSourceId, url::Origin());

  return std::make_pair(ukm::GetSourceIdForWebContentsDocument(contents),
                        main_frame->GetLastCommittedOrigin());
}

}  // namespace

TabUsageScenarioTracker::TabUsageScenarioTracker(
    UsageScenarioDataStoreImpl* usage_scenario_data_store)
    : usage_scenario_data_store_(usage_scenario_data_store) {
  // TODO(crbug.com/1153193): Owners of this class have to set the initial
  // state. Constructing the object like this starts off the state as empty. If
  // tabs/windows already exist when this object is created they need to be
  // added using the normal functions after creation.
  auto* screen = display::Screen::GetScreen();
  // Make sure that this doesn't get created before setting up the global Screen
  // instance.
  DCHECK(screen);
  screen->AddObserver(this);
}

TabUsageScenarioTracker::~TabUsageScenarioTracker() {
  auto* screen = display::Screen::GetScreen();
  // Make sure that this doesn't get destroyed after destroying the global
  // screen instance.
  DCHECK(screen);
  screen->RemoveObserver(this);
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
  DCHECK_NE(content_with_media_playing_fullscreen_, old_contents);

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
  // The first content::Visibility::VISIBLE notification is always sent, even
  // if the tab starts in the visible state.
  if (iter == visible_tabs_.end() &&
      web_contents->GetVisibility() == content::Visibility::VISIBLE) {
    usage_scenario_data_store_->OnWindowVisible();

    // If this tab is playing video then record that it became visible.
    if (base::Contains(contents_playing_video_, web_contents)) {
      usage_scenario_data_store_->OnVideoStartsInVisibleTab();
    }

    InsertContentsInMapOfVisibleTabs(web_contents);
  } else if (iter != visible_tabs_.end() &&
             web_contents->GetVisibility() != content::Visibility::VISIBLE) {
    // The tab was previously visible and it's now hidden or occluded.
    OnTabBecameHidden(&iter);
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
  auto* screen = display::Screen::GetScreen();
  DCHECK(screen);
  if (screen->GetNumDisplays() == 1) {
    if (is_fullscreen) {
      DCHECK(!content_with_media_playing_fullscreen_);
      content_with_media_playing_fullscreen_ = web_contents;
      usage_scenario_data_store_->OnFullScreenVideoStartsOnSingleMonitor();
    } else {
      OnContentStoppedPlayingMediaFullScreen();
    }
  }
}

void TabUsageScenarioTracker::OnMainFrameNavigationCommitted(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  usage_scenario_data_store_->OnTopLevelNavigation();

  if (web_contents->GetVisibility() == content::Visibility::VISIBLE) {
    auto iter = visible_tabs_.find(web_contents);
    DCHECK(iter != visible_tabs_.end());

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
      usage_scenario_data_store_->OnUkmSourceBecameVisible(iter->second.first,
                                                           iter->second.second);
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

void TabUsageScenarioTracker::OnDisplayAdded(const display::Display& unused) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* screen = display::Screen::GetScreen();
  if (screen->GetNumDisplays() == 1) {
    if (content_with_media_playing_fullscreen_ != nullptr) {
      DCHECK(usage_scenario_data_store_
                 ->is_playing_full_screen_video_single_monitor_since()
                 .is_null());
      usage_scenario_data_store_->OnFullScreenVideoStartsOnSingleMonitor();
    }
    return;
  }
  // Stop the fullscreen video on single monitor event if there's more than one
  // screen.
  if (!usage_scenario_data_store_
           ->is_playing_full_screen_video_single_monitor_since()
           .is_null()) {
    usage_scenario_data_store_->OnFullScreenVideoEndsOnSingleMonitor();
  }
}

void TabUsageScenarioTracker::OnDisplayRemoved(const display::Display& unused) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* screen = display::Screen::GetScreen();
  DCHECK(screen);
  // Update the data store if there's only one display now running media
  // fullscreen.
  if (screen->GetNumDisplays() == 1 &&
      content_with_media_playing_fullscreen_ != nullptr) {
    DCHECK(usage_scenario_data_store_
               ->is_playing_full_screen_video_single_monitor_since()
               .is_null());
    usage_scenario_data_store_->OnFullScreenVideoStartsOnSingleMonitor();
  }
}

void TabUsageScenarioTracker::OnContentStoppedPlayingMediaFullScreen() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  usage_scenario_data_store_->OnFullScreenVideoEndsOnSingleMonitor();
  content_with_media_playing_fullscreen_ = nullptr;
}

void TabUsageScenarioTracker::OnTabBecameHidden(
    VisibleTabsMap::iterator* visible_tab_iter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If this tab is playing video then record that it became non visible.
  if (base::Contains(contents_playing_video_, (*visible_tab_iter)->first)) {
    usage_scenario_data_store_->OnVideoStopsInVisibleTab();
  }

  // |OnMediaEffectivelyFullscreenChanged| doesn't get called if a tab playing
  // media fullscreen gets closed.
  if ((*visible_tab_iter)->first == content_with_media_playing_fullscreen_)
    OnContentStoppedPlayingMediaFullScreen();

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
  auto video_iter = contents_playing_video_.find(web_contents);
  // If |web_contents| is tracked in the list of visible WebContents then a
  // synthetic visibility change event should be emitted.
  if (iter != visible_tabs_.end()) {
    OnTabBecameHidden(&iter);
  }
  if (video_iter != contents_playing_video_.end()) {
    contents_playing_video_.erase(video_iter);
  }
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
        iter.first->second.first, iter.first->second.second);
  }
}

}  // namespace metrics
