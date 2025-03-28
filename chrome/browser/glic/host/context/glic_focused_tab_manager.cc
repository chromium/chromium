// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_focused_tab_manager.h"

#include <optional>

#include "base/functional/bind.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
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

GlicFocusedTabManager::GlicFocusedTabManager(
    Profile* profile,
    GlicWindowController& window_controller)
    : profile_(profile),
      window_controller_(window_controller),
      focused_tab_data_(NoFocusedTabData()) {
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
  // Debounce these updates in case Glic Window is about to become active.
  MaybeUpdateFocusedTab(/*force_notify=*/true, /*debounce=*/true);
}

void GlicFocusedTabManager::OnGlicWindowActivationChanged(bool active) {
  // Debounce updates when Glic Window becomes inactive in case a browser window
  // is about to become active.
  MaybeUpdateFocusedTab(/*force_notify=*/false, /*debounce=*/!active);
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

void GlicFocusedTabManager::MaybeUpdateFocusedTab(bool force_notify,
                                                  bool debounce) {
  // Cache any calls with force_notify set to true so they don't get swallowed
  // by subsequent calls without it. Otherwise necessary updates might get
  // dropped.
  if (force_notify) {
    cached_force_notify_ = true;
  }
  if (debounce) {
    debouncer_.Start(
        FROM_HERE, kDebounceDelay,
        base::BindOnce(&GlicFocusedTabManager::PerformMaybeUpdateFocusedTab,
                       base::Unretained(this), cached_force_notify_));
  } else {
    // Stop any pending debounced calls so they don't fire needlessly later.
    debouncer_.Stop();
    PerformMaybeUpdateFocusedTab(cached_force_notify_);
  }
}

void GlicFocusedTabManager::PerformMaybeUpdateFocusedTab(bool force_notify) {
  cached_force_notify_ = false;
  struct FocusedTabState new_focused_tab_state = ComputeFocusedTabState();
  bool focus_changed = !focused_tab_state_.IsSame(new_focused_tab_state);
  if (focus_changed) {
    focused_tab_state_ = new_focused_tab_state;
    focused_tab_data_ = GetFocusedTabData(new_focused_tab_state);
  }

  // If we have one, observe tab candidate. If not, whether that's because there
  // was never one, or because it's been invalidated, turn off tab candidate
  // observation.
  Observe(focused_tab_state_.candidate_tab.get());

  if (focus_changed || force_notify) {
    NotifyFocusedTabChanged();
  }
}

struct GlicFocusedTabManager::FocusedTabState
GlicFocusedTabManager::ComputeFocusedTabState() {
  struct FocusedTabState focused_tab_state = FocusedTabState();

  BrowserWindowInterface* candidate_browser = ComputeBrowserCandidate();
  if (candidate_browser) {
    focused_tab_state.candidate_browser = candidate_browser->GetWeakPtr();
  }
  if (!IsBrowserStateValid(candidate_browser)) {
    return focused_tab_state;
  }

  focused_tab_state.focused_browser = focused_tab_state.candidate_browser;

  content::WebContents* candidate_tab = ComputeTabCandidate(candidate_browser);
  if (candidate_tab) {
    focused_tab_state.candidate_tab = candidate_tab->GetWeakPtr();
  }
  if (!IsTabStateValid(candidate_tab)) {
    return focused_tab_state;
  }

  focused_tab_state.focused_tab = focused_tab_state.candidate_tab;

  return focused_tab_state;
}

BrowserWindowInterface* GlicFocusedTabManager::ComputeBrowserCandidate() {
  if (window_controller_->IsAttached()) {
    // When attached, we only allow focus if attached window is active.
    Browser* const attached_browser = window_controller_->attached_browser();
    if (attached_browser &&
        (attached_browser->IsActive() || window_controller_->IsActive()) &&
        IsBrowserValid(attached_browser)) {
      return attached_browser;
    }
    return nullptr;
  }

  if (window_controller_->IsActive()) {
    Browser* const profile_last_active =
        chrome::FindLastActiveWithProfile(profile_);
    return IsBrowserValid(profile_last_active) ? profile_last_active : nullptr;
  }

  Browser* const active_browser = BrowserList::GetInstance()->GetLastActive();
  if (active_browser && active_browser->IsActive() &&
      IsBrowserValid(active_browser)) {
    return active_browser;
  }

  return nullptr;
}

content::WebContents* GlicFocusedTabManager::ComputeTabCandidate(
    BrowserWindowInterface* browser_interface) {
  if (IsBrowserValid(browser_interface) &&
      IsBrowserStateValid(browser_interface)) {
    content::WebContents* active_contents =
        browser_interface->GetActiveTabInterface()
            ? browser_interface->GetActiveTabInterface()->GetContents()
            : nullptr;
    if (IsTabValid(active_contents)) {
      return active_contents;
    }
  }

  return nullptr;
}

void GlicFocusedTabManager::NotifyFocusedTabChanged() {
  focused_callback_list_.Notify(GetFocusedTabData());
}

bool GlicFocusedTabManager::IsBrowserValid(
    BrowserWindowInterface* browser_interface) {
  if (!browser_interface) {
    return false;
  }

  if (browser_interface->GetProfile() != profile_) {
    return false;
  }

  if (browser_interface->GetProfile()->IsOffTheRecord()) {
    return false;
  }

  return true;
}

bool GlicFocusedTabManager::IsBrowserStateValid(
    BrowserWindowInterface* browser_interface) {
  if (!browser_interface) {
    return false;
  }

  if (browser_interface->IsMinimized()) {
    return false;
  }

  return true;
}

bool GlicFocusedTabManager::IsTabValid(content::WebContents* web_contents) {
  return web_contents != nullptr;
}

bool GlicFocusedTabManager::IsTabStateValid(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }

  auto url =
      const_cast<content::WebContents*>(web_contents)->GetLastCommittedURL();
  if (url.SchemeIsHTTPOrHTTPS() || url.SchemeIsFile()) {
    return true;
  }

  return false;
}

FocusedTabData GlicFocusedTabManager::GetFocusedTabData(
    const GlicFocusedTabManager::FocusedTabState& focused_state) {
  if (focused_state.focused_tab) {
    return {focused_state.focused_tab};
  }

  if (focused_state.candidate_tab) {
    return {NoFocusedTabData("no focusable tab",
                             focused_state.candidate_tab.get())};
  }

  if (focused_state.focused_browser) {
    return {NoFocusedTabData("no focusable tab")};
  }

  if (focused_state.candidate_browser) {
    return {NoFocusedTabData("no focusable browser window")};
  }

  return {NoFocusedTabData("no browser window")};
}

FocusedTabData GlicFocusedTabManager::GetFocusedTabData() {
  return focused_tab_data_;
}

GlicFocusedTabManager::FocusedTabState::FocusedTabState() = default;
GlicFocusedTabManager::FocusedTabState::~FocusedTabState() = default;
GlicFocusedTabManager::FocusedTabState::FocusedTabState(
    const GlicFocusedTabManager::FocusedTabState& src) = default;
GlicFocusedTabManager::FocusedTabState&
GlicFocusedTabManager::FocusedTabState::operator=(
    const GlicFocusedTabManager::FocusedTabState& src) = default;

bool GlicFocusedTabManager::FocusedTabState::IsSame(
    const FocusedTabState& other) const {
  return IsWeakPtrSame(candidate_browser, other.candidate_browser) &&
         IsWeakPtrSame(focused_browser, other.focused_browser) &&
         IsWeakPtrSame(candidate_tab, other.candidate_tab) &&
         IsWeakPtrSame(focused_tab, other.focused_tab);
}
}  // namespace glic
