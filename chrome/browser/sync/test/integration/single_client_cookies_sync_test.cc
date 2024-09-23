// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/floating_sso/cookie_sync_test_util.h"
#include "chrome/browser/ash/floating_sso/floating_sso_service.h"
#include "chrome/browser/ash/floating_sso/floating_sso_service_factory.h"
#include "chrome/browser/ash/floating_sso/floating_sso_sync_bridge.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/cookie_specifics.pb.h"
#include "components/sync/test/fake_server.h"
#include "content/public/test/browser_test.h"

namespace {

using ash::floating_sso::CreatePredefinedCookieSpecificsForTest;
using ash::floating_sso::FloatingSsoService;
using ash::floating_sso::FloatingSsoServiceFactory;
using ash::floating_sso::FloatingSsoSyncBridge;

class CookiePresenceChecker : public SingleClientStatusChangeChecker {
 public:
  // Allows to wait until a cookie with a `storage_key` is added on the client.
  explicit CookiePresenceChecker(syncer::SyncServiceImpl* sync_service,
                                 FloatingSsoSyncBridge& bridge,
                                 const std::string& storage_key)
      : SingleClientStatusChangeChecker(sync_service),
        bridge_(bridge),
        storage_key_(storage_key) {
    if (!IsKeyPresentInBridgeStorage()) {
      // Make sure that we check exit condition after each local store commit.
      bridge_->SetOnStoreCommitCallbackForTest(
          base::BindRepeating(&CookiePresenceChecker::CheckExitCondition,
                              weak_factory_.GetWeakPtr()));
    }
  }
  ~CookiePresenceChecker() override = default;

  bool IsKeyPresentInBridgeStorage() const {
    return bridge_->IsInitialDataReadFinishedForTest() &&
           bridge_->CookieSpecificsInStore().contains(*storage_key_);
  }

  bool IsExitConditionSatisfied(std::ostream* os) override {
    return IsKeyPresentInBridgeStorage();
  }

 private:
  const raw_ref<FloatingSsoSyncBridge> bridge_;
  const raw_ref<const std::string> storage_key_;
  base::WeakPtrFactory<CookiePresenceChecker> weak_factory_{this};
};

class SingleClientCookiesSyncTest : public SyncTest {
 public:
  SingleClientCookiesSyncTest() : SyncTest(SINGLE_CLIENT) {
    features_.InitAndEnableFeature(ash::features::kFloatingSso);
  }

  SingleClientCookiesSyncTest(const SingleClientCookiesSyncTest&) = delete;
  SingleClientCookiesSyncTest& operator=(const SingleClientCookiesSyncTest&) =
      delete;
  ~SingleClientCookiesSyncTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    // Setup `policy_provider_` and enable Floating SSO via the policy.
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    policy::PolicyMap policy;
    policy.Set(policy::key::kFloatingSsoEnabled, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(true), nullptr);
    policy_provider_.UpdateChromePolicy(policy);
    SyncTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    SyncTest::TearDownInProcessBrowserTestFixture();
    policy_provider_.Shutdown();
  }

  void AddCookieOnTheServer(const sync_pb::CookieSpecifics& specifics) {
    sync_pb::EntitySpecifics entity;
    entity.mutable_cookie()->CopyFrom(specifics);
    fake_server_->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            specifics.name(), specifics.unique_key(), entity,
            /*creation_time=*/0, /*last_modified_time=*/0));
  }

  void AddCookieOnTheClient(const sync_pb::CookieSpecifics& specifics) {
    FloatingSsoSyncBridge& bridge = GetFloatingSsoBridge();
    base::test::TestFuture<void> commit_future;
    bridge.SetOnStoreCommitCallbackForTest(
        commit_future.GetRepeatingCallback());
    bridge.AddOrUpdateCookie(specifics);
    commit_future.Get();
  }

  void DeleteCookieOnTheClient(const std::string& storage_key) {
    FloatingSsoSyncBridge& bridge = GetFloatingSsoBridge();
    base::test::TestFuture<void> commit_future;
    bridge.SetOnStoreCommitCallbackForTest(
        commit_future.GetRepeatingCallback());
    bridge.DeleteCookie(storage_key);
    commit_future.Get();
  }

  FloatingSsoSyncBridge& GetFloatingSsoBridge() const {
    FloatingSsoService& service =
        CHECK_DEREF(FloatingSsoServiceFactory::GetForProfile(GetProfile(0)));
    return CHECK_DEREF(service.GetBridgeForTesting());
  }

 private:
  base::test::ScopedFeatureList features_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(SingleClientCookiesSyncTest, DownloadAndDelete) {
  sync_pb::CookieSpecifics remote_cookie =
      CreatePredefinedCookieSpecificsForTest(
          0, /*creation_time=*/base::Time::Now(), /*persistent=*/true);
  AddCookieOnTheServer(remote_cookie);
  ASSERT_TRUE(ServerCountMatchStatusChecker(syncer::COOKIES, 1).Wait());

  ASSERT_TRUE(SetupSync());

  // Check that the client downloaded `remote_cookie`.
  EXPECT_TRUE(CookiePresenceChecker(GetSyncService(0), GetFloatingSsoBridge(),
                                    remote_cookie.unique_key())
                  .Wait());

  // Delete `remote_cookie` on the client and check that the server reflects
  // this.
  DeleteCookieOnTheClient(remote_cookie.unique_key());
  EXPECT_TRUE(ServerCountMatchStatusChecker(syncer::COOKIES, 0).Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientCookiesSyncTest,
                       PRE_DownloadOnExistingClient) {
  // Enable Sync in the browser under test - this allows the next test to check
  // the behavior of a client which already used Sync in the past.
  ASSERT_TRUE(SetupSync());
}

// Test that the client which used Sync in the past receives an update.
IN_PROC_BROWSER_TEST_F(SingleClientCookiesSyncTest, DownloadOnExistingClient) {
  sync_pb::CookieSpecifics remote_cookie =
      CreatePredefinedCookieSpecificsForTest(
          0, /*creation_time=*/base::Time::Now(), /*persistent=*/true);
  AddCookieOnTheServer(remote_cookie);
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(ServerCountMatchStatusChecker(syncer::COOKIES, 1).Wait());

  // Check that the client downloaded `remote_cookie`.
  EXPECT_TRUE(CookiePresenceChecker(GetSyncService(0), GetFloatingSsoBridge(),
                                    remote_cookie.unique_key())
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientCookiesSyncTest, Upload) {
  ASSERT_TRUE(SetupSync());

  // Make sure that the server has no cookies initially.
  ASSERT_TRUE(ServerCountMatchStatusChecker(syncer::COOKIES, 0).Wait());

  // Add a new cookie on the client.
  sync_pb::CookieSpecifics cookie_from_client =
      CreatePredefinedCookieSpecificsForTest(
          0, /*creation_time=*/base::Time::Now(), /*persistent=*/true);
  AddCookieOnTheClient(cookie_from_client);

  // Check that the server receives the cookie.
  EXPECT_TRUE(ServerCountMatchStatusChecker(syncer::COOKIES, 1).Wait());
}

}  // namespace
