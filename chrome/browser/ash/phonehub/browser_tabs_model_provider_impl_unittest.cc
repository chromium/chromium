// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/phonehub/browser_tabs_model_provider_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/phonehub/fake_browser_tabs_metadata_fetcher.h"
#include "chromeos/ash/components/phonehub/mutable_phone_model.h"
#include "chromeos/ash/components/phonehub/phone_model_test_util.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/crosapi/mojom/synced_session_client.mojom.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/sync/base/features.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace phonehub {

namespace {

using ::testing::_;

constexpr char kPhoneNameOne[] = "Pixel";
constexpr char kPhoneNameTwo[] = "Galaxy";

class SessionSyncServiceMock : public sync_sessions::SessionSyncService {
 public:
  SessionSyncServiceMock() {}
  ~SessionSyncServiceMock() override {}

  MOCK_CONST_METHOD0(GetGlobalIdMapper, syncer::GlobalIdMapper*());
  MOCK_METHOD0(GetOpenTabsUIDelegate, sync_sessions::OpenTabsUIDelegate*());
  MOCK_METHOD1(
      SubscribeToForeignSessionsChanged,
      base::CallbackListSubscription(const base::RepeatingClosure& cb));
  MOCK_METHOD0(ScheduleGarbageCollection, void());
  MOCK_METHOD0(GetControllerDelegate,
               base::WeakPtr<syncer::DataTypeControllerDelegate>());
};

class OpenTabsUIDelegateMock : public sync_sessions::OpenTabsUIDelegate {
 public:
  OpenTabsUIDelegateMock() {}
  ~OpenTabsUIDelegateMock() override {}

  MOCK_METHOD1(GetAllForeignSessions,
               bool(std::vector<raw_ptr<const sync_sessions::SyncedSession,
                                        VectorExperimental>>* sessions));
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
               bool(const sync_sessions::SyncedSession** local));
};

multidevice::RemoteDeviceRef CreatePhoneDevice(const std::string& pii_name) {
  multidevice::RemoteDeviceRefBuilder builder;
  builder.SetPiiFreeName(pii_name);
  return builder.Build();
}

std::unique_ptr<sync_sessions::SyncedSession> CreateNewSession(
    const std::string& session_name,
    const base::Time& session_time = base::Time::FromSecondsSinceUnixEpoch(0)) {
  auto session = std::make_unique<sync_sessions::SyncedSession>();
  session->SetSessionName(session_name);
  session->SetModifiedTime(session_time);
  return session;
}

}  // namespace

