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
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
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

// Cookie that passes the Floating SSO filters.
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
    policy::PolicyTest::SetPolicy(&policies_, policy::key::kFloatingSsoEnabled,
                                  base::Value(policy_value));
    provider_.UpdateChromePolicy(policies_);
  }

  void SetSyncCookiesPref(bool pref_value) {
    profile()->GetPrefs()->SetBoolean(syncer::prefs::internal::kSyncCookies,
                                      pref_value);
  }

  void SetSyncDisabledPolicy(bool policy_value) {
    policy::PolicyTest::SetPolicy(&policies_, policy::key::kSyncDisabled,
                                  base::Value(policy_value));
    provider_.UpdateChromePolicy(policies_);
  }

  void SetFloatingSsoDomainBlocklistPolicy(const std::string& domain) {
    base::Value::List domains;
    domains.Append(domain);
    policy::PolicyTest::SetPolicy(&policies_,
                                  policy::key::kFloatingSsoDomainBlocklist,
                                  base::Value(std::move(domains)));
    provider_.UpdateChromePolicy(policies_);
  }

  void SetFloatingSsoDomainBlocklistExceptionsPolicy(
      const std::string& domain) {
    base::Value::List domains;
    if (!domain.empty()) {
      domains.Append(domain);
    }
    policy::PolicyTest::SetPolicy(
        &policies_, policy::key::kFloatingSsoDomainBlocklistExceptions,
        base::Value(std::move(domains)));
    provider_.UpdateChromePolicy(policies_);
  }

  void EnableAllFloatingSsoSettings() {
    SetFloatingSsoEnabledPolicy(/*policy_value=*/true);
    SetSyncCookiesPref(/*pref_value=*/true);
    SetSyncDisabledPolicy(/*policy_value=*/false);
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
    floating_sso_service()
        .GetBridgeForTesting()
        ->SetOnStoreCommitCallbackForTest(commit_future.GetRepeatingCallback());

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
    floating_sso_service()
        .GetBridgeForTesting()
        ->SetOnStoreCommitCallbackForTest(commit_future.GetRepeatingCallback());

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
    floating_sso_service()
        .GetBridgeForTesting()
        ->SetOnStoreCommitCallbackForTest(base::BarrierClosure(
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
  policy::PolicyMap policies_;
};

IN_PROC_BROWSER_TEST_F(FloatingSsoTest, ServiceRegistered) {
  ASSERT_TRUE(IsFloatingSsoServiceRegistered());
}

// FloatingSsoEnabled policy is disabled, cookies are not added to the store.
IN_PROC_BROWSER_TEST_F(FloatingSsoTest, FloatingSsoPolicyDisabled) {
  auto& service = floating_sso_service();
  SetFloatingSsoEnabledPolicy(/*policy_value=*/false);
  SetSyncCookiesPref(/*pref_value=*/true);
  SetSyncDisabledPolicy(/*policy_value=*/false);

  ASSERT_FALSE(service.IsBoundToCookieManagerForTesting());

  ASSERT_TRUE(SetCookie(cookie_manager(), kNonGoogleURL, kStandardCookieLine));

  // Cookie is not added to store because the FloatingSsoEnabled policy is
  // disabled.
  auto store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 0u);
}

// SyncCookies pref (user toggle) is disabled, cookies are not added to the
// store.
IN_PROC_BROWSER_TEST_F(FloatingSsoTest, SyncCookiesPrefDisabled) {
  auto& service = floating_sso_service();
  SetFloatingSsoEnabledPolicy(/*policy_value=*/true);
  SetSyncCookiesPref(/*pref_value=*/false);
  SetSyncDisabledPolicy(/*policy_value=*/false);

  ASSERT_FALSE(service.IsBoundToCookieManagerForTesting());

  ASSERT_TRUE(SetCookie(cookie_manager(), kNonGoogleURL, kStandardCookieLine));

  // Cookie is not added to store because the SyncCookies pref is disabled.
  auto store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 0u);
}

