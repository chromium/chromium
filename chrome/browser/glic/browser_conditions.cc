// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_conditions.h"

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif  // BUILDFLAG(IS_WIN)

namespace glic {

bool IsBrowserGlicCompatible(Profile* profile, Browser* browser) {
  // A browser is not compatible if it:
  // - is not a TYPE_NORMAL browser
  // - is from a glic-disabled profile
  // - is not visible
  // - uses a different Profile from glic
  // WARNING: updating these conditions will require updating
  // BrowserAttachObservation.
  return GlicEnabling::IsEnabledForProfile(browser->profile()) &&
         browser->is_type_normal() && browser->window()->IsVisible() &&
         browser->profile() == profile;
}

Browser* FindBrowserForAttachment(Profile* profile) {
  // TODO (crbug.com/390472495) Determine which browser to attach to. Currently
  // attaches to the last focused glic-compatible browser.
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    if (IsBrowserGlicCompatible(profile, browser)) {
      return browser;
    }
  }
  return nullptr;
}

bool IsBrowserInForeground(Browser* browser) {
  bool in_foreground = browser->IsActive();
#if BUILDFLAG(IS_WIN)
  // On Windows, clicking the status bar icon makes an active browser window
  // inactive, but it will still be the last active browser. Attach to the
  // last active browser if it's not occluded or minimized.
  if (!in_foreground) {
    in_foreground = browser->window()
                        ->GetNativeWindow()
                        ->GetHost()
                        ->GetNativeWindowOcclusionState() ==
                    aura::Window::OcclusionState::VISIBLE;
  }
#endif  // BUILDFLAG(IS_WIN)
  return in_foreground;
}

bool IsAnyBrowserInForeground() {
  Browser* last_active_browser = BrowserList::GetInstance()->GetLastActive();
  return last_active_browser && last_active_browser->is_type_normal()
             ? IsBrowserInForeground(last_active_browser)
             : false;
}

class BrowserAttachObservationImpl : public BrowserAttachObservation,
                                     public BrowserListObserver,
                                     public views::WidgetObserver {
 public:
  BrowserAttachObservationImpl(Profile* profile,
                               BrowserAttachObserver* observer)
      : profile_(profile),
        observer_(observer),
        browser_list_observation_(this),
        browser_widget_observation_(this) {
    browser_list_observation_.Observe(BrowserList::GetInstance());
    Browser* browser = FindBrowserForAttachment(profile_);
    current_value_ = browser;
    if (browser) {
      browser_widget_observation_.Observe(
          browser->GetBrowserView().GetWidget());
    }
  }

  bool CanAttachToBrowser() const override { return current_value_ != nullptr; }

  // BrowserListObserver implementation.
  void OnBrowserSetLastActive(Browser* browser) override {
    // BrowserList updates the active browser list before this call, so
    // `CheckForChange` will find the correct browser.
    CheckForChange();
  }

  void OnBrowserRemoved(Browser* browser) override {
    if (current_value_ == browser) {
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

 private:
  void CheckForChange() {
    SetBrowserForAttachment(FindBrowserForAttachment(profile_));
  }

  void SetBrowserForAttachment(Browser* browser) {
    if (current_value_ == browser) {
      return;
    }
    browser_widget_observation_.Reset();
    bool could_attach = current_value_ != nullptr;
    bool can_attach = browser != nullptr;
    current_value_ = browser;
    observer_->BrowserForAttachmentChanged(browser);
    if (could_attach != can_attach) {
      observer_->CanAttachToBrowserChanged(can_attach);
    }
    if (current_value_) {
      browser_widget_observation_.Observe(
          current_value_->GetBrowserView().GetWidget());
    }
  }

  raw_ptr<Profile> profile_;
  raw_ptr<Browser> current_value_;
  raw_ptr<BrowserAttachObserver> observer_;
  base::OneShotTimer check_for_change_timer_;
  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      browser_widget_observation_;
};

std::unique_ptr<BrowserAttachObservation> ObserveBrowserForAttachment(
    Profile* profile,
    BrowserAttachObserver* observer) {
  return std::make_unique<BrowserAttachObservationImpl>(profile, observer);
}

}  // namespace glic
