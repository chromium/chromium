// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_GROUP_SYNC_TAB_GROUP_SYNC_TAB_STATE_H_
#define CHROME_BROWSER_TAB_GROUP_SYNC_TAB_GROUP_SYNC_TAB_STATE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_user_data.h"

// This class is used to store information about a tab inside a saved tab group.
class TabGroupSyncTabState
    : public content::WebContentsUserData<TabGroupSyncTabState> {
 public:
  ~TabGroupSyncTabState() override;

  TabGroupSyncTabState(const TabGroupSyncTabState&) = delete;
  TabGroupSyncTabState& operator=(const TabGroupSyncTabState&) = delete;

  // Creates sync tab state for `web_contents`. Once this completes, this tab
  // will be restricted on certain activities.
  static void Create(content::WebContents* web_contents);

  // Removes the tab state for `web_contents`. After this, this tab will
  // be treated like a normal tab and certain activities will be permitted.
  static void Reset(content::WebContents* web_contents);

 protected:
  explicit TabGroupSyncTabState(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<TabGroupSyncTabState>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_TAB_GROUP_SYNC_TAB_GROUP_SYNC_TAB_STATE_H_
