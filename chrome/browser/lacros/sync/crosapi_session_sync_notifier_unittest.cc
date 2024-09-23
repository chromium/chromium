// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/sync/crosapi_session_sync_notifier.h"

#include <string_view>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "chromeos/crosapi/mojom/synced_session_client.mojom.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sync/test/fake_synced_session_client_ash.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_sessions/mock_sync_sessions_client.h"
#include "components/sync_sessions/open_tabs_ui_delegate_impl.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "components/sync_sessions/synced_session_tracker.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;
using testing::Return;

constexpr char kSessionTag1[] = "foreign1";
constexpr char kSessionTag2[] = "foreign2";
constexpr char kSessionTag3[] = "foreign3";
constexpr SessionID kWindowId1 = SessionID::FromSerializedValue(1);
constexpr SessionID kWindowId2 = SessionID::FromSerializedValue(2);
constexpr SessionID kWindowId3 = SessionID::FromSerializedValue(3);
constexpr SessionID kTabId1 = SessionID::FromSerializedValue(111);
constexpr SessionID kTabId2 = SessionID::FromSerializedValue(222);
constexpr SessionID kTabId3 = SessionID::FromSerializedValue(333);

// TODO(b/272291842): use a FakeSessionSyncService in place of this mock.
class MockSessionSyncService : public sync_sessions::SessionSyncService {
 public:
  MockSessionSyncService() = default;
  ~MockSessionSyncService() override = default;

  MOCK_METHOD(syncer::GlobalIdMapper*,
              GetGlobalIdMapper,
              (),
              (const, override));
  MOCK_METHOD(sync_sessions::OpenTabsUIDelegate*,
              GetOpenTabsUIDelegate,
              (),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              SubscribeToForeignSessionsChanged,
              (const base::RepeatingClosure& cb),
              (override));
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetControllerDelegate,
              ());
};

}  // namespace

class CrosapiSessionSyncNotifierTest : public testing::Test {
 public:
  CrosapiSessionSyncNotifierTest()
      : synced_session_tracker_(&mock_sync_sessions_client_),
        open_tabs_ui_delegate_(
            &mock_sync_sessions_client_,
            &synced_session_tracker_,
            base::BindRepeating(
                &CrosapiSessionSyncNotifierTest::DeleteForeignSessionCallback,
                base::Unretained(this))) {}

  void SetUp() override {
    ON_CALL(mock_session_sync_service_, SubscribeToForeignSessionsChanged(_))
        .WillByDefault(Invoke(this, &CrosapiSessionSyncNotifierTest::
                                        SubscribeToForeignSessionsChanged));

    ON_CALL(mock_session_sync_service_, GetOpenTabsUIDelegate())
        .WillByDefault(Invoke(
            this, &CrosapiSessionSyncNotifierTest::GetOpenTabsUIDelegate));

    // All SessionTabs are considered to have interesting URLs. Allows
    // `SyncedSessionTracker::LookupAllForeignSessions()` to be passed
    // `SessionLookup::PRESENTABLE` in
    // `MockOpenTabsUIDelegate::GetAllForeignSessions()` just like it is in
    // `OpenTabsUIDelegate::GetAllForeignSessions()`.
    ON_CALL(mock_sync_sessions_client_, ShouldSyncURL(_))
        .WillByDefault(Return(true));

    test_sync_service_ = std::make_unique<syncer::TestSyncService>();
    test_sync_service_->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, /*types=*/{});

