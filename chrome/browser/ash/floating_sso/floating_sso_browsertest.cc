// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/floating_sso/cookie_sync_conversions.h"
#include "chrome/browser/ash/floating_sso/cookie_sync_test_util.h"
#include "chrome/browser/ash/floating_sso/floating_sso_service.h"
#include "chrome/browser/ash/floating_sso/floating_sso_service_factory.h"
#include "chrome/browser/ash/floating_sso/floating_sso_sync_bridge.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/dependency_graph.h"
#include "components/keyed_service/core/keyed_service_base_factory.h"
#include "components/policy/policy_constants.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/protocol/cookie_specifics.pb.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace ash::floating_sso {

namespace {

using testing::_;

constexpr char kStandardCookieLine[] = "CookieName=CookieValue; max-age=3600";

constexpr char kCookieName[] = "CookieName";

// Unique key for standard cookie (kStandardCookieLine and kNonGoogleURL).
// Has cross-site ancestor (true), name (CookieName), domain + path
// (example.com/), kSecure scheme (2), port (8888).
constexpr char kCookieUniqueKey[] = "trueCookieNameexample.com/28888";

class CookieChangeListener : public network::mojom::CookieChangeListener {
 public:
  // Create a change listener for all cookies.
  CookieChangeListener(
      network::mojom::CookieManager* cookie_manager,
      base::RepeatingCallback<void(const net::CookieChangeInfo&)> callback)
      : callback_(std::move(callback)), receiver_(this) {
    cookie_manager->AddGlobalChangeListener(
        receiver_.BindNewPipeAndPassRemote());
  }

  // Create a change listener limited to a specific URL.
  CookieChangeListener(
      network::mojom::CookieManager* cookie_manager,
      const GURL& url,
      base::RepeatingCallback<void(const net::CookieChangeInfo&)> callback)
      : callback_(std::move(callback)), receiver_(this) {
    cookie_manager->AddCookieChangeListener(
        url, std::nullopt, receiver_.BindNewPipeAndPassRemote());
  }

  // network::mojom::CookieChangeListener:
  void OnCookieChange(const net::CookieChangeInfo& change) override {
    callback_.Run(change);
  }

 private:
  base::RepeatingCallback<void(const net::CookieChangeInfo&)> callback_;
  mojo::Receiver<network::mojom::CookieChangeListener> receiver_;
};

std::optional<net::CanonicalCookie> GetCookie(
    network::mojom::CookieManager* cookie_manager,
    const std::string& name) {
  base::test::TestFuture<const std::vector<net::CanonicalCookie>&> future;
  cookie_manager->GetAllCookies(future.GetCallback());
  for (const net::CanonicalCookie& cookie : future.Take()) {
    if (cookie.Name() == name) {
      return std::make_optional<net::CanonicalCookie>(cookie);
    }
  }
  return std::nullopt;
}

// Returns if the cookie was added successfully.
bool SetCookie(network::mojom::CookieManager* cookie_manager,
               const GURL& url,
               const net::CanonicalCookie& cookie) {
  base::test::TestFuture<net::CookieAccessResult> future;
  cookie_manager->SetCanonicalCookie(cookie, url,
                                     net::CookieOptions::MakeAllInclusive(),
                                     future.GetCallback());

  return future.Take().status.IsInclude();
}

// Returns if the cookie was added successfully.
bool SetCookie(network::mojom::CookieManager* cookie_manager,
               const net::CanonicalCookie& cookie) {
  GURL url = net::cookie_util::SimulatedCookieSource(cookie, "https");
  return SetCookie(cookie_manager, url, cookie);
}

// Returns if the cookie was added successfully.
bool SetCookie(network::mojom::CookieManager* cookie_manager,
               const GURL& url,
               const std::string& cookie_line) {
  auto cookie = net::CanonicalCookie::CreateForTesting(
      url, cookie_line, base::Time::Now(),
      /*server_time=*/std::nullopt,
      /*cookie_partition_key=*/std::nullopt, net::CookieSourceType::kOther);

  return SetCookie(cookie_manager, url, *cookie);
}

// Returns the number of deleted cookies.
uint32_t DeleteCookies(network::mojom::CookieManager* cookie_manager,
                       network::mojom::CookieDeletionFilter filter) {
  base::test::TestFuture<uint32_t> future;
  cookie_manager->DeleteCookies(
      network::mojom::CookieDeletionFilter::New(filter), future.GetCallback());
  return future.Get();
}

}  // namespace