// SyncDisabled policy is enabled, cookies are not added to the store.
IN_PROC_BROWSER_TEST_F(FloatingSsoTest, SyncDisabled) {
  auto& service = floating_sso_service();
  SetFloatingSsoEnabledPolicy(/*policy_value=*/true);
  SetSyncCookiesPref(/*pref_value=*/true);
  SetSyncDisabledPolicy(/*policy_value=*/true);

  ASSERT_FALSE(service.IsBoundToCookieManagerForTesting());

  ASSERT_TRUE(SetCookie(cookie_manager(), kNonGoogleURL, kStandardCookieLine));

  // Cookie is not added to store because the SyncCookies pref is disabled.
  auto store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(FloatingSsoTest, FloatingSsoEnabled) {
  auto& service = floating_sso_service();
  EnableAllFloatingSsoSettings();
  ASSERT_TRUE(service.IsBoundToCookieManagerForTesting());

  AddCookieAndWaitForCommit(cookie_manager(), kNonGoogleURL,
                            kStandardCookieLine);

  // Cookie is added to store.
  auto store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 1u);
  EXPECT_TRUE(store_entries.contains(kCookieUniqueKey));
}

IN_PROC_BROWSER_TEST_F(FloatingSsoTest, FloatingSsoStopsListeningAndResumes) {
  auto& service = floating_sso_service();
  EnableAllFloatingSsoSettings();
  ASSERT_TRUE(service.IsBoundToCookieManagerForTesting());

  AddCookieAndWaitForCommit(cookie_manager(), kNonGoogleURL,
                            kStandardCookieLine);

  // Cookie is added to store.
  const auto& store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 1u);
  EXPECT_TRUE(store_entries.contains(kCookieUniqueKey));

  SetFloatingSsoEnabledPolicy(/*policy_value=*/false);

  base::test::TestFuture<const net::CookieChangeInfo&> cookie_change_future;
  CookieChangeListener listener(cookie_manager(), kNonGoogleURL,
                                cookie_change_future.GetRepeatingCallback());

  // Add new cookie.
  ASSERT_TRUE(SetCookie(cookie_manager(), kNonGoogleURL,
                        "CookieNameNew=CookieValueNew; max-age=3600"));
  EXPECT_EQ(cookie_change_future.Take().cause,
            net::CookieChangeCause::INSERTED);
  EXPECT_EQ(store_entries.size(), 1u);

  // We fetch and commit both cookies again, so we need to wait for 2 commits.
  base::test::TestFuture<void> commit_future;
  floating_sso_service().GetBridgeForTesting()->SetOnStoreCommitCallbackForTest(
      base::BarrierClosure(
          /*num_callbacks=*/2, commit_future.GetRepeatingCallback()));

  // Re-enabling means that the cookies are fetched again and committed to the
  // store.
  SetFloatingSsoEnabledPolicy(/*policy_value=*/true);
  commit_future.Get();
  EXPECT_EQ(store_entries.size(), 2u);
}