    // Create object under test.
    crosapi_session_sync_notifier_ =
        std::make_unique<CrosapiSessionSyncNotifier>(
            &mock_session_sync_service_,
            fake_synced_session_client_ash_.CreateRemote(),
            test_sync_service_.get(), /*favicon_request_handler=*/nullptr);
  }

  base::CallbackListSubscription SubscribeToForeignSessionsChanged(
      const base::RepeatingClosure& cb) {
    foreign_sessions_changed_callback_ = cb;
    return {};
  }

  sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate() {
    return &open_tabs_ui_delegate_;
  }

  bool GetAllForeignSessions(
      std::vector<raw_ptr<const sync_sessions::SyncedSession,
                          VectorExperimental>>* sessions) {
    foreign_sessions_ = synced_session_tracker_.LookupAllForeignSessions(
        sync_sessions::SyncedSessionTracker::SessionLookup::PRESENTABLE);
    *sessions = foreign_sessions_;
    return !sessions->empty();
  }

  void NotifyForeignSessionsChanged() {
    foreign_sessions_changed_callback_.Run();
  }

  // Creates a new SessionTab with id `tab_id` inside of a SessionWindow with id
  // `window_id` inside of a SyncedSession with tag `session_tag`. If a
  // SessionTab with id `tab_id` already exists within a SyncedSession with tag
  // `session_tag` and SessionWindow with id `window_id`, this function returns
  // false, and no new objects are created. Otherwise, the function returns
  // true. If no SyncedSession and/or SessionWindow exist with their respective
  // tag/id, then a new one is created in order to create the new SessionTag.
  // All SyncedSessions will have device form factor `kPhone`. All SessionTabs
  // will have valid urls with https schemes.
  bool CreateForeignPhonePresentableTabInSession(
      const std::string_view& session_tag,
      const SessionID window_id,
      const SessionID tab_id) {
    sync_sessions::SyncedSession* session =
        synced_session_tracker_.GetSession(session_tag.data());
    const sessions::SessionTab* tab =
        synced_session_tracker_.LookupSessionTab(session_tag.data(), tab_id);

    // If a SessionTab with id `tab_id` exists within a SessionWindow with id
    // `window_id`, a duplicate is not created.
    if (tab && window_id == tab->window_id) {
      return false;
    }
    CreateForeignPhonePresentableTabInWindow(session_tag, window_id, tab_id);
    session->SetDeviceTypeAndFormFactor(
        sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
        syncer::DeviceInfo::FormFactor::kPhone);
    return true;
  }

  // Checks that all tab, window, and session information sent from the
  // `CrosapiSessionSyncNotifier` to the `FakeSyncedSessionClient` was received
  // exactly as sent, even if the sent message was empty.
  void ValidateSentSessions() {
    const std::vector<raw_ptr<const sync_sessions::SyncedSession,
                              VectorExperimental>>& sent_sessions =
        synced_session_tracker_.LookupAllForeignSessions(
            sync_sessions::SyncedSessionTracker::SessionLookup::PRESENTABLE);
    const std::vector<crosapi::mojom::SyncedSessionPtr>& received_sessions =
        fake_synced_session_client_ash_.LookupForeignSyncedPhoneSessions();
    ASSERT_EQ(sent_sessions.size(), received_sessions.size());
    for (size_t idx = 0; idx < sent_sessions.size(); idx++) {
      const sync_sessions::SyncedSession& sent_session = *sent_sessions[idx];
      const crosapi::mojom::SyncedSession* received_session =
          received_sessions[idx].get();
      EXPECT_EQ(sent_session.GetSessionName(), received_session->session_name);
      EXPECT_EQ(sent_session.GetModifiedTime(),
                received_session->modified_time);
      ValidateSentWindows(sent_session.GetSessionTag(),
                          received_session->windows);
    }
  }

  void SetPhoneSessionsUpdatedCallback(base::RepeatingClosure callback) {
    fake_synced_session_client_ash_
        .SetOnForeignSyncedPhoneSessionsUpdatedCallback(std::move(callback));
  }

  void SetDeleteForeignSessionCallback(const base::RepeatingClosure& cb) {
    delete_foreign_session_callback_ = cb;
  }

  syncer::TestSyncService* test_sync_service() {
    return test_sync_service_.get();
  }

  syncer::FakeSyncedSessionClientAsh* fake_synced_session_client_ash() {
    return &fake_synced_session_client_ash_;
  }

  CrosapiSessionSyncNotifier* crosapi_session_sync_notifier() {
    return crosapi_session_sync_notifier_.get();
  }

 private:
  // Helper to `CreateForeignPhonePresentableTabInSession()`, keeps all promises
  // made by that function. Finds the SessionWindow with id `window_id` to
  // create a SessionTab in. If none exists, it is created.
  void CreateForeignPhonePresentableTabInWindow(
      const std::string_view& session_tag,
      const SessionID window_id,
      const SessionID tab_id) {
    std::vector<const sessions::SessionWindow*> windows =
        synced_session_tracker_.LookupSessionWindows(session_tag.data());
    for (const sessions::SessionWindow* window : windows) {
      if (window_id == window->window_id) {
        // This can be done without checking for tab existence in the window
        // because the tab's existence is checked in the session in
        // `CreateForeignPhonePresentableTabInSession()`.
        CreateForeignPhonePresentableTab(session_tag.data(), window_id, tab_id);
        return;
      }
    }

    // No SessionWindow with tag `window_id` was found in SyncedSession with tag
    // `session_tag`, so one is created.
    synced_session_tracker_.PutWindowInSession(session_tag.data(), window_id);
    CreateForeignPhonePresentableTab(session_tag, window_id, tab_id);
  }

  // Helper to `CreateForeignPhonePresentableTabInSession()`, keeps all promises
  // made by that function. Creates a new SessionTab with id `tab_id`.
  void CreateForeignPhonePresentableTab(const std::string_view& session_tag,
                                        const SessionID window_id,
                                        const SessionID tab_id) {
    // This can be done without checking for tab existence in the window because
    // the tab's existence is checked in the session in
    // `CreateForeignPhonePresentableTabInSession()`.
    synced_session_tracker_.PutTabInWindow(session_tag.data(), window_id,
                                           tab_id);
    sessions::SessionTab* tab =
        synced_session_tracker_.GetTab(session_tag.data(), tab_id);
    tab->navigations.push_back(sessions::SerializedNavigationEntryTestHelper::
                                   CreateNavigationForTest());
    tab->timestamp = base::Time::Now();
  }

  // Helper to ValidateSentSessions. Validates the members of each window in the
  // session with tag `sent_session_tag` against the members of each window in
  // `received_windows`.
  void ValidateSentWindows(
      const std::string& sent_session_tag,
      const std::vector<crosapi::mojom::SyncedSessionWindowPtr>&
          received_windows) {
    std::vector<const sessions::SessionWindow*> sent_windows =
        synced_session_tracker_.LookupSessionWindows(sent_session_tag);
    EXPECT_EQ(sent_windows.empty(), received_windows.empty());
    if (sent_windows.empty()) {
      return;
    }

    ASSERT_EQ(sent_windows.size(), received_windows.size());
    for (size_t idx = 0; idx < sent_windows.size(); idx++) {
      ValidateSentTabs(sent_windows[idx]->tabs, received_windows[idx]->tabs);
    }
  }

  // Helper to ValidateSentSessions. Validates the members of each tab in
  // `sent_tabs` against the members of each tab in `received_tabs`.
  void ValidateSentTabs(
      const std::vector<std::unique_ptr<sessions::SessionTab>>& sent_tabs,
      const std::vector<crosapi::mojom::SyncedSessionTabPtr>& received_tabs) {
    ASSERT_EQ(sent_tabs.size(), received_tabs.size());
    for (size_t idx = 0; idx < sent_tabs.size(); idx++) {
      // Get sent SessionTab, check that the tab sent is valid to show via
      // PhoneHub.
      const sessions::SessionTab& sent_tab = *sent_tabs[idx];
      const int sent_selected_index = sent_tab.normalized_navigation_index();
      const sessions::SerializedNavigationEntry& sent_navigation =
          sent_tab.navigations[sent_selected_index];
      const GURL& sent_tab_url = sent_navigation.virtual_url();

      const crosapi::mojom::SyncedSessionTab* received_tab =
          received_tabs[idx].get();

      EXPECT_EQ(sent_tab_url, received_tab->current_navigation_url);
      EXPECT_EQ(sent_navigation.title(),
                received_tab->current_navigation_title);
      EXPECT_EQ(sent_tab.timestamp, received_tab->last_modified_timestamp);
    }
  }

  void DeleteForeignSessionCallback(const std::string& tag) {
    delete_foreign_session_callback_.Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<syncer::TestSyncService> test_sync_service_;
  std::unique_ptr<CrosapiSessionSyncNotifier> crosapi_session_sync_notifier_;
  syncer::FakeSyncedSessionClientAsh fake_synced_session_client_ash_;

  testing::NiceMock<sync_sessions::MockSyncSessionsClient>
      mock_sync_sessions_client_;
  sync_sessions::SyncedSessionTracker synced_session_tracker_;
  base::RepeatingClosure delete_foreign_session_callback_;
  sync_sessions::OpenTabsUIDelegateImpl open_tabs_ui_delegate_;
  testing::NiceMock<MockSessionSyncService> mock_session_sync_service_;
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      foreign_sessions_;
  base::RepeatingClosure foreign_sessions_changed_callback_;
};