class FloatingSsoTest : public policy::PolicyTest {
 public:
  FloatingSsoTest() {
    feature_list_.InitAndEnableFeature(ash::features::kFloatingSso);
  }
  ~FloatingSsoTest() override = default;

  void SetUpOnMainThread() override {
    network::mojom::NetworkContext* network_context =
        profile()->GetDefaultStoragePartition()->GetNetworkContext();
    network_context->GetCookieManager(
        cookie_manager_.BindNewPipeAndPassReceiver());
  }

 protected:
  void SetFloatingSsoEnabledPolicy(bool policy_value) {
    policy::PolicyMap policies;
    policy::PolicyTest::SetPolicy(&policies, policy::key::kFloatingSsoEnabled,
                                  base::Value(policy_value));
    provider_.UpdateChromePolicy(policies);
  }

  bool IsFloatingSsoServiceRegistered() {
    std::vector<raw_ptr<DependencyNode, VectorExperimental>> nodes;
    const bool success = BrowserContextDependencyManager::GetInstance()
                             ->GetDependencyGraphForTesting()
                             .GetConstructionOrder(&nodes);
    EXPECT_TRUE(success);
    return base::Contains(
        nodes, "FloatingSsoService",
        [](const DependencyNode* node) -> std::string_view {
          return static_cast<const KeyedServiceBaseFactory*>(node)->name();
        });
  }

  Profile* profile() { return browser()->profile(); }

  FloatingSsoService& floating_sso_service() {
    return CHECK_DEREF(FloatingSsoServiceFactory::GetForProfile(profile()));
  }

  network::mojom::CookieManager* cookie_manager() {
    return cookie_manager_.get();
  }

  const FloatingSsoSyncBridge::CookieSpecificsEntries& GetStoreEntries() {
    return floating_sso_service()
        .GetBridgeForTesting()
        ->CookieSpecificsInStore();
  }

  void AddCookieAndWaitForCommit(network::mojom::CookieManager* cookie_manager,
                                 const GURL& url,
                                 const std::string& cookie_line) {
    // Used for waiting for the store commit to be finalized.
    base::test::TestFuture<void> commit_future;
    floating_sso_service().GetBridgeForTesting()->SetOnCommitCallbackForTest(
        commit_future.GetRepeatingCallback());

    // Used for waiting for the cookie change event (INSERTED) to be dispatched.
    base::test::TestFuture<const net::CookieChangeInfo&> cookie_change_future;
    CookieChangeListener listener(cookie_manager, url,
                                  cookie_change_future.GetRepeatingCallback());

    // Add cookie.
    ASSERT_TRUE(SetCookie(cookie_manager, url, cookie_line));
    EXPECT_EQ(cookie_change_future.Take().cause,
              net::CookieChangeCause::INSERTED);
    commit_future.Get();
  }

  void DeleteCookieAndWaitForCommit(
      network::mojom::CookieManager* cookie_manager,
      const GURL& url,
      const std::string& cookie_name) {
    // Used for waiting for the store commit to be finalized.
    base::test::TestFuture<void> commit_future;
    floating_sso_service().GetBridgeForTesting()->SetOnCommitCallbackForTest(
        commit_future.GetRepeatingCallback());

    // Used for waiting for the cookie change event (EXPLICIT) to be dispatched.
    base::test::TestFuture<const net::CookieChangeInfo&> cookie_change_future;
    CookieChangeListener listener(cookie_manager, url,
                                  cookie_change_future.GetRepeatingCallback());

    // Delete cookie.
    network::mojom::CookieDeletionFilter filter;
    filter.cookie_name = cookie_name;
    ASSERT_EQ(DeleteCookies(cookie_manager, filter), 1u);
    EXPECT_EQ(cookie_change_future.Take().cause,
              net::CookieChangeCause::EXPLICIT);
    commit_future.Get();
  }

