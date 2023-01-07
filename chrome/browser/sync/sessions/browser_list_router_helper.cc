// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/browser_list_router_helper.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace sync_sessions {

BrowserListRouterHelper::BrowserListRouterHelper(
    SyncSessionsWebContentsRouter* router,
    Profile* profile)
    : router_(router), profile_(profile) {
  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    if (browser->profile() == profile_) {
      browser->tab_strip_model()->AddObserver(this);
    }
  }
  browser_list->AddObserver(this);
}

BrowserListRouterHelper::~BrowserListRouterHelper() {
  BrowserList::GetInstance()->RemoveObserver(this);
}

void BrowserListRouterHelper::OnBrowserAdded(Browser* browser) {
  if (browser->profile() == profile_) {
    browser->tab_strip_model()->AddObserver(this);
  }
}

void BrowserListRouterHelper::OnBrowserRemoved(Browser* browser) {
  if (browser->profile() == profile_) {
    browser->tab_strip_model()->RemoveObserver(this);
  }
}

void BrowserListRouterHelper::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  std::vector<content::WebContents*> web_contents;
  if (change.type() == TabStripModelChange::kInserted) {
    for (const TabStripModelChange::ContentsWithIndex& contents :
         change.GetInsert()->contents) {
      web_contents.push_back(contents.contents);
    }
  } else if (change.type() == TabStripModelChange::kReplaced) {
    web_contents.push_back(change.GetReplace()->new_contents);
  } else {
    return;
  }

  for (content::WebContents* contents : web_contents) {
    if (Profile::FromBrowserContext(contents->GetBrowserContext()) ==
        profile_) {
      router_->NotifyTabModified(contents, false);
    }
  }
}

}  // namespace sync_sessions