IN_PROC_BROWSER_TEST_F(FloatingSsoTest, FiltersOutGoogleCookies) {
  auto& service = floating_sso_service();
  EnableAllFloatingSsoSettings();
  ASSERT_TRUE(service.IsBoundToCookieManagerForTesting());

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
  EnableAllFloatingSsoSettings();
  ASSERT_TRUE(service.IsBoundToCookieManagerForTesting());

  ASSERT_TRUE(
      SetCookie(cookie_manager(), kNonGoogleURL, "CookieName=CookieValue"));

  // Cookie is not added to store.
  auto store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(FloatingSsoTest,
                       FiltersCookiesWithBlocklistBasicPattern) {
  auto& service = floating_sso_service();
  EnableAllFloatingSsoSettings();
  SetFloatingSsoDomainBlocklistPolicy("example.com");
  ASSERT_TRUE(service.IsBoundToCookieManagerForTesting());

  ASSERT_TRUE(SetCookie(cookie_manager(), GURL("https://example.com"),
                        kStandardCookieLine));
  ASSERT_TRUE(SetCookie(cookie_manager(), GURL("http://example.com"),
                        kStandardCookieLine));
  ASSERT_TRUE(SetCookie(cookie_manager(), GURL("https://www.example.com"),
                        kStandardCookieLine));
  ASSERT_TRUE(SetCookie(cookie_manager(), GURL("https://sub.www.example.com"),
                        kStandardCookieLine));

  // Cookies are not added to store.
  auto store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(FloatingSsoTest,
                       FiltersCookiesWithBlocklistSubdomainPattern) {
  auto& service = floating_sso_service();
  EnableAllFloatingSsoSettings();
  SetFloatingSsoDomainBlocklistPolicy("mail.example.com");
  ASSERT_TRUE(service.IsBoundToCookieManagerForTesting());

  ASSERT_TRUE(SetCookie(cookie_manager(), GURL("https://mail.example.com"),
                        kStandardCookieLine));

  // Cookie is not added to store.
  const auto& store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 0u);

  // Other subdomains are not filtered.
  AddCookieAndWaitForCommit(cookie_manager(), GURL("http://example.com"),
                            kStandardCookieLine);
  AddCookieAndWaitForCommit(cookie_manager(), GURL("https://www.example.com"),
                            kStandardCookieLine);

  // Cookies are added to store.
  EXPECT_EQ(store_entries.size(), 2u);
}

IN_PROC_BROWSER_TEST_F(FloatingSsoTest, FiltersCookiesWithBlocklistDotPattern) {
  auto& service = floating_sso_service();
  EnableAllFloatingSsoSettings();
  SetFloatingSsoDomainBlocklistPolicy(".example.com");
  ASSERT_TRUE(service.IsBoundToCookieManagerForTesting());

  ASSERT_TRUE(SetCookie(cookie_manager(), GURL("https://example.com"),
                        kStandardCookieLine));

  // Cookie is not added to store.
  const auto& store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 0u);

  // Subdomains are not filtered.
  AddCookieAndWaitForCommit(cookie_manager(), GURL("https://mail.example.com"),
                            kStandardCookieLine);

  // Cookie is added to store.
  EXPECT_EQ(store_entries.size(), 1u);
}

IN_PROC_BROWSER_TEST_F(FloatingSsoTest,
                       AllowSpecificDomainsWithWildcardBlocking) {
  auto& service = floating_sso_service();
  EnableAllFloatingSsoSettings();
  SetFloatingSsoDomainBlocklistPolicy("*");
  SetFloatingSsoDomainBlocklistExceptionsPolicy("example.com");
  ASSERT_TRUE(service.IsBoundToCookieManagerForTesting());

  ASSERT_TRUE(SetCookie(cookie_manager(), GURL("https://mail.com"),
                        kStandardCookieLine));
  ASSERT_TRUE(SetCookie(cookie_manager(), GURL("https://test.com"),
                        kStandardCookieLine));

  // Cookies are not added to store.
  const auto& store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 0u);

  // Allows domains in exceptions.
  AddCookieAndWaitForCommit(cookie_manager(), GURL("https://example.com"),
                            kStandardCookieLine);

  // Cookie is added to store.
  EXPECT_EQ(store_entries.size(), 1u);
}

IN_PROC_BROWSER_TEST_F(FloatingSsoTest, ExceptionListTakesPrecedence) {
  auto& service = floating_sso_service();
  EnableAllFloatingSsoSettings();
  SetFloatingSsoDomainBlocklistPolicy("example.com");
  SetFloatingSsoDomainBlocklistExceptionsPolicy("example.com");
  ASSERT_TRUE(service.IsBoundToCookieManagerForTesting());

  AddCookieAndWaitForCommit(cookie_manager(), GURL("https://example.com"),
                            kStandardCookieLine);

  // Cookie is added to store.
  auto store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 1u);
}