  void UpdateCookieAndWaitForCommit(
      network::mojom::CookieManager* cookie_manager,
      const GURL& url,
      const std::string& cookie_name) {
    // Used for waiting for the two store commits to be finalized.
    base::test::TestFuture<void> commit_future;
    floating_sso_service().GetBridgeForTesting()->SetOnCommitCallbackForTest(
        base::BarrierClosure(
            /*num_callbacks=*/2, commit_future.GetRepeatingCallback()));

    // Used for waiting for the cookie change events (OVERWRITE, INSERTED) to be
    // dispatched.
    base::test::TestFuture<std::vector<net::CookieChangeInfo>>
        cookie_change_future;
    CookieChangeListener listener(
        cookie_manager, url,
        base::BarrierCallback<const net::CookieChangeInfo&>(
            /*num_callbacks=*/2, cookie_change_future.GetRepeatingCallback()));

    // Update cookie.
    auto cookie = GetCookie(cookie_manager, cookie_name);
    ASSERT_TRUE(cookie.has_value());

    cookie.value().SetLastAccessDate(base::Time::Now());
    cookie_manager->SetCanonicalCookie(*cookie, url,
                                       net::CookieOptions::MakeAllInclusive(),
                                       base::DoNothing());

    // Updating an existing cookie is a two-phase delete + insert operation, so
    // two cookie change events are triggered.
    EXPECT_THAT(cookie_change_future.Take(),
                testing::ElementsAre(
                    testing::Field("cause", &net::CookieChangeInfo::cause,
                                   net::CookieChangeCause::OVERWRITE),
                    testing::Field("cause", &net::CookieChangeInfo::cause,
                                   net::CookieChangeCause::INSERTED)));
    commit_future.Get();
  }

  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
  base::test::ScopedFeatureList feature_list_;
  const GURL kNonGoogleURL = GURL("https://example.com:8888");
};

IN_PROC_BROWSER_TEST_F(FloatingSsoTest, ServiceRegistered) {
  ASSERT_TRUE(IsFloatingSsoServiceRegistered());
}

// TODO: b/346354327 - this test should check if changing cookies
// results in creation of Sync commits when the policy is enabled or
// disabled. For now it just checks a test-only flag which should be
// deprecated once we can test the intended behavior.
IN_PROC_BROWSER_TEST_F(FloatingSsoTest, CanBeEnabledViaPolicy) {
  auto& service = floating_sso_service();
  // Policy is disabled so the service shouldn't be enabled yet.
  EXPECT_FALSE(service.is_enabled_for_testing_);
  // Switch the policy on and off and make sure that the service reacts.
  SetFloatingSsoEnabledPolicy(/*policy_value=*/true);
  EXPECT_TRUE(service.is_enabled_for_testing_);
  SetFloatingSsoEnabledPolicy(/*policy_value=*/false);
  EXPECT_FALSE(service.is_enabled_for_testing_);
}