TEST_F(CrosapiSessionSyncNotifierTest,
       OnForeignSyncedPhoneSessionsUpdated_OneTab) {
  base::RunLoop run_loop;
  SetPhoneSessionsUpdatedCallback(run_loop.QuitClosure());

  EXPECT_TRUE(CreateForeignPhonePresentableTabInSession(kSessionTag1,
                                                        kWindowId1, kTabId1));
  NotifyForeignSessionsChanged();
  run_loop.Run();
  ValidateSentSessions();
}

TEST_F(CrosapiSessionSyncNotifierTest,
       OnForeignSyncedPhoneSessionsUpdated_OneSession_MultipleTabs) {
  base::RunLoop run_loop;
  SetPhoneSessionsUpdatedCallback(run_loop.QuitClosure());

  EXPECT_TRUE(CreateForeignPhonePresentableTabInSession(kSessionTag1,
                                                        kWindowId1, kTabId1));
  EXPECT_TRUE(CreateForeignPhonePresentableTabInSession(kSessionTag1,
                                                        kWindowId1, kTabId2));
  EXPECT_TRUE(CreateForeignPhonePresentableTabInSession(kSessionTag1,
                                                        kWindowId1, kTabId3));
  NotifyForeignSessionsChanged();
  run_loop.Run();
  ValidateSentSessions();
}

