// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/skill_recording_service.h"

#include <utility>

#include "base/check_deref.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/context_hub/record_replay/tab_active_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"

namespace context_hub {
namespace {

BrowserWindowInterface* GetLastActiveBrowser(Profile& profile) {
  ProfileBrowserCollection* collection =
      ProfileBrowserCollection::GetForProfile(&profile);
  return collection ? collection->GetLastActiveBrowser() : nullptr;
}

}  // namespace

SkillRecordingService::SkillRecordingService(Profile* profile)
    : profile_(CHECK_DEREF(profile)) {}

SkillRecordingService::~SkillRecordingService() {
  StopRecording();
}

void SkillRecordingService::Shutdown() {
  StopRecording();
}

void SkillRecordingService::StartRecording() {
  if (is_recording()) {
    return;
  }
  actions_.clear();
  start_time_ = base::TimeTicks::Now();

  if (ProfileBrowserCollection* collection =
          ProfileBrowserCollection::GetForProfile(&profile_.get())) {
    browser_collection_observation_.Observe(collection);
  }

  browser_tab_strip_tracker_ =
      std::make_unique<BrowserTabStripTracker>(this, this);
  browser_tab_strip_tracker_->Init();

  ObserveActiveTabInBrowser(GetLastActiveBrowser(*profile_));
}

browser::context_hub::mojom::SkillRecordingSessionDataPtr
SkillRecordingService::StopRecording() {
  if (!is_recording()) {
    return browser::context_hub::mojom::SkillRecordingSessionData::New();
  }
  start_time_.reset();
  browser_collection_observation_.Reset();
  tab_observer_.reset();
  browser_tab_strip_tracker_.reset();

  auto session_data =
      browser::context_hub::mojom::SkillRecordingSessionData::New();
  session_data->actions = std::exchange(actions_, {});
  return session_data;
}

void SkillRecordingService::RecordClick(int x, int y) {
  if (!is_recording()) {
    return;
  }
  auto action = browser::context_hub::mojom::SkillRecordingAction::New();
  action->description = base::StringPrintf("Mouse Clicked at X:%d, Y:%d", x, y);
  action->timestamp = base::TimeTicks::Now() - *start_time_;
  actions_.push_back(std::move(action));
}

bool SkillRecordingService::ShouldTrackBrowser(
    BrowserWindowInterface* browser) {
  return browser && browser->GetProfile() == &profile_.get();
}

void SkillRecordingService::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  if (!is_recording()) {
    return;
  }
  ObserveActiveTabInBrowser(browser);
}

void SkillRecordingService::OnBrowserClosed(BrowserWindowInterface* browser) {
  if (!is_recording()) {
    return;
  }
  BrowserWindowInterface* last_active = GetLastActiveBrowser(*profile_);
  ObserveActiveTabInBrowser(last_active != browser ? last_active : nullptr);
}

void SkillRecordingService::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!is_recording()) {
    return;
  }

  if (change.type() == TabStripModelChange::kReplaced && tab_observer_ &&
      change.GetReplace()->old_contents == tab_observer_->web_contents()) {
    ObserveWebContents(change.GetReplace()->new_contents);
    return;
  }

  if (change.type() == TabStripModelChange::kRemoved && tab_observer_ &&
      tab_observer_->web_contents()) {
    for (const TabStripModelChange::RemovedTab& contents :
         change.GetRemove()->contents) {
      if (contents.contents == tab_observer_->web_contents()) {
        tab_observer_.reset();
        break;
      }
    }
  }

  if (!selection.active_tab_changed()) {
    return;
  }

  BrowserWindowInterface* active_browser = GetLastActiveBrowser(*profile_);
  if (!active_browser ||
      active_browser->GetTabStripModel() != tab_strip_model) {
    return;
  }

  ObserveWebContents(selection.new_contents);
}

void SkillRecordingService::ObserveActiveTabInBrowser(
    BrowserWindowInterface* browser) {
  if (!browser || !browser->GetTabStripModel()) {
    tab_observer_.reset();
    return;
  }
  ObserveWebContents(browser->GetTabStripModel()->GetActiveWebContents());
}

void SkillRecordingService::ObserveWebContents(
    content::WebContents* web_contents) {
  if (tab_observer_ && tab_observer_->web_contents() == web_contents) {
    return;
  }
  tab_observer_.reset();
  if (web_contents) {
    tab_observer_ = std::make_unique<TabActiveObserver>(this, web_contents);
  }
}

}  // namespace context_hub