IN_PROC_BROWSER_TEST_F(FloatingSsoTest, FiltersOutGoogleCookies) {
  auto& service = floating_sso_service();
  SetFloatingSsoEnabledPolicy(/*policy_value=*/true);
  EXPECT_TRUE(service.is_enabled_for_testing_);

  ASSERT_TRUE(SetCookie(cookie_manager(), GURL("https://google.com"),
                        kStandardCookieLine));
  ASSERT_TRUE(SetCookie(cookie_manager(), GURL("https://accounts.google.com"),
                        kStandardCookieLine));
  ASSERT_TRUE(SetCookie(cookie_manager(), GURL("https://youtube.com"),
                        kStandardCookieLine));

  // Cookies are not added to store.
  auto store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(FloatingSsoTest, FiltersOutSessionCookies) {
  auto& service = floating_sso_service();
  SetFloatingSsoEnabledPolicy(/*policy_value=*/true);
  EXPECT_TRUE(service.is_enabled_for_testing_);

  ASSERT_TRUE(
      SetCookie(cookie_manager(), kNonGoogleURL, "CookieName=CookieValue"));

  // Cookie is not added to store.
  auto store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(FloatingSsoTest, KeepsThirdPartyCookies) {
  auto& service = floating_sso_service();
  SetFloatingSsoEnabledPolicy(/*policy_value=*/true);
  EXPECT_TRUE(service.is_enabled_for_testing_);

  ASSERT_TRUE(
      SetCookie(cookie_manager(), kNonGoogleURL,
                "CookieName=CookieValue; SameSite=None; Secure; max-age=3600"));

  // Cookie is added to store.
  auto store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 1u);
  EXPECT_TRUE(store_entries.contains(kCookieUniqueKey));
}

IN_PROC_BROWSER_TEST_F(FloatingSsoTest, AddsAndDeletesCookiesToStore) {
  auto& service = floating_sso_service();
  SetFloatingSsoEnabledPolicy(/*policy_value=*/true);
  EXPECT_TRUE(service.is_enabled_for_testing_);

  // Add cookie.
  AddCookieAndWaitForCommit(cookie_manager(), kNonGoogleURL,
                            kStandardCookieLine);

  // Cookie is added to store.
  auto store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 1u);
  EXPECT_TRUE(store_entries.contains(kCookieUniqueKey));

  // Update cookie.
  UpdateCookieAndWaitForCommit(cookie_manager(), kNonGoogleURL, kCookieName);
  store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 1u);

  // Delete cookie.
  DeleteCookieAndWaitForCommit(cookie_manager(), kNonGoogleURL, kCookieName);
  store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 0u);
}

// Verify that `FloatingSsoService` reacts to cookie changes arriving to
// `FloatingSsoSyncBridge` by applying corresponding changes to the browser.
IN_PROC_BROWSER_TEST_F(FloatingSsoTest, ApplyingChangesFromSync) {
  // Populate cookie jar with a cookie. We create a cookie from predefined
  // specifics just for convenience (easier to use its sync storage key when we
  // need it below).
  sync_pb::CookieSpecifics existing_local_cookie_specifics =
      CreatePredefinedCookieSpecificsForTest(
          0, /*creation_time=*/base::Time::Now(), /*persistent=*/true);
  std::unique_ptr<net::CanonicalCookie> existing_local_cookie =
      FromSyncProto(existing_local_cookie_specifics);
  ASSERT_TRUE(existing_local_cookie);
  ASSERT_TRUE(SetCookie(cookie_manager(), *existing_local_cookie));

  auto& service = floating_sso_service();
  SetFloatingSsoEnabledPolicy(/*policy_value=*/true);
  EXPECT_TRUE(service.is_enabled_for_testing_);

  // Set up a listener for cookie change events, we expect to observe two
  // changes below.
  base::test::TestFuture<std::vector<net::CookieChangeInfo>>
      cookie_change_future;
  CookieChangeListener listener(
      cookie_manager(),
      base::BarrierCallback<const net::CookieChangeInfo&>(
          /*num_callbacks=*/2, cookie_change_future.GetRepeatingCallback()));

  // This change list will only contain a Sync change which shouldn't result in
  // any browser changes.
  syncer::EntityChangeList no_op_change_list;
  // Addition of a new session cookie: we expect that we will not add it
  // to the cookie jar since session cookies are not synced by default. Note
  // that they might still come from the sync server if in the past they were
  // synced based on a different policy configuration.
  no_op_change_list.push_back(syncer::EntityChange::CreateAdd(
      kUniqueKeysForTests[2],
      CreateEntityDataForTest(CreatePredefinedCookieSpecificsForTest(
          2, /*creation_time=*/base::Time::Now(), /*persistent=*/false))));

  FloatingSsoSyncBridge& bridge =
      CHECK_DEREF(floating_sso_service().GetBridgeForTesting());
  // Apply `no_op_change_list`. We do it before applying other changes to be
  // sure that in case of a bug the cookie change from this call would appear by
  // the time we check all observed cookie changes at the end of the test.
  bridge.ApplyIncrementalSyncChanges(bridge.CreateMetadataChangeList(),
                                     std::move(no_op_change_list));

  // Create two sync changes which will affect the browser.
  syncer::EntityChangeList change_list;
  // Deletion of a cookie we added at the start of the test. We expect it to
  // eventually generate a `net::CookieChangeCause::EXPLICIT` event.
  change_list.push_back(syncer::EntityChange::CreateDelete(
      existing_local_cookie_specifics.unique_key()));
  // Addition of a new persistent cookie: we expect it to pass our filters and
  // eventually generate a `net::CookieChangeCause::INSERTED` event.
  change_list.push_back(syncer::EntityChange::CreateAdd(
      kUniqueKeysForTests[1],
      CreateEntityDataForTest(CreatePredefinedCookieSpecificsForTest(
          1, /*creation_time=*/base::Time::Now(), /*persistent=*/true))));

  // Apply changes via the bridge - this should in turn notify
  // `FloatingSsoService`, which is expected to notify the cookie manager.
  bridge.ApplyIncrementalSyncChanges(bridge.CreateMetadataChangeList(),
                                     std::move(change_list));

  // We expect one cookie to be deleted (corresponds to
  // `net::CookieChangeCause::EXPLICIT`) and one cookie to be added
  // (`net::CookieChangeCause::INSERTED`).
  EXPECT_THAT(cookie_change_future.Take(),
              testing::UnorderedElementsAre(
                  testing::Field("cause", &net::CookieChangeInfo::cause,
                                 net::CookieChangeCause::EXPLICIT),
                  testing::Field("cause", &net::CookieChangeInfo::cause,
                                 net::CookieChangeCause::INSERTED)));
}

