// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/browser_list_router_helper.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace sync_sessions {

BrowserListRouterHelper::BrowserListRouterHelper(
    SyncSessionsWebContentsRouter* router,
    Profile* profile)
    : router_(router), profile_(profile) {
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [this](BrowserWindowInterface* browser) {
        if (browser->GetProfile() == profile_) {
          // TODO(crbug.com/452120900): TabStripModel auto-unregistered by dtor
          browser->GetTabStripModel()->AddObserver(this);
        }
        return true;
      });
  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());
}

BrowserListRouterHelper::~BrowserListRouterHelper() = default;

void BrowserListRouterHelper::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  if (browser->GetProfile() == profile_) {
    // TODO(crbug.com/452120900): TabStripModel auto-unregistered by dtor
    browser->GetTabStripModel()->AddObserver(this);
  }
}

void BrowserListRouterHelper::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  std::vector<content::WebContents*> web_contents;
  switch (change.type()) {
    case TabStripModelChange::kInserted:
      for (const TabStripModelChange::ContentsWithIndex& contents :
           change.GetInsert()->contents) {
        web_contents.push_back(contents.contents);
      }
      break;
    case TabStripModelChange::kReplaced:
      web_contents.push_back(change.GetReplace()->new_contents);
      break;
    case TabStripModelChange::kRemoved:
      router_->NotifyTabClosed();
      return;
    case TabStripModelChange::kSelectionOnly:
    case TabStripModelChange::kMoved:
      return;
  }

  for (content::WebContents* contents : web_contents) {
    router_->NotifyTabModified(contents, false);
  }
}

}  // namespace sync_sessions
