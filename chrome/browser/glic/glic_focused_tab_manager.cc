// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_focused_tab_manager.h"

#include "base/functional/bind.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views/widget/widget.h"

namespace glic {

namespace {

constexpr base::TimeDelta kDebounceDelay = base::Seconds(0.5);

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
  widget_observation_.Reset();
  BrowserList::GetInstance()->RemoveObserver(this);
  window_controller_->RemoveStateObserver(this);
}

base::CallbackListSubscription
GlicFocusedTabManager::AddFocusedTabChangedCallback(
    FocusedTabChangedCallback callback) {
  return focused_callback_list_.Add(std::move(callback));
}

void GlicFocusedTabManager::OnBrowserSetLastActive(Browser* browser) {
  // Clear any existing browser callback subscription.
  browser_subscription_ = {};
  widget_observation_.Reset();

  // Subscribe to active tab changes to this browser if it's valid.
  if (IsBrowserValid(browser)) {
    browser_subscription_ = browser->RegisterActiveTabDidChange(
        base::BindRepeating(&GlicFocusedTabManager::OnActiveTabChanged,
                            base::Unretained(this)));

    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    if (browser_view) {
      views::Widget* widget = browser_view->GetWidget();
      if (widget) {
        widget_observation_.Observe(widget);
      }
    }
  }

  // We need to force-notify because even if the focused tab doesn't change, it
  // can be in a different browser window (i.e., the user drag-n-drop the
  // focused tab into a new window). Let the subscribers to decide what to do in
  // this case.
  //
  // TODO(crbug.com/393578218): We should have dedicated subscription lists for
  // different types of notifications.
  MaybeUpdateFocusedTab(/*force_notify=*/true);
}

void GlicFocusedTabManager::OnBrowserNoLongerActive(Browser* browser) {
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
  content::WebContents* const new_focused_web_contents = ComputeFocusedTab();
  bool focus_changed =
      (focused_web_contents_.WasInvalidated() ||
       new_focused_web_contents != focused_web_contents_.get());

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
  if (window_controller_->IsAttached()) {
    // When attached, we only allow focus if attached window is active.
    Browser* const attached_browser = window_controller_->attached_browser();
    if (attached_browser &&
        (attached_browser->IsActive() || window_controller_->IsActive())) {
      return ComputeFocusableTabForBrowser(attached_browser);
    }

    return nullptr;
  }

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
    Browser* browser) {
  if (IsBrowserValid(browser) && IsBrowserStateValid(browser)) {
    content::WebContents* const web_contents =
        browser->GetActiveTabInterface()
            ? browser->GetActiveTabInterface()->GetContents()
            : nullptr;
    if (IsValidFocusable(web_contents)) {
      return web_contents;
    }
  }

  return nullptr;
}

void GlicFocusedTabManager::NotifyFocusedTabChanged() {
  focused_callback_list_.Notify(GetWebContentsForFocusedTab());
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

bool GlicFocusedTabManager::IsValidFocusable(
    content::WebContents* web_contents) {
  // Changes here may also require new handling of `WebContents` observing.
  return web_contents;
}

content::WebContents* GlicFocusedTabManager::GetWebContentsForFocusedTab() {
  return focused_web_contents_.get();
}

}  // namespace glic