class FloatingSsoWithFloatingWorkspaceTest : public FloatingSsoTest {
 public:
  FloatingSsoWithFloatingWorkspaceTest() {
    feature_list_.Reset();
    feature_list_.InitWithFeatures(
        {features::kFloatingSso, features::kFloatingWorkspaceV2},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    // Disable Floating Workspace functionality because there is something in
    // the implementation that is making this test crash.
    // TODO(b/354907485): Investigate what is causing the crash and remove this
    // command line argument.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kSafeMode);

    PolicyTest::SetUp();
  }

 protected:
  void EnableFloatingPolicies() {
    policy::PolicyMap policies;
    policy::PolicyTest::SetPolicy(&policies, policy::key::kFloatingSsoEnabled,
                                  base::Value(true));
    policy::PolicyTest::SetPolicy(
        &policies, policy::key::kFloatingWorkspaceV2Enabled, base::Value(true));
    provider_.UpdateChromePolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(FloatingSsoWithFloatingWorkspaceTest,
                       KeepsSessionCookiesIfFloatingWorkspaceEnabled) {
  auto& service = floating_sso_service();
  EnableFloatingPolicies();
  EXPECT_TRUE(service.is_enabled_for_testing_);

  AddCookieAndWaitForCommit(cookie_manager(), kNonGoogleURL,
                            "CookieName=CookieValue");

  auto store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 1u);
}

// Defines mock versions of `AddOrUpdateCookie` and `DeleteCookie` which are
// the main methods to notify the bridge about local changes. This class allows
// to test how `FloatingSsoService` calls those methods of the bridge.
class MockFloatingSsoSyncBridge : public FloatingSsoSyncBridge {
 public:
  explicit MockFloatingSsoSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory create_store_callback)
      : FloatingSsoSyncBridge(std::move(change_processor),
                              std::move(create_store_callback)) {}
  ~MockFloatingSsoSyncBridge() override = default;

  MOCK_METHOD(void,
              AddOrUpdateCookie,
              (const sync_pb::CookieSpecifics& specifics),
              (override));
  MOCK_METHOD(void, DeleteCookie, (const std::string& storage_key), (override));
};

class FloatingSsoWithMockedBridgeTest : public FloatingSsoTest {
 public:
  void SetUpOnMainThread() override {
    FloatingSsoTest::SetUpOnMainThread();
    std::unique_ptr<testing::NiceMock<MockFloatingSsoSyncBridge>> mock_bridge =
        std::make_unique<testing::NiceMock<MockFloatingSsoSyncBridge>>(
            processor_.CreateForwardingProcessor(),
            DataTypeStoreServiceFactory::GetForProfile(profile())
                ->GetStoreFactory());
    bridge_ = mock_bridge.get();
    // Wait until the bridge finishes reading initial data from the store.
    ASSERT_TRUE(base::test::RunUntil([&mock_bridge]() {
      return mock_bridge->IsInitialDataReadFinishedForTest();
    }));
    floating_sso_service().SetBridgeForTesting(std::move(mock_bridge));
  }