IN_PROC_BROWSER_TEST_F(FloatingSsoTest,
                       FiltersOutGoogleCookiesDespiteException) {
  auto& service = floating_sso_service();
  EnableAllFloatingSsoSettings();
  SetFloatingSsoDomainBlocklistExceptionsPolicy("google.com");
  ASSERT_TRUE(service.IsBoundToCookieManagerForTesting());

  ASSERT_TRUE(SetCookie(cookie_manager(), GURL("https://google.com"),
                        kStandardCookieLine));
  ASSERT_TRUE(SetCookie(cookie_manager(), GURL("https://accounts.google.com"),
                        kStandardCookieLine));

  // Cookies are not added to store.
  auto store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(FloatingSsoTest, RespectsBlockAndExemptListUpdates) {
  auto& service = floating_sso_service();
  EnableAllFloatingSsoSettings();
  SetFloatingSsoDomainBlocklistPolicy("*");
  SetFloatingSsoDomainBlocklistExceptionsPolicy("example.com");
  ASSERT_TRUE(service.IsBoundToCookieManagerForTesting());

  ASSERT_TRUE(SetCookie(cookie_manager(), GURL("https://mail.com"),
                        kStandardCookieLine));
  AddCookieAndWaitForCommit(cookie_manager(), GURL("https://example.com"),
                            kStandardCookieLine);

  // Only the example.com cookie is added to the store.
  const auto& store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 1u);
  constexpr char kExampleCookieUniqueKey[] = "trueCookieNameexample.com/2443";
  EXPECT_TRUE(store_entries.contains(kExampleCookieUniqueKey));

  // Update the block and exception list.
  SetFloatingSsoDomainBlocklistPolicy("example.com");
  SetFloatingSsoDomainBlocklistExceptionsPolicy("");

  // Changing the cookie URL for mail.com to not trigger an update but an
  // insert.
  AddCookieAndWaitForCommit(cookie_manager(), GURL("https://sub.mail.com/"),
                            kStandardCookieLine);
  ASSERT_TRUE(SetCookie(cookie_manager(), GURL("https://example.com"),
                        kStandardCookieLine));

  // sub.mail.com cookie is added to store. The store still contains the
  // example.com cookie.
  EXPECT_EQ(store_entries.size(), 2u);
  EXPECT_TRUE(store_entries.contains(kExampleCookieUniqueKey));
  EXPECT_TRUE(store_entries.contains("trueCookieNamesub.mail.com/2443"));
}

IN_PROC_BROWSER_TEST_F(FloatingSsoTest, CookiesFromSyncBlocked) {
  auto& service = floating_sso_service();
  EnableAllFloatingSsoSettings();
  SetFloatingSsoDomainBlocklistPolicy("example.com");
  ASSERT_TRUE(service.IsBoundToCookieManagerForTesting());

  // Set up a listener for cookie change events, we expect to observe one change
  // below.
  base::test::TestFuture<const net::CookieChangeInfo&> cookie_change_future;
  CookieChangeListener listener(cookie_manager(),
                                cookie_change_future.GetRepeatingCallback());

  FloatingSsoSyncBridge& bridge =
      CHECK_DEREF(floating_sso_service().GetBridgeForTesting());

  // Create two sync changes.
  syncer::EntityChangeList change_list;
  // Addition of a new persistent cookie with the example.com domain. We expect
  // it to not pass our filters because of the blocklist.
  change_list.push_back(syncer::EntityChange::CreateAdd(
      kUniqueKeysForTests[1],
      CreateEntityDataForTest(CreatePredefinedCookieSpecificsForTest(
          1, /*creation_time=*/base::Time::Now(), /*persistent=*/true))));

  // Addition of a new persistent cookie with the test.com domain. We expect it
  // to pass our filters since it does not match the blocklist.
  constexpr char kTestCookieUniqueKey[] = "trueCookieNametest.com/2443";
  change_list.push_back(syncer::EntityChange::CreateAdd(
      kTestCookieUniqueKey,
      CreateEntityDataForTest(
          CreateCookieSpecificsForTest(kTestCookieUniqueKey, kCookieName,
                                       /*creation_time=*/base::Time::Now(),
                                       /*persistent=*/true, "www.test.com"))));

  bridge.ApplyIncrementalSyncChanges(bridge.CreateMetadataChangeList(),
                                     std::move(change_list));

  // We expect one cookie to be added (`net::CookieChangeCause::INSERTED`). The
  // other cookie doesn't trigger any event because it is filtered out based on
  // the blocklist and doesn't get added to the cookie jar.
  EXPECT_EQ(cookie_change_future.Take().cause,
            net::CookieChangeCause::INSERTED);
}

IN_PROC_BROWSER_TEST_F(FloatingSsoTest, KeepsThirdPartyCookies) {
  auto& service = floating_sso_service();
  EnableAllFloatingSsoSettings();
  ASSERT_TRUE(service.IsBoundToCookieManagerForTesting());

  AddCookieAndWaitForCommit(
      cookie_manager(), kNonGoogleURL,
      "CookieName=CookieValue; SameSite=None; Secure; max-age=3600");

  // Cookie is added to store.
  auto store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 1u);
  EXPECT_TRUE(store_entries.contains(kCookieUniqueKey));
}

