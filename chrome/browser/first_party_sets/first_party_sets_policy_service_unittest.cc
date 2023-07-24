// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/first_party_sets/scoped_mock_first_party_sets_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "services/network/public/mojom/first_party_sets_access_delegate.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pair;

MATCHER_P(CarryingConfig, config, "") {
  if (arg.is_null())
    return false;
  return ExplainMatchResult(testing::Eq(config), arg->config, result_listener);
}

MATCHER_P2(CarryingConfigAndCacheFilter, config, cache_filter, "") {
  if (arg.is_null())
    return false;
  return arg->config == config && arg->cache_filter == cache_filter;
}

namespace first_party_sets {

namespace {
const base::Version kVersion("1.2.3");
}

class MockFirstPartySetsAccessDelegate
    : public network::mojom::FirstPartySetsAccessDelegate {
 public:
  MockFirstPartySetsAccessDelegate() = default;
  MockFirstPartySetsAccessDelegate(const MockFirstPartySetsAccessDelegate&) =
      delete;
  MockFirstPartySetsAccessDelegate& operator=(
      const MockFirstPartySetsAccessDelegate&) = delete;
  ~MockFirstPartySetsAccessDelegate() override = default;

  MOCK_METHOD1(NotifyReady,
               void(network::mojom::FirstPartySetsReadyEventPtr ready_event));
  MOCK_METHOD1(SetEnabled, void(bool));
};

class DefaultFirstPartySetsPolicyServiceTest : public testing::Test {
 public:
  DefaultFirstPartySetsPolicyServiceTest() = default;

  void SetUp() override {
    mock_delegate_receiver_.Bind(
        mock_delegate_remote_.BindNewPipeAndPassReceiver());
  }

  content::BrowserTaskEnvironment& env() { return env_; }

 protected:
  testing::NiceMock<MockFirstPartySetsAccessDelegate> mock_delegate;
  mojo::Receiver<network::mojom::FirstPartySetsAccessDelegate>
      mock_delegate_receiver_{&mock_delegate};
  mojo::Remote<network::mojom::FirstPartySetsAccessDelegate>
      mock_delegate_remote_;

 private:
  content::BrowserTaskEnvironment env_;
  first_party_sets::ScopedMockFirstPartySetsHandler
      mock_first_party_sets_handler_;
};

TEST_F(DefaultFirstPartySetsPolicyServiceTest, DisabledByFeature) {
  TestingProfile profile;
  FirstPartySetsPolicyService* service =
      FirstPartySetsPolicyServiceFactory::GetForBrowserContext(&profile);
  service->AddRemoteAccessDelegate(std::move(mock_delegate_remote_));

  net::FirstPartySetsContextConfig config;
  net::FirstPartySetsCacheFilter cache_filter;

  // Ensure NotifyReady is called with the empty config.
  EXPECT_CALL(mock_delegate, NotifyReady(CarryingConfigAndCacheFilter(
                                 std::ref(config), std::ref(cache_filter))))
      .Times(1);

  env().RunUntilIdle();
}

TEST_F(DefaultFirstPartySetsPolicyServiceTest, GuestProfiles) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kFirstPartySets);
  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  FirstPartySetsPolicyService* service =
      FirstPartySetsPolicyServiceFactory::GetForBrowserContext(profile.get());
  service->AddRemoteAccessDelegate(std::move(mock_delegate_remote_));

  net::FirstPartySetsContextConfig config;
  net::FirstPartySetsCacheFilter cache_filter;

  // Ensure NotifyReady is called with the empty config.
  EXPECT_CALL(mock_delegate, NotifyReady(CarryingConfigAndCacheFilter(
                                 std::ref(config), std::ref(cache_filter))))
      .Times(1);

  env().RunUntilIdle();
}

TEST_F(DefaultFirstPartySetsPolicyServiceTest, EnabledForLegitProfile) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kFirstPartySets);
  TestingProfile profile;
  FirstPartySetsPolicyService* service =
      FirstPartySetsPolicyServiceFactory::GetForBrowserContext(&profile);
  service->AddRemoteAccessDelegate(std::move(mock_delegate_remote_));

  net::FirstPartySetsContextConfig config;
  net::FirstPartySetsCacheFilter cache_filter;

  // Ensure NotifyReady is called with the empty config.
  EXPECT_CALL(mock_delegate, NotifyReady(CarryingConfigAndCacheFilter(
                                 std::ref(config), std::ref(cache_filter))))
      .Times(1);

  env().RunUntilIdle();
}