class BrowserTabsModelProviderImplTest
    : public testing::Test,
      public BrowserTabsModelProvider::Observer {
 public:
  BrowserTabsModelProviderImplTest() = default;

  BrowserTabsModelProviderImplTest(const BrowserTabsModelProviderImplTest&) =
      delete;
  BrowserTabsModelProviderImplTest& operator=(
      const BrowserTabsModelProviderImplTest&) = delete;
  ~BrowserTabsModelProviderImplTest() override = default;

  // BrowserTabsModelProvider::Observer:
  void OnBrowserTabsUpdated(
      bool is_sync_enabled,
      const std::vector<BrowserTabsModel::BrowserTabMetadata>&
          browser_tabs_metadata) override {
    phone_model_.SetBrowserTabsModel(BrowserTabsModel(
        /*is_tab_sync_enabled=*/is_sync_enabled, browser_tabs_metadata));
  }

  // testing::Test:
  void SetUp() override {
    ON_CALL(mock_session_sync_service_, GetOpenTabsUIDelegate())
        .WillByDefault(Invoke(
            this, &BrowserTabsModelProviderImplTest::open_tabs_ui_delegate));

    ON_CALL(open_tabs_ui_delegate_, GetAllForeignSessions(_))
        .WillByDefault(Invoke(
            this,
            &BrowserTabsModelProviderImplTest::MockGetAllForeignSessions));

    ON_CALL(mock_session_sync_service_, SubscribeToForeignSessionsChanged(_))
        .WillByDefault(Invoke(this, &BrowserTabsModelProviderImplTest::
                                        MockSubscribeToForeignSessionsChanged));
  }

  void CreateProvider() {
    provider_ = std::make_unique<BrowserTabsModelProviderImpl>(
        &fake_multidevice_setup_client_, &mock_sync_service_,
        &mock_session_sync_service_,
        std::make_unique<FakeBrowserTabsMetadataFetcher>());
    provider_->AddObserver(this);
  }

  void SetPiiFreeName(const std::string& pii_free_name) {
    fake_multidevice_setup_client_.SetHostStatusWithDevice(std::make_pair(
        multidevice_setup::mojom::HostStatus::kEligibleHostExistsButNoHostSet,
        CreatePhoneDevice(/*pii_name=*/pii_free_name)));
  }

  base::CallbackListSubscription MockSubscribeToForeignSessionsChanged(
      const base::RepeatingClosure& cb) {
    foreign_sessions_changed_callback_ = std::move(cb);
    return {};
  }

  bool MockGetAllForeignSessions(
      std::vector<raw_ptr<const sync_sessions::SyncedSession,
                          VectorExperimental>>* sessions) {
    if (sessions_) {
      *sessions = *sessions_;
      return !sessions->empty();
    }
    return false;
  }

  testing::NiceMock<OpenTabsUIDelegateMock>* open_tabs_ui_delegate() {
    return enable_tab_sync_ ? &open_tabs_ui_delegate_ : nullptr;
  }

  void NotifySubscription() { foreign_sessions_changed_callback_.Run(); }

  void OnForeignSyncedPhoneSessionsUpdated(
      std::vector<crosapi::mojom::SyncedSessionPtr> sessions) {
    NOTREACHED();
  }

  void OnSessionSyncEnabledChanged(bool enabled) { NOTREACHED(); }

  void set_synced_sessions(
      std::vector<raw_ptr<const sync_sessions::SyncedSession,
                          VectorExperimental>>* sessions) {
    sessions_ = sessions;
  }

  void set_enable_tab_sync(bool is_enabled) { enable_tab_sync_ = is_enabled; }

  FakeBrowserTabsMetadataFetcher* fake_browser_tabs_metadata_fetcher() {
    return static_cast<FakeBrowserTabsMetadataFetcher*>(
        provider_->browser_tabs_metadata_fetcher_.get());
  }

  MutablePhoneModel phone_model_;
  multidevice_setup::FakeMultiDeviceSetupClient fake_multidevice_setup_client_;

  testing::NiceMock<syncer::MockSyncService> mock_sync_service_;
  testing::NiceMock<SessionSyncServiceMock> mock_session_sync_service_;
  std::unique_ptr<BrowserTabsModelProviderImpl> provider_;

  testing::NiceMock<OpenTabsUIDelegateMock> open_tabs_ui_delegate_;

  bool enable_tab_sync_ = true;
  raw_ptr<std::vector<
      raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>>
      sessions_ = nullptr;
  base::RepeatingClosure foreign_sessions_changed_callback_;
};

TEST_F(BrowserTabsModelProviderImplTest, AttemptBrowserTabsModelUpdate) {
  CreateProvider();

  // Test no Pii Free name despite sync being enabled.
  set_enable_tab_sync(true);
  set_synced_sessions(nullptr);
  NotifySubscription();
  EXPECT_FALSE(phone_model_.browser_tabs_model()->is_tab_sync_enabled());
  EXPECT_TRUE(phone_model_.browser_tabs_model()->most_recent_tabs().empty());
  EXPECT_FALSE(
      fake_browser_tabs_metadata_fetcher()->DoesPendingCallbackExist());

  // Set name of phone. Tests that OnHostStatusChanged causes name change.
  SetPiiFreeName(kPhoneNameOne);

  // Test disabling tab sync with no browser tab metadata.
  set_enable_tab_sync(false);
  set_synced_sessions(nullptr);
  NotifySubscription();
  EXPECT_FALSE(phone_model_.browser_tabs_model()->is_tab_sync_enabled());
  EXPECT_TRUE(phone_model_.browser_tabs_model()->most_recent_tabs().empty());
  EXPECT_FALSE(
      fake_browser_tabs_metadata_fetcher()->DoesPendingCallbackExist());

  // Test enabling tab sync with no browser tab metadata.
  set_enable_tab_sync(true);
  set_synced_sessions(nullptr);
  NotifySubscription();
  EXPECT_TRUE(phone_model_.browser_tabs_model()->is_tab_sync_enabled());
  EXPECT_TRUE(phone_model_.browser_tabs_model()->most_recent_tabs().empty());
  EXPECT_FALSE(
      fake_browser_tabs_metadata_fetcher()->DoesPendingCallbackExist());

  // Test enabling tab sync with no matching pii name with session_name.
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions;
  std::unique_ptr<sync_sessions::SyncedSession> session =
      CreateNewSession(kPhoneNameTwo);
  sessions.emplace_back(session.get());
  set_enable_tab_sync(true);
  set_synced_sessions(&sessions);
  NotifySubscription();
  EXPECT_TRUE(phone_model_.browser_tabs_model()->is_tab_sync_enabled());
  EXPECT_TRUE(phone_model_.browser_tabs_model()->most_recent_tabs().empty());
  EXPECT_FALSE(
      fake_browser_tabs_metadata_fetcher()->DoesPendingCallbackExist());

  // Test enabling tab sync with matching pii name with session_name, which
  // will cause the |fake_browser_tabs_metadata_fetcher()| to have a pending
  // callback.
  std::unique_ptr<sync_sessions::SyncedSession> new_session =
      CreateNewSession(kPhoneNameOne);
  sessions.emplace_back(new_session.get());
  set_enable_tab_sync(true);
  set_synced_sessions(&sessions);
  NotifySubscription();
  EXPECT_TRUE(phone_model_.browser_tabs_model()->is_tab_sync_enabled());
  EXPECT_TRUE(phone_model_.browser_tabs_model()->most_recent_tabs().empty());
  EXPECT_TRUE(fake_browser_tabs_metadata_fetcher()->DoesPendingCallbackExist());

  // Test that once |fake_browser_tabs_metadata_fetcher()| responds, the phone
  // model will be appropriately updated.
  std::vector<BrowserTabsModel::BrowserTabMetadata> metadata;
  metadata.push_back(CreateFakeBrowserTabMetadata());
  fake_browser_tabs_metadata_fetcher()->RespondToCurrentFetchAttempt(
      std::move(metadata));
  EXPECT_EQ(phone_model_.browser_tabs_model()->most_recent_tabs().size(), 1U);
}

