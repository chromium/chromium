// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/tab_group_sync_tab_state.h"

TabGroupSyncTabState::TabGroupSyncTabState(content::WebContents* web_contents)
    : content::WebContentsUserData<TabGroupSyncTabState>(*web_contents) {}

TabGroupSyncTabState::~TabGroupSyncTabState() = default;

void TabGroupSyncTabState::RemoveTabState(content::WebContents* web_contents) {
  web_contents->RemoveUserData(TabGroupSyncTabState::UserDataKey());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabGroupSyncTabState);
