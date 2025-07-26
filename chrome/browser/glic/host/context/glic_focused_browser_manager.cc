// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_focused_browser_manager.h"

#include "base/functional/bind.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/cocoa/appkit_utils.h"
#endif

namespace glic {

namespace {
constexpr base::TimeDelta kDebounceDelay = base::Seconds(0.1);

// Returns whether `a` and `b` both point to the same object.
// Note that if both `a` and `b` are invalidated, this returns true, even if
// the object they once pointed to is different. For our purposes, this is OK.
// This code helps address focus state changes from an old state that's since
// been invalidated to a new state that is now nullptr (we want to treat this
// as a "focus changed" scenario and notify).
template <typename T>
bool IsWeakPtrSame(const base::WeakPtr<T>& a, const base::WeakPtr<T>& b) {
  return std::make_pair(a.get(), a.WasInvalidated()) ==
         std::make_pair(b.get(), b.WasInvalidated());
}
}  // namespace

GlicFocusedBrowserManager::GlicFocusedBrowserManager(
    GlicWindowController* window_controller)
    : window_controller_(*window_controller) {
  BrowserList::GetInstance()->AddObserver(this);
  window_activation_subscription_ =
      window_controller->AddWindowActivationChangedCallback(base::BindRepeating(
          &GlicFocusedBrowserManager::OnGlicWindowActivationChanged,
          base::Unretained(this)));
  window_controller->AddStateObserver(this);
}

GlicFocusedBrowserManager::~GlicFocusedBrowserManager() {
  browser_subscriptions_.clear();
  widget_observation_.Reset();
  BrowserList::GetInstance()->RemoveObserver(this);
  window_controller_->RemoveStateObserver(this);
}

BrowserWindowInterface* GlicFocusedBrowserManager::GetFocusedBrowser() const {
  return focused_browser_state_.focused_browser.get();
}

BrowserWindowInterface* GlicFocusedBrowserManager::GetCandidateBrowser() const {
  return focused_browser_state_.candidate_browser.get();
}

base::CallbackListSubscription
GlicFocusedBrowserManager::AddFocusedBrowserChangedCallback(
    FocusedBrowserChangedCallback callback) {
  return focused_browser_callback_list_.Add(std::move(callback));
}

void GlicFocusedBrowserManager::OnBrowserAdded(Browser* browser) {
  if (IsBrowserValidForSharingInProfile(browser,
                                        window_controller_->profile())) {
    std::vector<base::CallbackListSubscription> subscriptions;
    subscriptions.push_back(browser->RegisterDidBecomeActive(
        base::BindRepeating(&GlicFocusedBrowserManager::OnBrowserBecameActive,
                            base::Unretained(this))));
    subscriptions.push_back(browser->RegisterDidBecomeInactive(
        base::BindRepeating(&GlicFocusedBrowserManager::OnBrowserBecameInactive,
                            base::Unretained(this))));
    browser_subscriptions_[browser] = std::move(subscriptions);
  }
}

void GlicFocusedBrowserManager::OnBrowserRemoved(Browser* browser) {
  // Remove the browser if it exists in the map.
  browser_subscriptions_.erase(browser);
  MaybeUpdateFocusedBrowser();
}

void GlicFocusedBrowserManager::OnBrowserBecameActive(
    BrowserWindowInterface* browser_interface) {
  // Observe for browser window minimization changes.
  widget_observation_.Reset();
  views::Widget* widget = browser_interface->TopContainer()->GetWidget();
  widget_observation_.Observe(widget);

  MaybeUpdateFocusedBrowser();
}

void GlicFocusedBrowserManager::OnBrowserBecameInactive(
    BrowserWindowInterface* browser_interface) {
  // Debounce these updates in case Glic Window is about to become active.
  MaybeUpdateFocusedBrowser(/*debounce=*/true);
}

void GlicFocusedBrowserManager::OnGlicWindowActivationChanged(bool active) {
  // Debounce updates when Glic Window becomes inactive in case a browser window
  // is about to become active.
  MaybeUpdateFocusedBrowser(/*debounce=*/!active);
}

void GlicFocusedBrowserManager::OnWidgetShowStateChanged(
    views::Widget* widget) {
  MaybeUpdateFocusedBrowser();
}

void GlicFocusedBrowserManager::OnWidgetVisibilityChanged(views::Widget* widget,
                                                          bool visible) {
  MaybeUpdateFocusedBrowser();
}

void GlicFocusedBrowserManager::OnWidgetVisibilityOnScreenChanged(
    views::Widget* widget,
    bool visible) {
  MaybeUpdateFocusedBrowser();
}

void GlicFocusedBrowserManager::OnWidgetDestroyed(views::Widget* widget) {
  widget_observation_.Reset();
}

void GlicFocusedBrowserManager::PanelStateChanged(const mojom::PanelState&,
                                                  Browser*) {
  MaybeUpdateFocusedBrowser();
}

void GlicFocusedBrowserManager::MaybeUpdateFocusedBrowser(bool debounce) {
  if (debounce) {
    debouncer_.Start(
        FROM_HERE, kDebounceDelay,
        base::BindOnce(
            &GlicFocusedBrowserManager::PerformMaybeUpdateFocusedBrowser,
            base::Unretained(this)));
  } else {
    debouncer_.Stop();
    PerformMaybeUpdateFocusedBrowser();
  }
}

void GlicFocusedBrowserManager::PerformMaybeUpdateFocusedBrowser() {
  FocusedBrowserState new_focused_browser_state = ComputeFocusedBrowserState();
  if (!focused_browser_state_.IsSame(new_focused_browser_state)) {
    focused_browser_state_ = new_focused_browser_state;
    focused_browser_callback_list_.Notify(
        focused_browser_state_.candidate_browser.get(),
        focused_browser_state_.focused_browser.get());
  }
}

GlicFocusedBrowserManager::FocusedBrowserState
GlicFocusedBrowserManager::ComputeFocusedBrowserState() {
  FocusedBrowserState focused_browser_state;
  BrowserWindowInterface* candidate_browser = ComputeBrowserCandidate();
  if (candidate_browser) {
    focused_browser_state.candidate_browser = candidate_browser->GetWeakPtr();
    if (IsBrowserStateValid(candidate_browser)) {
      focused_browser_state.focused_browser =
          focused_browser_state.candidate_browser;
    }
  }
  return focused_browser_state;
}

BrowserWindowInterface* GlicFocusedBrowserManager::ComputeBrowserCandidate() {
#if BUILDFLAG(IS_MAC)
  if (!ui::IsActiveApplication()) {
    return nullptr;
  }
#endif

  if (window_controller_->IsAttached()) {
    // When attached, we only allow focus if attached window is active.
    Browser* const attached_browser = window_controller_->attached_browser();
    if (attached_browser &&
        (attached_browser->IsActive() || window_controller_->IsActive()) &&
        IsBrowserValidForSharingInProfile(attached_browser,
                                          window_controller_->profile())) {
      return attached_browser;
    }
    return nullptr;
  }

  Browser* const active_browser = BrowserList::GetInstance()->GetLastActive();
  if (!active_browser || !IsBrowserValidForSharingInProfile(
                             active_browser, window_controller_->profile())) {
    return nullptr;
  }

  if (window_controller_->IsActive() || active_browser->IsActive()) {
    return active_browser;
  }

  return nullptr;
}

bool GlicFocusedBrowserManager::IsBrowserStateValid(
    BrowserWindowInterface* browser_interface) {
  ui::BaseWindow* window = browser_interface->GetWindow();
  return !window->IsMinimized() && window->IsVisible() &&
         browser_interface->capabilities()->IsVisibleOnScreen();
}

GlicFocusedBrowserManager::FocusedBrowserState::FocusedBrowserState() = default;
GlicFocusedBrowserManager::FocusedBrowserState::~FocusedBrowserState() =
    default;
GlicFocusedBrowserManager::FocusedBrowserState::FocusedBrowserState(
    const FocusedBrowserState& src) = default;
GlicFocusedBrowserManager::FocusedBrowserState&
GlicFocusedBrowserManager::FocusedBrowserState::operator=(
    const FocusedBrowserState& src) = default;

bool GlicFocusedBrowserManager::FocusedBrowserState::IsSame(
    const FocusedBrowserState& other) const {
  return IsWeakPtrSame(candidate_browser, other.candidate_browser) &&
         IsWeakPtrSame(focused_browser, other.focused_browser);
}

}  // namespace glic