TEST_F(BrowserTabsModelProviderImplTest, ClearTabMetadataDuringMetadataFetch) {
  CreateProvider();
  SetPiiFreeName(kPhoneNameOne);
  std::unique_ptr<sync_sessions::SyncedSession> new_session =
      CreateNewSession(kPhoneNameOne);
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions({new_session.get()});

  set_enable_tab_sync(true);
  set_synced_sessions(&sessions);
  NotifySubscription();
  EXPECT_TRUE(fake_browser_tabs_metadata_fetcher()->DoesPendingCallbackExist());

  // Set to no synced sessions. Tab sync is still enabled.
  set_synced_sessions(nullptr);
  NotifySubscription();

  // Test that if the Browser tab metadata is cleared while a browser tab
  // metadata fetch is in progress, the in progress callback will be cancelled.
  std::vector<BrowserTabsModel::BrowserTabMetadata> metadata;
  metadata.push_back(CreateFakeBrowserTabMetadata());
  fake_browser_tabs_metadata_fetcher()->RespondToCurrentFetchAttempt(
      std::move(metadata));
  EXPECT_TRUE(phone_model_.browser_tabs_model()->most_recent_tabs().empty());
}

TEST_F(BrowserTabsModelProviderImplTest, SessionCorrectlySelected) {
  CreateProvider();
  SetPiiFreeName(kPhoneNameOne);
  std::unique_ptr<sync_sessions::SyncedSession> session_a =
      CreateNewSession(kPhoneNameOne, base::Time::FromSecondsSinceUnixEpoch(1));
  std::unique_ptr<sync_sessions::SyncedSession> session_b =
      CreateNewSession(kPhoneNameOne, base::Time::FromSecondsSinceUnixEpoch(3));
  std::unique_ptr<sync_sessions::SyncedSession> session_c =
      CreateNewSession(kPhoneNameOne, base::Time::FromSecondsSinceUnixEpoch(2));
  std::unique_ptr<sync_sessions::SyncedSession> session_d = CreateNewSession(
      kPhoneNameTwo, base::Time::FromSecondsSinceUnixEpoch(10));

  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions(
          {session_a.get(), session_b.get(), session_c.get(), session_d.get()});

  set_enable_tab_sync(true);
  set_synced_sessions(&sessions);
  NotifySubscription();
  EXPECT_TRUE(fake_browser_tabs_metadata_fetcher()->DoesPendingCallbackExist());

  // |session_b| should be the selected session because it is the has the same
  // session_name as the set phone name and the latest modified time.
  EXPECT_EQ(fake_browser_tabs_metadata_fetcher()->GetSession(),
            session_b.get());
}

}  // namespace phonehub
}  // namespace ash