class FirstPartySetsPolicyServiceTest
    : public DefaultFirstPartySetsPolicyServiceTest {
 public:
  FirstPartySetsPolicyServiceTest() {
    // Enable base::Feature for all tests since only the pref can change
    // whether the service is enabled.
    features_.InitAndEnableFeature(features::kFirstPartySets);
  }

  void SetUp() override {
    DefaultFirstPartySetsPolicyServiceTest::SetUp();

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("TestProfile");
    service_ =
        FirstPartySetsPolicyServiceFactory::GetForBrowserContext(profile_);
    ASSERT_NE(service_, nullptr);

    // We can't avoid eagerly initializing the service, due to
    // indirection/caching in the factory infrastructure. So we wait for the
    // initialization to complete, and then reset the instance so that we can
    // call InitForTesting and inject different configs.
    base::RunLoop run_loop;
    service_->WaitForFirstInitCompleteForTesting(run_loop.QuitClosure());
    run_loop.Run();
    service_->ResetForTesting();

    service_->AddRemoteAccessDelegate(std::move(mock_delegate_remote_));
  }

  void TearDown() override {
    CHECK(service_);
    // Even though we reassign this in SetUp, service may be persisted between
    // tests if the factory has already created a service for the testing
    // profile being used.
    service_->ResetForTesting();
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
  }

  void SetContextConfig(net::FirstPartySetsContextConfig config) {
    first_party_sets_handler_.SetContextConfig(std::move(config));
  }

  void SetCacheFilter(net::FirstPartySetsCacheFilter cache_filter) {
    first_party_sets_handler_.SetCacheFilter(std::move(cache_filter));
  }

  void SetGlobalSets(net::GlobalFirstPartySets global_sets) {
    first_party_sets_handler_.SetGlobalSets(std::move(global_sets));
  }

  void SetEnabledPref(bool enabled) {
    profile()->GetPrefs()->SetBoolean(
        prefs::kPrivacySandboxFirstPartySetsEnabled, enabled);
  }

  void SetInvokeCallbacksAsynchronously(bool asynchronous) {
    first_party_sets_handler_.set_invoke_callbacks_asynchronously(asynchronous);
  }

 protected:
  Profile* profile() { return profile_; }
  FirstPartySetsPolicyService* service() { return service_; }

 private:
  ScopedMockFirstPartySetsHandler first_party_sets_handler_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<Profile, DanglingUntriaged> profile_;
  base::test::ScopedFeatureList features_;
  raw_ptr<FirstPartySetsPolicyService, DanglingUntriaged> service_;
};

TEST_F(FirstPartySetsPolicyServiceTest, IsSiteInManagedSet_WithoutConfig) {
  EXPECT_FALSE(service()->IsSiteInManagedSet(
      net::SchemefulSite(GURL("https://example.test"))));
}

