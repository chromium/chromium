// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/tab_resumption/tab_resumption_test_support.h"

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sync_sessions/session_sync_service.h"
#include "url/gurl.h"

MockOpenTabsUIDelegate::MockOpenTabsUIDelegate() = default;

MockOpenTabsUIDelegate::~MockOpenTabsUIDelegate() = default;

MockSessionSyncService::MockSessionSyncService() = default;

MockSessionSyncService::~MockSessionSyncService() = default;

syncer::GlobalIdMapper* MockSessionSyncService::GetGlobalIdMapper() const {
  return nullptr;
}

MockOpenTabsUIDelegate* MockSessionSyncService::GetOpenTabsUIDelegate() {
  return &mock_open_tabs_ui_delegate_;
}

base::CallbackListSubscription
MockSessionSyncService::SubscribeToForeignSessionsChanged(
    const base::RepeatingClosure& cb) {
  return subscriber_list_.Add(cb);
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
MockSessionSyncService::GetControllerDelegate() {
  return nullptr;
}

sync_sessions::SyncedSession* SampleSession(const char session_tag[],
                                            int num_windows) {
  auto* sample_session = new sync_sessions::SyncedSession();
  for (int i = 0; i < num_windows; i++) {
    sample_session->windows[SessionID::FromSerializedValue(i)] =
        SampleSessionWindow(3);
  }

  constexpr char session_name[] = "Test Name";

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

  auto navigation =
      sessions::SerializedNavigationEntryTestHelper::CreateNavigationForTest();
  navigation.set_title(u"Test");
  navigation.set_virtual_url(GURL(kSampleUrl));
  navigation.set_timestamp(base::Time::Now());
  navigation.set_favicon_url(GURL(kSampleUrl));
  session_tab->navigations.push_back(navigation);

  session_tab->timestamp = base::Time::Now();
  session_tab->tab_id = SessionID::FromSerializedValue(tab_id);

  return session_tab;
}