  void TearDownOnMainThread() override {
    bridge_ = nullptr;
    FloatingSsoTest::TearDownOnMainThread();
  }

  // Add a cookie as if requested by the Sync server, i.e. by calling
  // `ApplyIncrementalSyncChanges` on the bridge.
  void AddCookieSyncRequest(const sync_pb::CookieSpecifics& specifics) {
    base::test::TestFuture<const net::CookieChangeInfo&> cookie_change_future;
    CookieChangeListener listener(cookie_manager(),
                                  cookie_change_future.GetRepeatingCallback());

    syncer::EntityChangeList addition_list;
    addition_list.push_back(syncer::EntityChange::CreateAdd(
        specifics.unique_key(), CreateEntityDataForTest(specifics)));
    bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                         std::move(addition_list));

    // Wait for the change to be noticed by the browser.
    ASSERT_EQ(cookie_change_future.Take().cause,
              net::CookieChangeCause::INSERTED);
  }

  // Remove a cookie as if requested by the Sync server, i.e. by calling
  // `ApplyIncrementalSyncChanges` on the bridge.
  void RemoveCookieSyncRequest(const sync_pb::CookieSpecifics& specifics) {
    base::test::TestFuture<const net::CookieChangeInfo&> cookie_change_future;
    CookieChangeListener listener(cookie_manager(),
                                  cookie_change_future.GetRepeatingCallback());

    syncer::EntityChangeList deletion_list;
    deletion_list.push_back(
        syncer::EntityChange::CreateDelete(specifics.unique_key()));
    bridge().ApplyIncrementalSyncChanges(bridge().CreateMetadataChangeList(),
                                         std::move(deletion_list));

    // Wait for the change to be noticed by the browser.
    ASSERT_EQ(cookie_change_future.Take().cause,
              net::CookieChangeCause::EXPLICIT);
  }

  testing::NiceMock<MockFloatingSsoSyncBridge>& bridge() { return *bridge_; }

 private:
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> processor_;
  raw_ptr<testing::NiceMock<MockFloatingSsoSyncBridge>> bridge_;
};

IN_PROC_BROWSER_TEST_F(FloatingSsoWithMockedBridgeTest,
                       NoOpChangesAreNotPassedToBridge) {
  auto& service = floating_sso_service();
  SetFloatingSsoEnabledPolicy(/*policy_value=*/true);
  ASSERT_TRUE(service.is_enabled_for_testing_);

  // Below we will add and then delete a cookie via calls to
  // `ApplyIncrementalSyncChanges` method of the bridge. Since
  // `FloatingSsoService` observes all cookie changes, it could in theory notify
  // the bridge about these changes. Check that this doesn't happen (because the
  // service should not ask the bridge to perform no-op changes).
  EXPECT_CALL(bridge(), AddOrUpdateCookie(_)).Times(0);
  EXPECT_CALL(bridge(), DeleteCookie(_)).Times(0);

  const sync_pb::CookieSpecifics specifics =
      CreatePredefinedCookieSpecificsForTest(
          0, /*creation_time=*/base::Time::Now(), /*persistent=*/true);
  AddCookieSyncRequest(specifics);
  RemoveCookieSyncRequest(specifics);
}

}  // namespace ash::floating_sso
