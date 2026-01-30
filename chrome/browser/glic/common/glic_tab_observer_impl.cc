// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/glic_tab_observer_impl.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

GlicTabObserverImpl::GlicTabObserverImpl(Profile* profile,
                                         EventCallback callback)
    : profile_(profile), callback_(std::move(callback)) {
  browser_observation_.Observe(GlobalBrowserCollection::GetInstance());
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [this, profile](BrowserWindowInterface* browser) {
        if (browser->GetProfile() == profile) {
          browser->GetTabStripModel()->AddObserver(this);
        }
        return true;
      });
}

GlicTabObserverImpl::~GlicTabObserverImpl() {
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [this](BrowserWindowInterface* browser) {
        if (browser->GetProfile() == profile_) {
          browser->GetTabStripModel()->RemoveObserver(this);
        }
        return true;
      });
}

void GlicTabObserverImpl::OnBrowserCreated(BrowserWindowInterface* browser) {
  if (browser->GetProfile() == profile_) {
    browser->GetTabStripModel()->AddObserver(this);
  }
}

void GlicTabObserverImpl::OnBrowserClosed(BrowserWindowInterface* browser) {
  if (browser->GetProfile() == profile_) {
    browser->GetTabStripModel()->RemoveObserver(this);
  }
}

void GlicTabObserverImpl::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted) {
    const auto& insert = *change.GetInsert();
    for (const auto& content : insert.contents) {
      tabs::TabInterface* new_tab =
          tab_strip_model->GetTabAtIndex(content.index);
      TabCreationType type = DetermineTabCreationType(new_tab);
      tabs::TabInterface* opener =
          tab_strip_model->GetOpenerOfTabAt(content.index);
      callback_.Run(TabCreationEvent{new_tab, selection.old_tab, opener, type});
    }
    return;
  }

  if (selection.active_tab_changed()) {
    callback_.Run(TabActivationEvent{selection.new_tab, selection.old_tab});
  }

  if (change.type() == TabStripModelChange::kRemoved ||
      change.type() == TabStripModelChange::kMoved ||
      change.type() == TabStripModelChange::kReplaced) {
    callback_.Run(TabMutationEvent{});
  }
}

void GlicTabObserverImpl::OnTabChangedAt(tabs::TabInterface* tab,
                                         int index,
                                         TabChangeType change_type) {
  callback_.Run(TabMutationEvent{});
}

TabCreationType GlicTabObserverImpl::DetermineTabCreationType(
    tabs::TabInterface* new_tab) {
  content::WebContents* new_contents = new_tab->GetContents();
  if (!new_contents) {
    return TabCreationType::kUnknown;
  }

  content::NavigationController& controller = new_contents->GetController();
  content::NavigationEntry* entry = controller.GetPendingEntry();
  if (!entry) {
    // If there's no pending entry, it's not a user-initiated new tab
    // in the way we're looking for.
    return TabCreationType::kUnknown;
  }

  // Only care about user-initiated new tab navigations via the plus
  // button or Ctrl+T.
  if (ui::PageTransitionCoreTypeIs(entry->GetTransitionType(),
                                   ui::PAGE_TRANSITION_TYPED)) {
    return TabCreationType::kUserInitiated;
  }

  if (ui::PageTransitionCoreTypeIs(entry->GetTransitionType(),
                                   ui::PAGE_TRANSITION_LINK)) {
    return TabCreationType::kFromLink;
  }

  return TabCreationType::kUnknown;
}
