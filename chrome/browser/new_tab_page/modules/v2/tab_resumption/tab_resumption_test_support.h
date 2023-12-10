// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_TAB_RESUMPTION_TAB_RESUMPTION_TEST_SUPPORT_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_TAB_RESUMPTION_TAB_RESUMPTION_TEST_SUPPORT_H_

#include <string>
#include <vector>

#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockOpenTabsUIDelegate : public sync_sessions::OpenTabsUIDelegate {
 public:
  MockOpenTabsUIDelegate();
  MockOpenTabsUIDelegate(const MockOpenTabsUIDelegate&) = delete;
  MockOpenTabsUIDelegate& operator=(const MockOpenTabsUIDelegate&) = delete;
  ~MockOpenTabsUIDelegate() override;

  MOCK_METHOD1(
      GetAllForeignSessions,
      bool(std::vector<const sync_sessions::SyncedSession*>* sessions));

  MOCK_METHOD3(GetForeignTab,
               bool(const std::string& tag,
                    const SessionID tab_id,
                    const sessions::SessionTab** tab));

  MOCK_METHOD1(DeleteForeignSession, void(const std::string& tag));

  MOCK_METHOD1(
      GetForeignSession,
      std::vector<const sessions::SessionWindow*>(const std::string& tag));

  MOCK_METHOD2(GetForeignSessionTabs,
               bool(const std::string& tag,
                    std::vector<const sessions::SessionTab*>* tabs));

  MOCK_METHOD1(GetLocalSession,
               bool(const sync_sessions::SyncedSession** local_session));
};

class MockSessionSyncService : public sync_sessions::SessionSyncService {
 public:
  MockSessionSyncService();
  MockSessionSyncService(const MockSessionSyncService&) = delete;
  MockSessionSyncService& operator=(const MockSessionSyncService&) = delete;
  ~MockSessionSyncService() override;

  // SessionSyncService overrides.
  syncer::GlobalIdMapper* GetGlobalIdMapper() const override;

  MockOpenTabsUIDelegate* GetOpenTabsUIDelegate() override;

  base::CallbackListSubscription SubscribeToForeignSessionsChanged(
      const base::RepeatingClosure& cb) override;

  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetControllerDelegate()
      override;

 private:
  base::RepeatingClosureList subscriber_list_;
  MockOpenTabsUIDelegate mock_open_tabs_ui_delegate_;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_TAB_RESUMPTION_TAB_RESUMPTION_TEST_SUPPORT_H_
