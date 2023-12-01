// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/tab_resumption/tab_resumption_util.h"

#include "components/history/core/browser/history_types.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/synced_session.h"

std::unique_ptr<sync_sessions::SyncedSession> SampleSession(
    const char session_name[],
    const char session_tag[],
    int num_windows,
    int num_tabs) {
  auto sample_session = std::make_unique<sync_sessions::SyncedSession>();
  for (int i = 0; i < num_windows; i++) {
    sample_session->windows[SessionID::FromSerializedValue(i)] =
        SampleSessionWindow(num_tabs);
  }

  sample_session->SetSessionTag(session_tag);
  sample_session->SetSessionName(session_name);
  sample_session->SetModifiedTime(base::Time::Now());

  return sample_session;
}

std::unique_ptr<sync_sessions::SyncedSessionWindow> SampleSessionWindow(
    int num_tabs) {
  auto synced_session_window =
      std::make_unique<sync_sessions::SyncedSessionWindow>();
  synced_session_window->wrapped_window.timestamp = base::Time::Now();
  for (int i = 0; i < num_tabs; i++) {
    synced_session_window->wrapped_window.tabs.push_back(SampleSessionTab(i));
  }
  return synced_session_window;
}

std::unique_ptr<sessions::SessionTab> SampleSessionTab(int tab_id) {
  auto session_tab = std::make_unique<sessions::SessionTab>();
  session_tab->current_navigation_index = 0;

  sessions::SerializedNavigationEntry navigation;
  navigation.set_title(u"Test");
  navigation.set_virtual_url(GURL(kSampleUrl));
  navigation.set_timestamp(base::Time::Now());
  navigation.set_favicon_url(GURL(kSampleUrl));
  session_tab->navigations.push_back(navigation);

  session_tab->timestamp = base::Time::Now();
  session_tab->tab_id = SessionID::FromSerializedValue(tab_id);

  return session_tab;
}
