// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/browser_conditions.h"

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/win/hwnd_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace glic {

namespace {
#if BUILDFLAG(IS_WIN)

struct IsBrowserTopmostWindowState {
  HWND browser_hwnd = nullptr;
  bool browser_is_topmost_window = false;
};

// Window enumerator used to determine if the top most visible window, other
// than the system tray, is the browser window it is looking for. This is called
// in z-order, i.e., topmost window first.
// Window enumerator, so returning FALSE stops enumerating.
// `lParam` is a pointer to IsBrowserTopmostWindowState, which contains the
// browser HWND it is looking for. When finished enumerating, it sets
// topmost_visible_non_opaque_hwnd to the topmost non system tray HWND it
// finds.
BOOL CALLBACK IsBrowserWindowTopmostWindowEnumerator(HWND hwnd, LPARAM lParam) {
  struct IsBrowserTopmostWindowState* state =
      reinterpret_cast<struct IsBrowserTopmostWindowState*>(lParam);
  if (hwnd == state->browser_hwnd) {
    state->browser_is_topmost_window = true;
    return FALSE;
  } else if (gfx::GetClassName(hwnd) != L"Shell_TrayWnd" &&
             gfx::IsWindowVisibleAndFullyOpaque(hwnd, nullptr)) {
    state->browser_is_topmost_window = false;
    return FALSE;
  }
  return TRUE;
}

bool IsBrowserWindowTopmostWindow(BrowserWindowInterface* bwi) {
  HWND browser_hwnd =
      bwi->GetWindow()->GetNativeWindow()->GetHost()->GetAcceleratedWidget();

  struct IsBrowserTopmostWindowState state{browser_hwnd, false};
  EnumWindows(&IsBrowserWindowTopmostWindowEnumerator,
              reinterpret_cast<LPARAM>(&state));
  return state.browser_is_topmost_window;
}

#endif  // BUILDFLAG(IS_WIN)

bool IsBrowserGlicCompatible(Profile* profile,
                             BrowserWindowInterface* browser) {
  // A browser is not compatible if it:
  // - is not a TYPE_NORMAL browser
  // - is from a glic-disabled profile
  // - uses a different Profile from glic
  // WARNING: updating these conditions will require updating
  // BrowserAttachObservation.
  return GlicEnabling::IsEnabledForProfile(browser->GetProfile()) &&
         browser->GetType() == BrowserWindowInterface::TYPE_NORMAL &&
         browser->GetProfile() == profile;
}

}  // namespace

bool IsBrowserGlicAttachable(Profile* profile,
                             BrowserWindowInterface* browser) {
  return IsBrowserGlicCompatible(profile, browser) &&
         browser->GetWindow()->IsVisible() &&
         !browser->GetWindow()->IsMinimized();
}

BrowserWindowInterface* FindBrowserForAttachment(Profile* profile) {
  // TODO (crbug.com/390472495) Determine which browser to attach to. Currently
  // attaches to the last focused glic-compatible browser.
  BrowserWindowInterface* browser_for_attachment = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (IsBrowserGlicAttachable(profile, browser)) {
          browser_for_attachment = browser;
          return false;  // stop iterating
        }
        return true;  // continue iterating
      });
  return browser_for_attachment;
}

bool IsBrowserInForeground(BrowserWindowInterface* bwi) {
  if (bwi->IsActive()) {
    return true;
  }
#if BUILDFLAG(IS_WIN)
  // On Windows, clicking the status bar icon makes an active browser window
  // inactive, but it will still be the last active browser. Attach to the
  // last active browser if it's the foremost visible window, other than the
  // system tray.
  return IsBrowserWindowTopmostWindow(bwi);
#else
  return false;
#endif  // BUILDFLAG(IS_WIN)
}

bool IsBrowserVisible(Browser* browser) {
  return browser && browser->window() &&
         browser->GetBrowserView().GetWidget() &&
         browser->window()->IsVisible() && !browser->window()->IsMinimized() &&
         browser->capabilities()->IsVisibleOnScreen();
}