TEST_F(CrosapiSessionSyncNotifierTest,
       OnForeignSyncedPhoneSessionsUpdated_MultipleSessions_MultipleTabs) {
  base::RunLoop run_loop;
  SetPhoneSessionsUpdatedCallback(run_loop.QuitClosure());

  EXPECT_TRUE(CreateForeignPhonePresentableTabInSession(kSessionTag1,
                                                        kWindowId1, kTabId1));
  EXPECT_TRUE(CreateForeignPhonePresentableTabInSession(kSessionTag1,
                                                        kWindowId2, kTabId2));
  EXPECT_TRUE(CreateForeignPhonePresentableTabInSession(kSessionTag1,
                                                        kWindowId3, kTabId3));

  EXPECT_TRUE(CreateForeignPhonePresentableTabInSession(kSessionTag2,
                                                        kWindowId1, kTabId1));
  EXPECT_TRUE(CreateForeignPhonePresentableTabInSession(kSessionTag2,
                                                        kWindowId2, kTabId2));
  EXPECT_TRUE(CreateForeignPhonePresentableTabInSession(kSessionTag2,
                                                        kWindowId3, kTabId3));

  EXPECT_TRUE(CreateForeignPhonePresentableTabInSession(kSessionTag3,
                                                        kWindowId1, kTabId1));
  EXPECT_TRUE(CreateForeignPhonePresentableTabInSession(kSessionTag3,
                                                        kWindowId2, kTabId2));
  EXPECT_TRUE(CreateForeignPhonePresentableTabInSession(kSessionTag3,
                                                        kWindowId3, kTabId3));
  NotifyForeignSessionsChanged();
  run_loop.Run();
  ValidateSentSessions();
}

TEST_F(CrosapiSessionSyncNotifierTest,
       OnForeignSyncedPhoneSessionsUpdated_NoSessions) {
  base::RunLoop run_loop;
  SetPhoneSessionsUpdatedCallback(run_loop.QuitClosure());

  NotifyForeignSessionsChanged();
  run_loop.Run();
  ValidateSentSessions();
}

TEST_F(CrosapiSessionSyncNotifierTest, SyncServiceObserverAdded) {
  EXPECT_TRUE(
      test_sync_service()->HasObserver(crosapi_session_sync_notifier()));
}

TEST_F(CrosapiSessionSyncNotifierTest, OnStateChanged_NoChangeToStartingValue) {
  // OnStateChanged() is called from the CrosapiSessionSyncNotifier constructor.
  // The "tab sync enabled" value should remain |false|
  test_sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, /*types=*/{});
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(fake_synced_session_client_ash()->is_session_sync_enabled());
}

TEST_F(CrosapiSessionSyncNotifierTest,
       OnStateChanged_TabSyncEnabledStateChanged) {
  // OnStateChange() is called if the "tab sync enabled" value changes
  test_sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/true, /*types=*/{});
  test_sync_service()->FireStateChanged();
  fake_synced_session_client_ash()->FlushMojoForTesting();
  EXPECT_TRUE(fake_synced_session_client_ash()->is_session_sync_enabled());
}
