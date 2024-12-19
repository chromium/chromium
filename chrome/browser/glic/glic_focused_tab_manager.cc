// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_focused_tab_manager.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"

namespace glic {

// TODO(wry): Add interactive_ui_tests to check basic functionality.

GlicFocusedTabManager::GlicFocusedTabManager(
    Profile* profile,
    GlicWindowController& window_controller)
    : profile_(profile), window_controller_(window_controller) {
  BrowserList::GetInstance()->AddObserver(this);
  window_activation_subscription_ =
      window_controller.AddWindowActivationChangedCallback(base::BindRepeating(
          &GlicFocusedTabManager::OnGlicWindowActivationChanged,
          base::Unretained(this)));
}

GlicFocusedTabManager::~GlicFocusedTabManager() {
  BrowserList::GetInstance()->RemoveObserver(this);
}

base::CallbackListSubscription
GlicFocusedTabManager::AddFocusedTabChangedCallback(
    FocusedTabChangedCallback callback) {
  return focused_callback_list_.Add(std::move(callback));
}

void GlicFocusedTabManager::OnBrowserSetLastActive(Browser* browser) {
  // Clear any existing browser callback subscription.
  browser_subscription_ = {};

  // Subscribe to active tab changes to this browser if it's valid.
  if (IsValidBrowser(browser)) {
    browser_subscription_ = browser->RegisterActiveTabDidChange(
        base::BindRepeating(&GlicFocusedTabManager::OnActiveTabChanged,
                            base::Unretained(this)));
  }

  MaybeUpdateFocusedTab();
}

void GlicFocusedTabManager::OnBrowserNoLongerActive(Browser* browser) {
  MaybeUpdateFocusedTab();
}

void GlicFocusedTabManager::OnGlicWindowActivationChanged(bool active) {
  MaybeUpdateFocusedTab();
}

void GlicFocusedTabManager::OnActiveTabChanged(
    BrowserWindowInterface* browser_interface) {
  MaybeUpdateFocusedTab();
}

void GlicFocusedTabManager::PrimaryPageChanged(content::Page& page) {
  // We always want to trigger our notify callback here (even if focused tab
  // remains the same) so that subscribers can update if they care about primary
  // page changed events.
  MaybeUpdateFocusedTab(/*force_notify=*/true);
}

void GlicFocusedTabManager::MaybeUpdateFocusedTab(bool force_notify) {
  content::WebContents* const new_focused_web_contents = ComputeFocusedTab();
  bool focus_changed = new_focused_web_contents != focused_web_contents_.get();

  if (focus_changed) {
    if (new_focused_web_contents) {
      focused_web_contents_ = new_focused_web_contents->GetWeakPtr();
    } else {
      focused_web_contents_.reset();
    }

    // This is sufficient for now because there's currently no way for an
    // invalid focusable to become valid without changing |WebContents|.
    Observe(new_focused_web_contents);
  }

  if (focus_changed || force_notify) {
    NotifyFocusedTabChanged();
  }
}

content::WebContents* GlicFocusedTabManager::ComputeFocusedTab() {
  if (window_controller_->IsActive()) {
    Browser* const profile_last_active =
        chrome::FindLastActiveWithProfile(profile_);
    return ComputeFocusableTabForBrowser(profile_last_active);
  }

  Browser* const active_browser = BrowserList::GetInstance()->GetLastActive();
  if (active_browser && active_browser->IsActive()) {
    return ComputeFocusableTabForBrowser(active_browser);
  }

  return nullptr;
}

content::WebContents* GlicFocusedTabManager::ComputeFocusableTabForBrowser(
    BrowserWindowInterface* browser_interface) {
  if (IsValidBrowser(browser_interface)) {
    content::WebContents* const web_contents =
        browser_interface->GetActiveTabInterface()
            ? browser_interface->GetActiveTabInterface()->GetContents()
            : nullptr;
    if (IsValidFocusable(web_contents)) {
      return web_contents;
    }
  }

  return nullptr;
}

void GlicFocusedTabManager::NotifyFocusedTabChanged() {
  // TODO(wry): Debounce here to avoid awkwardness with Mac OS
  // deactivation/activation handling.
  focused_callback_list_.Notify(GetWebContentsForFocusedTab());
}

bool GlicFocusedTabManager::IsValidBrowser(
    BrowserWindowInterface* browser_interface) {
  // TODO(wry): Handle browser minimized.
  return browser_interface && browser_interface->GetProfile() == profile_ &&
         !browser_interface->GetProfile()->IsOffTheRecord();
}

bool GlicFocusedTabManager::IsValidFocusable(
    content::WebContents* web_contents) {
  // Changes here may also require new handling of |WebContents| observing.
  return web_contents;
}

content::WebContents* GlicFocusedTabManager::GetWebContentsForFocusedTab() {
  return focused_web_contents_.get();
}

}  // namespace glic