BrowserWindowInterface* GetActiveGlicEligibleBrowser(Profile* profile) {
  BrowserWindowInterface* const active_bwi =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  if (active_bwi && IsBrowserGlicAttachable(profile, active_bwi) &&
      IsBrowserInForeground(active_bwi)) {
    return active_bwi;
  }
  return nullptr;
}

class BrowserAttachObservationImpl : public BrowserAttachObservation,
                                     public BrowserCollectionObserver,
                                     public views::WidgetObserver {
 public:
  BrowserAttachObservationImpl(Profile* profile,
                               BrowserAttachObserver* observer)
      : profile_(profile),
        observer_(observer),
        browser_collection_observation_(this),
        browser_widget_observations_(this) {
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [this](BrowserWindowInterface* browser_window_interface) {
          OnBrowserCreated(browser_window_interface);
          return true;
        });
    if (profile_->AllowsBrowserWindows()) {
      browser_collection_observation_.Observe(
          ProfileBrowserCollection::GetForProfile(profile_));
    }
    current_value_ = FindBrowserForAttachment(profile_);
  }

  bool CanAttachToBrowser() const override { return current_value_ != nullptr; }

  // BrowserCollectionObserver implementation.
  void OnBrowserActivated(BrowserWindowInterface* browser) override {
    // BrowserList updates the active browser list before this call, so
    // `CheckForChange` will find the correct browser.
    CheckForChange();
  }
  void OnBrowserCreated(BrowserWindowInterface* browser) override {
    if (IsBrowserGlicCompatible(profile_, browser)) {
      browser_widget_observations_.AddObservation(
          browser->GetBrowserForMigrationOnly()->GetBrowserView().GetWidget());
    }
  }
  void OnBrowserClosed(BrowserWindowInterface* browser) override {
    if (current_value_ != nullptr && current_value_ == browser) {
      // BrowserList updates the active browser list before this call, so
      // `CheckForChange` will find the correct browser.
      CheckForChange();
    }
  }

  // views::WidgetObserver implementation.
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override {
    // Note: visibility change takes effect after this call, so PostMessage is
    // critical here.
    check_for_change_timer_.Start(
        FROM_HERE, base::TimeDelta(),
        base::BindOnce(&BrowserAttachObservationImpl::CheckForChange,
                       base::Unretained(this)));
  }
  void OnWidgetShowStateChanged(views::Widget* widget) override {
    CheckForChange();
  }
  void OnWidgetDestroyed(views::Widget* widget) override {
    // Note: widget observer removal has to be done at widget destruction time
    // because when OnBrowserRemoved is called, the widget has already
    // been destroyed.
    if (browser_widget_observations_.IsObservingSource(widget)) {
      browser_widget_observations_.RemoveObservation(widget);
    }
  }

 private:
  void CheckForChange() {
    SetBrowserForAttachment(FindBrowserForAttachment(profile_));
  }

  void SetBrowserForAttachment(BrowserWindowInterface* browser) {
    if (current_value_ == browser) {
      return;
    }
    bool could_attach = current_value_ != nullptr;
    bool can_attach = browser != nullptr;
    current_value_ = browser;
    observer_->BrowserForAttachmentChanged(browser);
    if (could_attach != can_attach) {
      observer_->CanAttachToBrowserChanged(can_attach);
    }
  }

  raw_ptr<Profile> profile_;
  raw_ptr<BrowserWindowInterface> current_value_;
  raw_ptr<BrowserAttachObserver> observer_;
  base::OneShotTimer check_for_change_timer_;
  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_;
  base::ScopedMultiSourceObservation<views::Widget, views::WidgetObserver>
      browser_widget_observations_;
};

std::unique_ptr<BrowserAttachObservation> ObserveBrowserForAttachment(
    Profile* profile,
    BrowserAttachObserver* observer) {
  return std::make_unique<BrowserAttachObservationImpl>(profile, observer);
}

}  // namespace glic
