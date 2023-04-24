// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/phonehub/browser_tabs_model_provider_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/sync/synced_session_client_ash.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/phonehub/fake_browser_tabs_metadata_fetcher.h"
#include "chromeos/ash/components/phonehub/mutable_phone_model.h"
#include "chromeos/ash/components/phonehub/phone_model_test_util.h"
#include "chromeos/ash/components/standalone_browser/lacros_availability.h"
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
constexpr char kManagedUserEmail[] = "user@managedchrome.com";
constexpr base::Time kRecentSessionTime = base::Time::FromTimeT(100);
constexpr base::Time kOlderSessionTime = base::Time::FromTimeT(10);

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
               base::WeakPtr<syncer::ModelTypeControllerDelegate>());
  MOCK_METHOD1(ProxyTabsStateChanged,
               void(syncer::DataTypeController::State state));
};

class OpenTabsUIDelegateMock : public sync_sessions::OpenTabsUIDelegate {
 public:
  OpenTabsUIDelegateMock() {}
  ~OpenTabsUIDelegateMock() override {}

  MOCK_METHOD1(
      GetAllForeignSessions,
      bool(std::vector<const sync_sessions::SyncedSession*>* sessions));
  MOCK_METHOD3(GetForeignTab,
               bool(const std::string& tag,
                    const SessionID tab_id,
                    const sessions::SessionTab** tab));
  MOCK_METHOD1(DeleteForeignSession, void(const std::string& tag));
  MOCK_METHOD2(GetForeignSession,
               bool(const std::string& tag,
                    std::vector<const sessions::SessionWindow*>* windows));
  MOCK_METHOD2(GetForeignSessionTabs,
               bool(const std::string& tag,
                    std::vector<const sessions::SessionTab*>* tabs));
  MOCK_METHOD1(GetLocalSession,
               bool(const sync_sessions::SyncedSession** local));
};

// This class wraps the setup of a Lacros Only environment and makes it easy to
// reset the state after use by destroying the handle.
class ScopedLacrosOnlyHandle {
 public:
  ScopedLacrosOnlyHandle() {
    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    fake_user_manager_ = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    SetLoggedInUser();
    SetLacrosAvailability();
  }

  ~ScopedLacrosOnlyHandle() { ResetLacrosAvailability(); }

 private:
  void SetLoggedInUser() {
    AccountId account_id = AccountId::FromUserEmail(kManagedUserEmail);
    const user_manager::User* user = fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
  }

  void SetLacrosAvailability() {
    policy::PolicyMap policy;
    policy.Set(policy::key::kLacrosAvailability, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(GetLacrosAvailabilityPolicyName(
                   standalone_browser::LacrosAvailability::kLacrosOnly)),
               /*external_data_fetcher=*/nullptr);
    crosapi::browser_util::CacheLacrosAvailability(policy);
  }

  void ResetLacrosAvailability() {
    crosapi::browser_util::ClearLacrosAvailabilityCacheForTest();
  }

  raw_ptr<ash::FakeChromeUserManager, ExperimentalAsh> fake_user_manager_ =
      nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

multidevice::RemoteDeviceRef CreatePhoneDevice(const std::string& pii_name) {
  multidevice::RemoteDeviceRefBuilder builder;
  builder.SetPiiFreeName(pii_name);
  return builder.Build();
}

std::unique_ptr<sync_sessions::SyncedSession> CreateNewSession(
    const std::string& session_name,
    const base::Time& session_time = base::Time::FromDoubleT(0)) {
  auto session = std::make_unique<sync_sessions::SyncedSession>();
  session->SetSessionName(session_name);
  session->SetModifiedTime(session_time);
  return session;
}

std::vector<crosapi::mojom::SyncedSessionPtr>
CreateTestSyncedSessionsNoMatchingSession() {
  // This is the most recent session for Galaxy session.
  std::vector<crosapi::mojom::SyncedSessionPtr> sessions;
  crosapi::mojom::SyncedSessionPtr session =
      crosapi::mojom::SyncedSession::New();
  session->session_name = kPhoneNameTwo;
  session->modified_time = kRecentSessionTime;
  sessions.push_back(std::move(session));
  return sessions;
}

std::vector<crosapi::mojom::SyncedSessionPtr> CreateTestSyncedSessions() {
  std::vector<crosapi::mojom::SyncedSessionPtr> sessions;

  // This is the most recent session for Pixel session.
  crosapi::mojom::SyncedSessionPtr session1 =
      crosapi::mojom::SyncedSession::New();
  session1->session_name = kPhoneNameOne;
  session1->modified_time = kRecentSessionTime;
  sessions.push_back(std::move(session1));

  // This is the most recent session for Galaxy session.
  crosapi::mojom::SyncedSessionPtr session2 =
      crosapi::mojom::SyncedSession::New();
  session2->session_name = kPhoneNameTwo;
  session2->modified_time = kRecentSessionTime;
  sessions.push_back(std::move(session2));

  // This is an older session for Pixel session.
  crosapi::mojom::SyncedSessionPtr session3 =
      crosapi::mojom::SyncedSession::New();
  session3->session_name = kPhoneNameOne;
  session3->modified_time = kOlderSessionTime;
  sessions.push_back(std::move(session3));

  return sessions;
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
        &fake_multidevice_setup_client_, &synced_session_client_ash_,
        &mock_sync_service_, &mock_session_sync_service_,
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
      std::vector<const sync_sessions::SyncedSession*>* sessions) {
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
    synced_session_client_ash_.OnForeignSyncedPhoneSessionsUpdated(
        std::move(sessions));
  }

