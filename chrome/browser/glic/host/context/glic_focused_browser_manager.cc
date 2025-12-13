// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_focused_browser_manager.h"

#include "base/functional/bind.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"
#include "chrome/browser/glic/widget/glic_window_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/cocoa/appkit_utils.h"
#endif

namespace glic {

namespace {
constexpr base::TimeDelta kDebounceDelay = base::Seconds(0.1);
bool g_testing_mode = false;

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

void GlicFocusedBrowserManager::SetTestingModeForTesting(bool testing_mode) {
  g_testing_mode = testing_mode;
}

GlicFocusedBrowserManager::GlicFocusedBrowserManager(
    GlicInstance::UIDelegate* window_controller,
    Profile* profile)
    : window_controller_(*window_controller), profile_(profile) {
  if (!GlicEnabling::IsMultiInstanceEnabled()) {
    GlicWindowControllerImpl* window_controller_impl =
        static_cast<GlicWindowControllerImpl*>(window_controller);
    window_activation_subscription_ =
        window_controller_impl->AddWindowActivationChangedCallback(
            base::BindRepeating(
                &GlicFocusedBrowserManager::OnGlicWindowActivationChanged,
                base::Unretained(this)));
  }
}

GlicFocusedBrowserManager::~GlicFocusedBrowserManager() {
  browser_subscriptions_.clear();
  widget_observation_.Reset();
  BrowserList::GetInstance()->RemoveObserver(this);
  window_controller_->RemoveStateObserver(this);
}

void GlicFocusedBrowserManager::Initialize() {
  BrowserList::GetInstance()->AddObserver(this);
  window_controller_->AddStateObserver(this);
  for (Browser* browser : *BrowserList::GetInstance()) {
    OnBrowserAdded(browser);
    if (browser->IsActive()) {
      OnBrowserBecameActive(browser);
    }
  }
  MaybeUpdateFocusedBrowser();
  is_initialized_ = true;
}

BrowserWindowInterface* GlicFocusedBrowserManager::GetFocusedBrowser() const {
  return browser_state_.focused_state.focused_browser.get();
}

BrowserWindowInterface* GlicFocusedBrowserManager::GetCandidateBrowser() const {
  return browser_state_.focused_state.candidate_browser.get();
}

BrowserWindowInterface* GlicFocusedBrowserManager::GetActiveBrowser() const {
  return browser_state_.active_browser.get();
}

base::CallbackListSubscription
GlicFocusedBrowserManager::AddFocusedBrowserChangedCallback(
    FocusedBrowserChangedCallback callback) {
  if (!is_initialized_) {
    Initialize();
  }
  return focused_browser_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
GlicFocusedBrowserManager::AddActiveBrowserChangedCallback(
    base::RepeatingCallback<void(BrowserWindowInterface*)> callback) {
  if (!is_initialized_) {
    Initialize();
  }
  return active_browser_callback_list_.Add(std::move(callback));
}

void GlicFocusedBrowserManager::OnBrowserAdded(Browser* browser) {
  if (IsBrowserValidForSharingInProfile(browser, profile_)) {
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
  views::Widget* widget =
      BrowserElementsViews::From(browser_interface)->GetPrimaryWindowWidget();
  widget_observation_.Observe(widget);

  MaybeUpdateFocusedBrowser();
}

void GlicFocusedBrowserManager::OnBrowserBecameInactive(
    BrowserWindowInterface* browser_interface) {
  // Debounce these updates in case Glic Window is about to become active.
  MaybeUpdateFocusedBrowser(/*debounce=*/true);
}

void GlicFocusedBrowserManager::OnGlicWindowActivationChanged(bool active) {
  if (!is_initialized_) {
    Initialize();
  }
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

void GlicFocusedBrowserManager::PanelStateChanged(
    const mojom::PanelState&,
    const GlicWindowController::PanelStateContext& context) {
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
  BrowserState old_state = browser_state_;
  browser_state_ = ComputeBrowserState();
  if (!IsWeakPtrSame(old_state.active_browser, browser_state_.active_browser)) {
    active_browser_callback_list_.Notify(browser_state_.active_browser.get());
  }
  if (!old_state.focused_state.IsSame(browser_state_.focused_state)) {
    focused_browser_callback_list_.Notify(
        browser_state_.focused_state.candidate_browser.get(),
        browser_state_.focused_state.focused_browser.get());
  }
}

GlicFocusedBrowserManager::BrowserState
GlicFocusedBrowserManager::ComputeBrowserState() {
  BrowserState browser_state;
  browser_state.focused_state = ComputeFocusedBrowserState();
  BrowserWindowInterface* active_browser = ComputeActiveBrowser();
  browser_state.active_browser =
      active_browser ? active_browser->GetWeakPtr() : nullptr;
  return browser_state;
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
  BrowserWindowInterface* active_browser = ComputeActiveBrowser();
  if (!active_browser ||
      !IsBrowserValidForSharingInProfile(active_browser, profile_)) {
    return nullptr;
  }

  return active_browser;
}

BrowserWindowInterface* GlicFocusedBrowserManager::ComputeActiveBrowser() {
#if BUILDFLAG(IS_MAC)
  // Ignore this check when testing because we can't guarantee that the
  // application is active.
  if (!g_testing_mode && !ui::IsActiveApplication()) {
    VLOG(1) << "ActiveBrowserCalc: App not active";
    return nullptr;
  }
#endif

  BrowserWindowInterface* const bwi =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  if (!bwi) {
    VLOG(1) << "ActiveBrowserCalc: No active browser";
    return nullptr;
  }
  if (!(window_controller_->IsActive() &&
        window_controller_->GetPanelState().kind ==
            mojom::PanelStateKind::kDetached) &&
      !bwi->IsActive()) {
    VLOG(1) << "ActiveBrowserCalc: !IsActive()";
    return nullptr;
  }
  VLOG(1) << "ActiveBrowserCalc: active browser";
  return bwi;
}

bool GlicFocusedBrowserManager::IsBrowserStateValid(
    BrowserWindowInterface* browser_interface) {
  ui::BaseWindow* window = browser_interface->GetWindow();
  return !window->IsMinimized() && window->IsVisible() &&
         // Disable this check for some tests. See crbug.com/447705905.
         (g_testing_mode ||
          browser_interface->capabilities()->IsVisibleOnScreen());
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

GlicFocusedBrowserManager::BrowserState::BrowserState() = default;
GlicFocusedBrowserManager::BrowserState::~BrowserState() = default;
GlicFocusedBrowserManager::BrowserState::BrowserState(const BrowserState& src) =
    default;
GlicFocusedBrowserManager::BrowserState&
GlicFocusedBrowserManager::BrowserState::operator=(const BrowserState& src) =
    default;

}  // namespace glic