TEST_F(FirstPartySetsPolicyServiceTest, IsSiteInManagedSet_SiteNotInConfig) {
  SetContextConfig(net::FirstPartySetsContextConfig(
      {{net::SchemefulSite(GURL("https://example.test")),
        net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
            net::SchemefulSite(GURL("https://primary.test")),
            net::SiteType::kAssociated, absl::nullopt))}}));
  service()->InitForTesting();

  EXPECT_FALSE(service()->IsSiteInManagedSet(
      net::SchemefulSite(GURL("https://not-example.test"))));
  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServiceTest,
       IsSiteInManagedSet_SiteInConfig_AsDeletion) {
  net::SchemefulSite example_site(GURL("https://example.test"));
  SetContextConfig(net::FirstPartySetsContextConfig(
      {{example_site, net::FirstPartySetEntryOverride()}}));
  service()->InitForTesting();
  EXPECT_FALSE(service()->IsSiteInManagedSet(example_site));
  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServiceTest,
       IsSiteInManagedSet_SiteInConfig_AsModification) {
  net::SchemefulSite example_site(GURL("https://example.test"));
  SetContextConfig(net::FirstPartySetsContextConfig(
      {{example_site, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                          net::SchemefulSite(GURL("https://primary.test")),
                          net::SiteType::kAssociated, absl::nullopt))}}));
  service()->InitForTesting();
  EXPECT_TRUE(service()->IsSiteInManagedSet(example_site));
  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServiceTest,
       IsSiteInManagedSet_SiteInConfig_PrefDisabled) {
  net::SchemefulSite example_site(GURL("https://example.test"));
  SetContextConfig(net::FirstPartySetsContextConfig(
      {{example_site, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                          net::SchemefulSite(GURL("https://primary.test")),
                          net::SiteType::kAssociated, absl::nullopt))}}));
  SetEnabledPref(false);
  service()->InitForTesting();
  EXPECT_FALSE(service()->IsSiteInManagedSet(example_site));
  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServiceTest,
       IsSiteInManagedSet_SiteInConfig_FeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kFirstPartySets);
  net::SchemefulSite example_site(GURL("https://example.test"));
  SetContextConfig(net::FirstPartySetsContextConfig(
      {{example_site, net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
                          net::SchemefulSite(GURL("https://primary.test")),
                          net::SiteType::kAssociated, absl::nullopt))}}));
  service()->InitForTesting();
  EXPECT_FALSE(service()->IsSiteInManagedSet(example_site));
  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServiceTest, FindEntry_FpsDisabledByFeature) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList features;
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(GURL("https://associate1.test"));

  // Create Global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  SetGlobalSets(net::GlobalFirstPartySets(
      kVersion,
      {{associate1_site,
        {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated,
                                 0)}}},
      {}));
  // Simulate the profile set overrides are empty.
  service()->InitForTesting();

  // Simulate First-Party Sets disabled by the feature.
  features.InitAndDisableFeature(features::kFirstPartySets);
  SetEnabledPref(true);
  // Verify that FindEntry doesn't return associate1's entry when FPS is off.
  EXPECT_FALSE(service()->FindEntry(associate1_site));
  histogram_tester.ExpectUniqueSample(
      "Cookie.FirstPartySets.NumBrowserQueriesBeforeInitialization", 0, 1);
  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServiceTest, FindEntry_FpsDisabledByPref) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList features;
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(GURL("https://associate1.test"));

  // Create Global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  SetGlobalSets(net::GlobalFirstPartySets(
      kVersion,
      {{associate1_site,
        {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated,
                                 0)}}},
      {}));

  // Simulate First-Party Sets disabled by the preference.
  features.InitAndEnableFeature(features::kFirstPartySets);
  SetEnabledPref(false);

  service()->InitForTesting();

  // Verify that FindEntry doesn't return associate1's entry when FPS is off.
  EXPECT_FALSE(service()->FindEntry(associate1_site));
  histogram_tester.ExpectUniqueSample(
      "Cookie.FirstPartySets.NumBrowserQueriesBeforeInitialization", 0, 1);
  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServiceTest,
       FindEntry_FpsEnabled_ReturnsEmptyUntilAllSetsReady) {
  base::test::ScopedFeatureList features;
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(GURL("https://associate1.test"));
  net::FirstPartySetEntry associate1_entry(
      net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated, 0));

  // Fully enable First-Party Sets.
  features.InitAndEnableFeature(features::kFirstPartySets);
  SetEnabledPref(true);
  // Verify that FindEntry returns empty if the global sets and profile sets
  // aren't ready yet.
  EXPECT_FALSE(service()->FindEntry(associate1_site));

  // Simulate the global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  SetGlobalSets(net::GlobalFirstPartySets(
      kVersion, {{associate1_site, {associate1_entry}}}, {}));

  // Verify that FindEntry returns empty if both sources of sets aren't ready
  // yet.
  EXPECT_FALSE(service()->FindEntry(associate1_site));

  // Simulate the profile set overrides are empty.
  service()->InitForTesting();

  // Verify that FindEntry finally returns associate1's entry.
  EXPECT_EQ(service()->FindEntry(associate1_site).value(), associate1_entry);
  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServiceTest,
       FindEntry_NumQueriesRecorded_BeforeConfigReady) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList features;

  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate_site(GURL("https://associate.test"));
  net::FirstPartySetEntry associate_entry(
      net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated, 0));

  // Fully enable First-Party Sets.
  features.InitAndEnableFeature(features::kFirstPartySets);
  SetEnabledPref(true);

  // Simulate 3 FindEntry queries which all should return empty.
  EXPECT_FALSE(service()->FindEntry(associate_site));
  EXPECT_FALSE(service()->FindEntry(associate_site));
  EXPECT_FALSE(service()->FindEntry(associate_site));

  // Simulate the global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate.test"}
  SetGlobalSets(net::GlobalFirstPartySets(
      kVersion, {{associate_site, {associate_entry}}}, {}));

  // Simulate the profile set overrides are empty.
  service()->InitForTesting();

  // The queries that occur before global sets are ready should be
  // counted in our metric.
  histogram_tester.ExpectUniqueSample(
      "Cookie.FirstPartySets.NumBrowserQueriesBeforeInitialization", 3, 1);

  // Verify that FindEntry finally returns associate1's entry.
  EXPECT_EQ(service()->FindEntry(associate_site).value(), associate_entry);

  // The queries that occur after global sets are ready shouldn't be
  // counted by our metric.
  histogram_tester.ExpectUniqueSample(
      "Cookie.FirstPartySets.NumBrowserQueriesBeforeInitialization", 3, 1);

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServiceTest,
       FindEntry_NumQueriesRecorded_AfterConfigReady) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList features;

  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate_site(GURL("https://associate.test"));
  net::FirstPartySetEntry associate_entry(
      net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated, 0));

  // Fully enable First-Party Sets.
  features.InitAndEnableFeature(features::kFirstPartySets);
  SetEnabledPref(true);

  // Simulate the global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate.test"}
  SetGlobalSets(net::GlobalFirstPartySets(
      kVersion, {{associate_site, {associate_entry}}}, {}));

  // Simulate the profile set overrides are empty.
  service()->InitForTesting();

  // Simulate 3 FindEntry queries which all are answered successfully.
  EXPECT_EQ(service()->FindEntry(associate_site).value(), associate_entry);
  EXPECT_EQ(service()->FindEntry(associate_site).value(), associate_entry);
  EXPECT_EQ(service()->FindEntry(associate_site).value(), associate_entry);

  // None of the 3 queries should be counted in our metric since the service
  // already has received its context config.
  histogram_tester.ExpectUniqueSample(
      "Cookie.FirstPartySets.NumBrowserQueriesBeforeInitialization", 0, 1);
}

