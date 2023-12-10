// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_TAB_RESUMPTION_TAB_RESUMPTION_UTIL_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_TAB_RESUMPTION_TAB_RESUMPTION_UTIL_H_

#include "components/history/core/browser/history_types.h"
#include "components/sync_sessions/synced_session.h"

inline constexpr char kSampleUrl[] = "https://www.google.com";

std::unique_ptr<sync_sessions::SyncedSession> SampleSession(
    const char session_name[],
    const char session_tag[],
    int num_windows,
    int num_tabs);
std::unique_ptr<sync_sessions::SyncedSessionWindow> SampleSessionWindow(
    int num_tabs);
std::unique_ptr<sessions::SessionTab> SampleSessionTab(int tab_id);

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_TAB_RESUMPTION_TAB_RESUMPTION_UTIL_H_
