// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_focused_tab_manager.h"

#include <optional>

#include "base/functional/bind.h"
#include "chrome/browser/glic/glic_tab_data.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/common/url_constants.h"
#include "ui/views/widget/widget.h"

namespace glic {

namespace {

constexpr base::TimeDelta kDebounceDelay = base::Seconds(0.1);

}  // namespace

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
  window_controller.AddStateObserver(this);
}

GlicFocusedTabManager::~GlicFocusedTabManager() {
  browser_subscriptions_.clear();
  widget_observation_.Reset();
  BrowserList::GetInstance()->RemoveObserver(this);
  window_controller_->RemoveStateObserver(this);
}

base::CallbackListSubscription
GlicFocusedTabManager::AddFocusedTabChangedCallback(
    FocusedTabChangedCallback callback) {
  return focused_callback_list_.Add(std::move(callback));
}

void GlicFocusedTabManager::OnBrowserAdded(Browser* browser) {
  // Subscribe to active tab changes to this browser if it's valid.
  if (IsBrowserValid(browser)) {
    std::vector<base::CallbackListSubscription> subscriptions;

    subscriptions.push_back(browser->RegisterDidBecomeActive(
        base::BindRepeating(&GlicFocusedTabManager::OnBrowserBecameActive,
                            base::Unretained(this))));

    subscriptions.push_back(browser->RegisterDidBecomeInactive(
        base::BindRepeating(&GlicFocusedTabManager::OnBrowserBecameInactive,
                            base::Unretained(this))));

    subscriptions.push_back(browser->RegisterActiveTabDidChange(
        base::BindRepeating(&GlicFocusedTabManager::OnActiveTabChanged,
                            base::Unretained(this))));

    browser_subscriptions_[browser] = std::move(subscriptions);
  }
}

void GlicFocusedTabManager::OnBrowserRemoved(Browser* browser) {
  // Remove the browser if it exists in the map.
  browser_subscriptions_.erase(browser);
  MaybeUpdateFocusedTab();
}

void GlicFocusedTabManager::OnBrowserBecameActive(
    BrowserWindowInterface* browser_interface) {
  // Observe for browser window minimization changes.
  widget_observation_.Reset();
  views::Widget* widget = browser_interface->TopContainer()->GetWidget();
  widget_observation_.Observe(widget);

  // We need to force-notify because even if the focused tab doesn't change, it
  // can be in a different browser window (i.e., the user drag-n-drop the
  // focused tab into a new window). Let the subscribers to decide what to do in
  // this case.
  //
  // TODO(crbug.com/393578218): We should have dedicated subscription lists for
  // different types of notifications.
  MaybeUpdateFocusedTab(/*force_notify=*/true);
}

void GlicFocusedTabManager::OnBrowserBecameInactive(
    BrowserWindowInterface* browser_interface) {
  MaybeUpdateFocusedTab(/*force_notify=*/true);
}

void GlicFocusedTabManager::OnGlicWindowActivationChanged(bool active) {
  MaybeUpdateFocusedTab();
}

void GlicFocusedTabManager::OnWidgetShowStateChanged(views::Widget* widget) {
  MaybeUpdateFocusedTab();
}

