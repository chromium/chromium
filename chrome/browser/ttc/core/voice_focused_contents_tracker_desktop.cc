// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/voice_focused_contents_tracker_desktop.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"

namespace ttc {

// static
std::unique_ptr<VoiceFocusedContentsTracker>
VoiceFocusedContentsTracker::Create(Profile& profile) {
  return std::make_unique<VoiceFocusedContentsTrackerDesktop>(profile);
}

VoiceFocusedContentsTrackerDesktop::VoiceFocusedContentsTrackerDesktop(
    Profile& profile) {
  ProfileBrowserCollection* browsers =
      ProfileBrowserCollection::GetForProfile(&profile);
  if (!browsers) {
    return;
  }
  browser_collection_observation_.Observe(browsers);
  BindToBrowser(browsers->GetLastActiveBrowser());
}

VoiceFocusedContentsTrackerDesktop::~VoiceFocusedContentsTrackerDesktop() =
    default;

content::WebContents* VoiceFocusedContentsTrackerDesktop::GetActiveWebContents()
    const {
  if (!bound_browser_) {
    return nullptr;
  }
  tabs::TabInterface* active_tab = bound_browser_->GetActiveTabInterface();
  return active_tab ? active_tab->GetContents() : nullptr;
}

void VoiceFocusedContentsTrackerDesktop::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  BindToBrowser(browser);
}

void VoiceFocusedContentsTrackerDesktop::OnBrowserClosed(
    BrowserWindowInterface* browser) {
  if (!bound_browser_ || browser != bound_browser_) {
    return;
  }
  BindToBrowser(
      browser_collection_observation_.GetSource()->GetLastActiveBrowser());
}

void VoiceFocusedContentsTrackerDesktop::BindToBrowser(
    BrowserWindowInterface* browser) {
  if (bound_browser_ == browser) {
    return;
  }
  const bool had_active_contents = bound_browser_ != nullptr;
  active_tab_subscription_ = {};
  bound_browser_ = browser;
  if (bound_browser_) {
    active_tab_subscription_ =
        bound_browser_->RegisterActiveTabDidChange(base::BindRepeating(
            &VoiceFocusedContentsTrackerDesktop::OnBoundBrowserActiveTabChanged,
            base::Unretained(this)));
  }
  if (had_active_contents || GetActiveWebContents()) {
    NotifyVoiceFocusedContentsChanged();
  }
}

void VoiceFocusedContentsTrackerDesktop::OnBoundBrowserActiveTabChanged(
    BrowserWindowInterface* browser) {
  CHECK_EQ(browser, bound_browser_);
  NotifyVoiceFocusedContentsChanged();
}

}  // namespace ttc