  void OnSessionSyncEnabledChanged(bool enabled) {
    synced_session_client_ash_.OnSessionSyncEnabledChanged(enabled);
  }

  void set_synced_sessions(
      std::vector<const sync_sessions::SyncedSession*>* sessions) {
    sessions_ = sessions;
  }

  void set_enable_tab_sync(bool is_enabled) { enable_tab_sync_ = is_enabled; }

  FakeBrowserTabsMetadataFetcher* fake_browser_tabs_metadata_fetcher() {
    return static_cast<FakeBrowserTabsMetadataFetcher*>(
        provider_->browser_tabs_metadata_fetcher_.get());
  }

  MutablePhoneModel phone_model_;
  multidevice_setup::FakeMultiDeviceSetupClient fake_multidevice_setup_client_;
  SyncedSessionClientAsh synced_session_client_ash_;

  testing::NiceMock<syncer::MockSyncService> mock_sync_service_;
  testing::NiceMock<SessionSyncServiceMock> mock_session_sync_service_;
  std::unique_ptr<BrowserTabsModelProviderImpl> provider_;

  testing::NiceMock<OpenTabsUIDelegateMock> open_tabs_ui_delegate_;

  bool enable_tab_sync_ = true;
  raw_ptr<std::vector<const sync_sessions::SyncedSession*>, ExperimentalAsh>
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
  std::vector<const sync_sessions::SyncedSession*> sessions;
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

TEST_F(BrowserTabsModelProviderImplTest, OnForeignSyncedPhoneSessionsUpdated) {
  // Enable "Lacros Only" by setting feature flags and creating a handle which
  // sets the logged-in user and the Lacros availability policy.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{syncer::kChromeOSSyncedSessionSharing,
                            ash::features::kLacrosOnly},
      /*disabled_features=*/{});
  ScopedLacrosOnlyHandle lacros_only_handle;

  CreateProvider();
  OnSessionSyncEnabledChanged(true);

  // Test no Pii Free name.
  OnForeignSyncedPhoneSessionsUpdated({});
  ASSERT_TRUE(phone_model_.browser_tabs_model().has_value());
  EXPECT_FALSE(phone_model_.browser_tabs_model()->is_tab_sync_enabled());
  EXPECT_TRUE(phone_model_.browser_tabs_model()->most_recent_tabs().empty());
  EXPECT_FALSE(
      fake_browser_tabs_metadata_fetcher()->DoesPendingCallbackExist());

  // Set name of phone. Tests that OnHostStatusChanged causes name change.
  SetPiiFreeName(kPhoneNameOne);

  // Test tab sync with no foreign synced sessions updated.
  OnForeignSyncedPhoneSessionsUpdated({});
  ASSERT_TRUE(phone_model_.browser_tabs_model().has_value());
  EXPECT_TRUE(phone_model_.browser_tabs_model()->is_tab_sync_enabled());
  EXPECT_TRUE(phone_model_.browser_tabs_model()->most_recent_tabs().empty());
  EXPECT_FALSE(
      fake_browser_tabs_metadata_fetcher()->DoesPendingCallbackExist());

  // Test tab sync with foreign synced sessions that do not match session.
  OnForeignSyncedPhoneSessionsUpdated(
      CreateTestSyncedSessionsNoMatchingSession());
  ASSERT_TRUE(phone_model_.browser_tabs_model().has_value());
  EXPECT_TRUE(phone_model_.browser_tabs_model()->is_tab_sync_enabled());
  EXPECT_FALSE(
      fake_browser_tabs_metadata_fetcher()->DoesPendingCallbackExist());

  // Test tab sync with foreign synced sessions updated.
  OnForeignSyncedPhoneSessionsUpdated(CreateTestSyncedSessions());
  ASSERT_TRUE(phone_model_.browser_tabs_model().has_value());
  EXPECT_TRUE(phone_model_.browser_tabs_model()->is_tab_sync_enabled());
  EXPECT_TRUE(fake_browser_tabs_metadata_fetcher()->DoesPendingCallbackExist());
  // TODO(b/260599791): Move ForeignSyncedSessionAsh to a directory that can
  // be imported by FakeBrowserTabsMetadataFetcher and verify that the
  // correct session is being passed.
}