IN_PROC_BROWSER_TEST_F(FloatingSsoTest, AddsAndDeletesCookiesToStore) {
  auto& service = floating_sso_service();
  EnableAllFloatingSsoSettings();
  ASSERT_TRUE(service.IsBoundToCookieManagerForTesting());

  // Add cookie.
  AddCookieAndWaitForCommit(cookie_manager(), kNonGoogleURL,
                            kStandardCookieLine);

  // Cookie is added to store.
  const auto& store_entries = GetStoreEntries();
  EXPECT_EQ(store_entries.size(), 1u);
  EXPECT_TRUE(store_entries.contains(kCookieUniqueKey));

  // Update cookie.
  UpdateCookieAndWaitForCommit(cookie_manager(), kNonGoogleURL, kCookieName);
  EXPECT_EQ(store_entries.size(), 1u);

  // Delete cookie.
  DeleteCookieAndWaitForCommit(cookie_manager(), kNonGoogleURL, kCookieName);
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
  EnableAllFloatingSsoSettings();
  ASSERT_TRUE(service.IsBoundToCookieManagerForTesting());

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
  void EnableFloatingWorkspace() {
    policy::PolicyTest::SetPolicy(&policies_,
                                  policy::key::kFloatingWorkspaceV2Enabled,
                                  base::Value(true));
    provider_.UpdateChromePolicy(policies_);
  }
};

IN_PROC_BROWSER_TEST_F(FloatingSsoWithFloatingWorkspaceTest,
                       KeepsSessionCookiesIfFloatingWorkspaceEnabled) {
  auto& service = floating_sso_service();
  EnableAllFloatingSsoSettings();
  EnableFloatingWorkspace();
  ASSERT_TRUE(service.IsBoundToCookieManagerForTesting());

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
  void SetUpInProcessBrowserTestFixture() override {
    FloatingSsoTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&FloatingSsoWithMockedBridgeTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    FloatingSsoServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindOnce([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
          Profile* profile = Profile::FromBrowserContext(context);
          return std::make_unique<FloatingSsoService>(
              profile->GetPrefs(),
              std::make_unique<testing::NiceMock<MockFloatingSsoSyncBridge>>(
                  std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
                      syncer::COOKIES, base::DoNothing()),
                  DataTypeStoreServiceFactory::GetForProfile(profile)
                      ->GetStoreFactory()),
              profile->GetDefaultStoragePartition()
                  ->GetCookieManagerForBrowserProcess());
        }));
  }

  void SetUpOnMainThread() override {
    FloatingSsoTest::SetUpOnMainThread();
    // Wait until the bridge finishes reading initial data from the store.
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return bridge().IsInitialDataReadFinishedForTest(); }));
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

  testing::NiceMock<MockFloatingSsoSyncBridge>& bridge() {
    return static_cast<testing::NiceMock<MockFloatingSsoSyncBridge>&>(
        *floating_sso_service().GetBridgeForTesting());
  }

 private:
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(FloatingSsoWithMockedBridgeTest,
                       NoOpChangesAreNotPassedToBridge) {
  auto& service = floating_sso_service();
  EnableAllFloatingSsoSettings();
  ASSERT_TRUE(service.IsBoundToCookieManagerForTesting());

  // Below we will add and then delete a cookie via calls to
  // `ApplyIncrementalSyncChanges` method of the bridge. Since
  // `FloatingSsoService` observes all cookie changes, it could in theory notify
  // the bridge about these changes. Check that this doesn't happen (because the
  // service should not ask the bridge to perform no-op changes).
  EXPECT_CALL(bridge(), AddOrUpdateCookie).Times(0);
  EXPECT_CALL(bridge(), DeleteCookie).Times(0);

  const sync_pb::CookieSpecifics specifics =
      CreatePredefinedCookieSpecificsForTest(
          0, /*creation_time=*/base::Time::Now(), /*persistent=*/true);
  AddCookieSyncRequest(specifics);
  RemoveCookieSyncRequest(specifics);
}

}  // namespace ash::floating_sso