void GlicFocusedTabManager::OnWidgetDestroyed(views::Widget* widget) {
  widget_observation_.Reset();
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

void GlicFocusedTabManager::PanelStateChanged(
    const glic::mojom::PanelState& panel_state,
    Browser*) {
  MaybeUpdateFocusedTab();
}

void GlicFocusedTabManager::MaybeUpdateFocusedTab(bool force_notify) {
  // Cache any calls with force_notify set to true so they don't get swallowed
  // by subsequent calls without it. Otherwise necessary updates might get
  // dropped.
  if (force_notify) {
    cached_force_notify_ = true;
  }
  debouncer_.Start(
      FROM_HERE, kDebounceDelay,
      base::BindOnce(&GlicFocusedTabManager::PerformMaybeUpdateFocusedTab,
                     base::Unretained(this), cached_force_notify_));
}

void GlicFocusedTabManager::PerformMaybeUpdateFocusedTab(bool force_notify) {
  cached_force_notify_ = false;
  FocusedTabData new_focused_tab_data = ComputeFocusedTabData();
  bool focus_changed = focused_tab_data_ != new_focused_tab_data;
  if (focus_changed) {
    focused_tab_data_ = new_focused_tab_data;

    // This is sufficient for now because there's currently no way for an
    // invalid focusable to become valid without changing |WebContents|.
    Observe(focused_tab_data_.focused_tab_contents.get());
  }

  if (focus_changed || force_notify) {
    NotifyFocusedTabChanged();
  }
}

FocusedTabData GlicFocusedTabManager::ComputeFocusedTabData() {
  if (window_controller_->IsAttached()) {
    // When attached, we only allow focus if attached window is active.
    Browser* const attached_browser = window_controller_->attached_browser();
    if (attached_browser &&
        (attached_browser->IsActive() || window_controller_->IsActive())) {
      return ComputeFocusableTabDataForBrowser(attached_browser);
    }

    return FocusedTabData(nullptr, std::nullopt,
                          glic::mojom::NoCandidateTabError::kNoFocusableTabs);
  }

  if (window_controller_->IsActive()) {
    Browser* const profile_last_active =
        chrome::FindLastActiveWithProfile(profile_);
    return ComputeFocusableTabDataForBrowser(profile_last_active);
  }

  Browser* const active_browser = BrowserList::GetInstance()->GetLastActive();
  if (active_browser && active_browser->IsActive()) {
    return ComputeFocusableTabDataForBrowser(active_browser);
  }

  // Otherwise return FocusedTabData with NoCandidateTabError::kNoFocusableTabs.
  return FocusedTabData(nullptr, std::nullopt,
                        glic::mojom::NoCandidateTabError::kNoFocusableTabs);
}

FocusedTabData GlicFocusedTabManager::ComputeFocusableTabDataForBrowser(
    Browser* browser) {
  if (IsBrowserValid(browser) && IsBrowserStateValid(browser)) {
    content::WebContents* const web_contents =
        browser->GetActiveTabInterface()
            ? browser->GetActiveTabInterface()->GetContents()
            : nullptr;
    std::optional<glic::mojom::NoCandidateTabError> no_focused_tab_error =
        IsValidCandidate(web_contents);
    std::optional<glic::mojom::InvalidCandidateError> invalid_candidate_error =
        IsValidFocusable(web_contents);
    return FocusedTabData(web_contents, invalid_candidate_error,
                          no_focused_tab_error);
  }
  // Otherwise return FocusedTabData with null web_contents and kNoFocusableTabs
  // error.
  return FocusedTabData(nullptr, std::nullopt,
                        glic::mojom::NoCandidateTabError::kNoFocusableTabs);
}

void GlicFocusedTabManager::NotifyFocusedTabChanged() {
  focused_callback_list_.Notify(GetFocusedTabData());
}

bool GlicFocusedTabManager::IsBrowserValid(Browser* browser) {
  if (!browser) {
    return false;
  }

  if (browser->GetProfile() != profile_) {
    return false;
  }

  if (browser->GetProfile()->IsOffTheRecord()) {
    return false;
  }

  return true;
}

bool GlicFocusedTabManager::IsBrowserStateValid(Browser* browser) {
  if (browser->window()->IsMinimized()) {
    return false;
  }

  return true;
}

std::optional<glic::mojom::NoCandidateTabError>
GlicFocusedTabManager::IsValidCandidate(content::WebContents* web_contents) {
  if (!web_contents) {
    return glic::mojom::NoCandidateTabError::kNoFocusableTabs;
  }

  return std::nullopt;
}

std::optional<glic::mojom::InvalidCandidateError>
GlicFocusedTabManager::IsValidFocusable(content::WebContents* web_contents) {
  if (!web_contents) {
    return glic::mojom::InvalidCandidateError::kUnknown;
  }
  auto url =
      const_cast<content::WebContents*>(web_contents)->GetLastCommittedURL();
  if (url.SchemeIs(content::kChromeDevToolsScheme)) {
    return glic::mojom::InvalidCandidateError::kUnsupportedUrl;
  }
  return std::nullopt;
}

FocusedTabData GlicFocusedTabManager::GetFocusedTabData() {
  return focused_tab_data_;
}
}  // namespace glic