TEST_F(BrowserTabsModelProviderImplTest, OnSessionSyncEnabledChanged) {
  // Enable "Lacros Only" by setting feature flags and creating a handle which
  // sets the logged-in user and the Lacros availability policy.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{syncer::kChromeOSSyncedSessionSharing,
                            ash::features::kLacrosOnly},
      /*disabled_features=*/{});
  ScopedLacrosOnlyHandle lacros_only_handle;

  CreateProvider();

  // Set name of phone and cached sessions.
  SetPiiFreeName(kPhoneNameOne);

  // If session sync becomes enabled with no sessions available, metadata
  // fetcher is not invoked.
  OnSessionSyncEnabledChanged(true);
  ASSERT_TRUE(phone_model_.browser_tabs_model().has_value());
  EXPECT_TRUE(phone_model_.browser_tabs_model()->is_tab_sync_enabled());
  EXPECT_FALSE(
      fake_browser_tabs_metadata_fetcher()->DoesPendingCallbackExist());

  // Trigger an update and respond to the fetch request.
  OnForeignSyncedPhoneSessionsUpdated(CreateTestSyncedSessions());
  EXPECT_TRUE(fake_browser_tabs_metadata_fetcher()->DoesPendingCallbackExist());
  std::vector<BrowserTabsModel::BrowserTabMetadata> metadata1;
  metadata1.push_back(CreateFakeBrowserTabMetadata());
  fake_browser_tabs_metadata_fetcher()->RespondToCurrentFetchAttempt(
      std::move(metadata1));
  ASSERT_TRUE(phone_model_.browser_tabs_model().has_value());
  EXPECT_TRUE(phone_model_.browser_tabs_model()->is_tab_sync_enabled());

  // If session sync becomes disabled we should reflect this in the model.
  OnSessionSyncEnabledChanged(false);
  ASSERT_TRUE(phone_model_.browser_tabs_model().has_value());
  EXPECT_FALSE(phone_model_.browser_tabs_model()->is_tab_sync_enabled());
  EXPECT_FALSE(
      fake_browser_tabs_metadata_fetcher()->DoesPendingCallbackExist());

  // If session sync is re-enabled, we should invoke the metadata fetcher.
  OnSessionSyncEnabledChanged(true);
  EXPECT_TRUE(fake_browser_tabs_metadata_fetcher()->DoesPendingCallbackExist());
  std::vector<BrowserTabsModel::BrowserTabMetadata> metadata2;
  metadata2.push_back(CreateFakeBrowserTabMetadata());
  fake_browser_tabs_metadata_fetcher()->RespondToCurrentFetchAttempt(
      std::move(metadata2));
  ASSERT_TRUE(phone_model_.browser_tabs_model().has_value());
  EXPECT_TRUE(phone_model_.browser_tabs_model()->is_tab_sync_enabled());
}

TEST_F(BrowserTabsModelProviderImplTest, ClearTabMetadataDuringMetadataFetch) {
  CreateProvider();
  SetPiiFreeName(kPhoneNameOne);
  std::unique_ptr<sync_sessions::SyncedSession> new_session =
      CreateNewSession(kPhoneNameOne);
  std::vector<const sync_sessions::SyncedSession*> sessions(
      {new_session.get()});

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
      CreateNewSession(kPhoneNameOne, base::Time::FromDoubleT(1));
  std::unique_ptr<sync_sessions::SyncedSession> session_b =
      CreateNewSession(kPhoneNameOne, base::Time::FromDoubleT(3));
  std::unique_ptr<sync_sessions::SyncedSession> session_c =
      CreateNewSession(kPhoneNameOne, base::Time::FromDoubleT(2));
  std::unique_ptr<sync_sessions::SyncedSession> session_d =
      CreateNewSession(kPhoneNameTwo, base::Time::FromDoubleT(10));

  std::vector<const sync_sessions::SyncedSession*> sessions(
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