class FirstPartySetsPolicyServicePrefObserverTest
    : public FirstPartySetsPolicyServiceTest {
 public:
  FirstPartySetsPolicyServicePrefObserverTest() {
    // Enable base::Feature for all tests since only the pref can change
    // whether the service is enabled.
    features_.InitAndEnableFeature(features::kFirstPartySets);
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(FirstPartySetsPolicyServicePrefObserverTest,
       OnProfileConfigReady_InitDisabled_NotifiesReadyWithConfig) {
  net::SchemefulSite test_primary(GURL("https://a.test"));
  net::FirstPartySetEntry test_entry(test_primary, net::SiteType::kPrimary,
                                     absl::nullopt);
  net::FirstPartySetsContextConfig test_config(
      {{test_primary, net::FirstPartySetEntryOverride(test_entry)}});
  SetContextConfig(test_config.Clone());

  service()->InitForTesting();

  EXPECT_CALL(mock_delegate, NotifyReady(CarryingConfig(std::ref(test_config))))
      .Times(1);

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServicePrefObserverTest,
       OnFirstPartySetsEnabledChanged_Default_WithConfig) {
  service()->InitForTesting();

  EXPECT_CALL(mock_delegate, SetEnabled(_)).Times(1);
  EXPECT_CALL(mock_delegate, NotifyReady(_)).Times(1);

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServicePrefObserverTest,
       OnFirstPartySetsEnabledChanged_Default_WithoutConfig) {
  EXPECT_CALL(mock_delegate, SetEnabled(_)).Times(1);
  EXPECT_CALL(mock_delegate, NotifyReady(_)).Times(0);

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServicePrefObserverTest,
       OnFirstPartySetsEnabledChanged_Disables_WithConfig) {
  service()->InitForTesting();
  EXPECT_CALL(mock_delegate, SetEnabled(true)).Times(1);

  service()->OnFirstPartySetsEnabledChanged(false);

  EXPECT_CALL(mock_delegate, SetEnabled(false)).Times(1);
  EXPECT_CALL(mock_delegate, NotifyReady(_)).Times(1);

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServicePrefObserverTest,
       OnFirstPartySetsEnabledChanged_Disables_WithoutConfig) {
  EXPECT_CALL(mock_delegate, SetEnabled(true)).Times(1);

  service()->OnFirstPartySetsEnabledChanged(false);

  EXPECT_CALL(mock_delegate, SetEnabled(false)).Times(1);
  EXPECT_CALL(mock_delegate, NotifyReady(_)).Times(0);

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServicePrefObserverTest,
       OnFirstPartySetsEnabledChanged_Enables_WithConfig) {
  net::SchemefulSite test_primary(GURL("https://a.test"));
  net::FirstPartySetEntry test_entry(test_primary, net::SiteType::kPrimary,
                                     absl::nullopt);
  net::FirstPartySetsContextConfig test_config(
      {{test_primary, net::FirstPartySetEntryOverride(test_entry)}});
  SetContextConfig(test_config.Clone());

  service()->InitForTesting();
  service()->OnFirstPartySetsEnabledChanged(true);

  // Ensure access delegate is called with SetEnabled(true) and NotifyReady is
  // called with the config (during initialization -- not due to SetEnabled).
  EXPECT_CALL(mock_delegate, SetEnabled(true)).Times(2);

  EXPECT_CALL(mock_delegate, NotifyReady(CarryingConfig(std::ref(test_config))))
      .Times(1);

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServicePrefObserverTest,
       OnFirstPartySetsEnabledChanged_Enables_WithoutConfig) {
  service()->OnFirstPartySetsEnabledChanged(true);

  // NotifyReady isn't called since the config isn't ready to be sent.
  EXPECT_CALL(mock_delegate, SetEnabled(true)).Times(2);
  EXPECT_CALL(mock_delegate, NotifyReady(_)).Times(0);

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServiceTest, NotifiesReadyWithConfigAndCacheFilter) {
  net::SchemefulSite test_primary(GURL("https://a.test"));
  net::FirstPartySetEntry test_entry(test_primary, net::SiteType::kPrimary,
                                     absl::nullopt);
  net::FirstPartySetsContextConfig test_config(
      {{test_primary, net::FirstPartySetEntryOverride(test_entry)}});
  net::FirstPartySetsCacheFilter test_cache_filter({{test_primary, 1}},
                                                   /*browser_run_id=*/1);
  SetContextConfig(test_config.Clone());
  SetCacheFilter(test_cache_filter.Clone());
  service()->InitForTesting();

  EXPECT_CALL(mock_delegate,
              NotifyReady(CarryingConfigAndCacheFilter(
                  std::ref(test_config), std::ref(test_cache_filter))))
      .Times(1);

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServiceTest,
       ComputeFirstPartySetMetadata_BeforeInitialization) {
  net::SchemefulSite test_primary(GURL("https://a.test"));
  net::FirstPartySetEntry test_entry(test_primary, net::SiteType::kPrimary,
                                     absl::nullopt);
  net::FirstPartySetsContextConfig test_config(
      {{test_primary, net::FirstPartySetEntryOverride(test_entry)}});

  base::test::TestFuture<net::FirstPartySetMetadata> future;
  service()->ComputeFirstPartySetMetadata(test_primary, &test_primary,
                                          future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  SetContextConfig(test_config.Clone());
  SetInvokeCallbacksAsynchronously(/*asynchronous=*/true);
  service()->InitForTesting();

  EXPECT_NE(future.Take(), net::FirstPartySetMetadata());
}

TEST_F(FirstPartySetsPolicyServiceTest,
       ComputeFirstPartySetMetadata_AfterInitialization_StillAsync) {
  net::SchemefulSite test_primary(GURL("https://a.test"));
  net::FirstPartySetEntry test_entry(test_primary, net::SiteType::kPrimary,
                                     absl::nullopt);
  net::FirstPartySetsContextConfig test_config(
      {{test_primary, net::FirstPartySetEntryOverride(test_entry)}});

  SetContextConfig(test_config.Clone());
  SetInvokeCallbacksAsynchronously(/*asynchronous=*/true);
  service()->InitForTesting();

  base::test::TestFuture<net::FirstPartySetMetadata> future;
  service()->ComputeFirstPartySetMetadata(test_primary, &test_primary,
                                          future.GetCallback());
  EXPECT_NE(future.Take(), net::FirstPartySetMetadata());
}

TEST_F(FirstPartySetsPolicyServiceTest,
       ComputeFirstPartySetMetadata_AfterInitialization_Sync) {
  net::SchemefulSite test_primary(GURL("https://a.test"));
  net::FirstPartySetEntry test_entry(test_primary, net::SiteType::kPrimary,
                                     absl::nullopt);
  net::FirstPartySetsContextConfig test_config(
      {{test_primary, net::FirstPartySetEntryOverride(test_entry)}});

  SetContextConfig(test_config.Clone());
  SetInvokeCallbacksAsynchronously(/*asynchronous=*/false);
  service()->InitForTesting();

  base::test::TestFuture<net::FirstPartySetMetadata> future;
  service()->ComputeFirstPartySetMetadata(test_primary, &test_primary,
                                          future.GetCallback());
  EXPECT_TRUE(future.IsReady());
  EXPECT_NE(future.Take(), net::FirstPartySetMetadata());
}

TEST_F(FirstPartySetsPolicyServiceTest,
       ComputeFirstPartySetMetadata_PrefDisabled) {
  net::SchemefulSite test_primary(GURL("https://a.test"));
  net::FirstPartySetEntry test_entry(test_primary, net::SiteType::kPrimary,
                                     absl::nullopt);
  net::FirstPartySetsContextConfig test_config(
      {{test_primary, net::FirstPartySetEntryOverride(test_entry)}});

  SetContextConfig(test_config.Clone());
  SetInvokeCallbacksAsynchronously(/*asynchronous=*/false);
  SetEnabledPref(false);

  service()->InitForTesting();

  base::test::TestFuture<net::FirstPartySetMetadata> future;
  service()->ComputeFirstPartySetMetadata(test_primary, &test_primary,
                                          future.GetCallback());
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Take(), net::FirstPartySetMetadata());
}

class ThirdPartyCookieBlockingFirstPartySetsPolicyServiceTest
    : public DefaultFirstPartySetsPolicyServiceTest {
 protected:
  ThirdPartyCookieBlockingFirstPartySetsPolicyServiceTest() {
    features_.InitWithFeatures(
        {
            features::kFirstPartySets,
            net::features::kForceThirdPartyCookieBlocking,
        },
        {});
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(ThirdPartyCookieBlockingFirstPartySetsPolicyServiceTest, EnabledAtInit) {
  EXPECT_CALL(mock_delegate, SetEnabled(true)).Times(1);

  TestingProfile profile;
  FirstPartySetsPolicyService* service =
      FirstPartySetsPolicyServiceFactory::GetForBrowserContext(&profile);
  service->AddRemoteAccessDelegate(std::move(mock_delegate_remote_));

  env().RunUntilIdle();
}

TEST_F(ThirdPartyCookieBlockingFirstPartySetsPolicyServiceTest, AlwaysEnabled) {
  // The mock method is called once during the service construction.
  EXPECT_CALL(mock_delegate, SetEnabled(true)).Times(1);

  TestingProfile profile;
  FirstPartySetsPolicyService* service =
      FirstPartySetsPolicyServiceFactory::GetForBrowserContext(&profile);
  service->AddRemoteAccessDelegate(std::move(mock_delegate_remote_));

  // These changes should not be forwarded to the delegate.
  service->OnFirstPartySetsEnabledChanged(false);
  service->OnFirstPartySetsEnabledChanged(true);

  env().RunUntilIdle();
}

namespace {

enum PrefState { kDefault, kDisabled, kEnabled };

}  // namespace

class FirstPartySetsPolicyServiceResumeThrottleTest
    : public FirstPartySetsPolicyServiceTest,
      public ::testing::WithParamInterface<bool> {
 public:
  FirstPartySetsPolicyServiceResumeThrottleTest() {
    features_.InitAndEnableFeatureWithParameters(
        features::kFirstPartySets,
        {{features::kFirstPartySetsClearSiteDataOnChangedSets.name,
          GetParam() ? "true" : "false"}});
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Verify the throttle resume callback is always invoked.
TEST_P(FirstPartySetsPolicyServiceResumeThrottleTest,
       RegisterThrottleResumeCallback) {
  SetInvokeCallbacksAsynchronously(true);
  service()->InitForTesting();
  base::RunLoop run_loop;
  service()->RegisterThrottleResumeCallback(run_loop.QuitClosure());
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(All,
                         FirstPartySetsPolicyServiceResumeThrottleTest,
                         ::testing::Bool());

}  // namespace first_party_sets
