// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_tab_creation_observer.h"

#include "base/functional/callback.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

GlicTabCreationObserver::GlicTabCreationObserver(Profile* profile,
                                                 TabCreatedCallback callback)
    : profile_(profile), callback_(std::move(callback)) {
  BrowserList::AddObserver(this);
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile() == profile_) {
      browser->tab_strip_model()->AddObserver(this);
    }
  }
}

GlicTabCreationObserver::~GlicTabCreationObserver() {
  BrowserList::RemoveObserver(this);
  // Manually remove observer from all TabStripModels for this profile.
  // This is necessary because BrowserList::RemoveObserver only stops
  // observing new browser additions/removals, not existing TabStripModels.
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile() == profile_) {
      browser->tab_strip_model()->RemoveObserver(this);
    }
  }
}

void GlicTabCreationObserver::OnBrowserAdded(Browser* browser) {
  if (browser->profile() == profile_) {
    browser->tab_strip_model()->AddObserver(this);
  }
}

void GlicTabCreationObserver::OnBrowserRemoved(Browser* browser) {
  if (browser->profile() == profile_) {
    browser->tab_strip_model()->RemoveObserver(this);
  }
}

void GlicTabCreationObserver::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() != TabStripModelChange::kInserted) {
    return;
  }

  for (const auto& contents_with_index : change.GetInsert()->contents) {
    content::WebContents* new_contents = contents_with_index.contents;
    content::NavigationController& controller = new_contents->GetController();
    content::NavigationEntry* entry = controller.GetPendingEntry();
    if (!entry) {
      // If there's no pending entry, it's not a user-initiated new tab
      // in the way we're looking for, so we skip it.
      continue;
    }

    // Only care about user-initiated new tab navigations via the plus button
    // or Ctrl+T.
    if (!ui::PageTransitionCoreTypeIs(entry->GetTransitionType(),
                                      ui::PAGE_TRANSITION_TYPED)) {
      continue;
    }
    if (!selection.old_contents) {
      continue;
    }

    tabs::TabInterface* new_tab =
        tabs::TabInterface::GetFromContents(new_contents);
    tabs::TabInterface* old_tab =
        tabs::TabInterface::GetFromContents(selection.old_contents);

    if (!new_tab || !old_tab) {
      continue;
    }
    callback_.Run(*old_tab, *new_tab);
  }
}
