// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"

#include "base/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/first_party_sets/mock_first_party_sets_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "services/network/public/mojom/first_party_sets_access_delegate.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

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
    content::FirstPartySetsHandler::GetInstance()->ResetForTesting();
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
    content::FirstPartySetsHandler::GetInstance()->ResetForTesting();
    content::FirstPartySetsHandler::GetInstance()->SetGlobalSetsForTesting({});

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
    DCHECK(service_);
    // Even though we reassign this in SetUp, service may be persisted between
    // tests if the factory has already created a service for the testing
    // profile being used.
    service_->ResetForTesting();
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
  }

 protected:
  Profile* profile() { return profile_; }
  FirstPartySetsPolicyService* service() { return service_; }

 private:
  std::unique_ptr<TestingProfileManager> profile_manager_;
  Profile* profile_;
  base::test::ScopedFeatureList features_;
  FirstPartySetsPolicyService* service_;
};

TEST_F(FirstPartySetsPolicyServiceTest, IsSiteInManagedSet_WithoutConfig) {
  EXPECT_FALSE(service()->IsSiteInManagedSet(
      net::SchemefulSite(GURL("https://example.test"))));
}

TEST_F(FirstPartySetsPolicyServiceTest, IsSiteInManagedSet_SiteNotInConfig) {
  service()->InitForTesting(
      [](PrefService* prefs,
         base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
        std::move(callback).Run(net::FirstPartySetsContextConfig(
            {{net::SchemefulSite(GURL("https://example.test")),
              {net::FirstPartySetEntry(
                  net::SchemefulSite(GURL("https://primary.test")),
                  net::SiteType::kAssociated, absl::nullopt)}}}));
      });

  EXPECT_FALSE(service()->IsSiteInManagedSet(
      net::SchemefulSite(GURL("https://not-example.test"))));
  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServiceTest,
       IsSiteInManagedSet_SiteInConfig_AsDeletion) {
  net::SchemefulSite example_site =
      net::SchemefulSite(GURL("https://example.test"));
  service()->InitForTesting(
      [example_site](
          PrefService* prefs,
          base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
        std::move(callback).Run(net::FirstPartySetsContextConfig(
            {{example_site, {absl::nullopt}}}));
      });
  EXPECT_FALSE(service()->IsSiteInManagedSet(example_site));
  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServiceTest,
       IsSiteInManagedSet_SiteInConfig_AsModification) {
  net::SchemefulSite example_site =
      net::SchemefulSite(GURL("https://example.test"));
  service()->InitForTesting(
      [example_site](
          PrefService* prefs,
          base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
        std::move(callback).Run(net::FirstPartySetsContextConfig(
            {{example_site,
              {net::FirstPartySetEntry(
                  net::SchemefulSite(GURL("https://primary.test")),
                  net::SiteType::kAssociated, absl::nullopt)}}}));
      });
  EXPECT_TRUE(service()->IsSiteInManagedSet(example_site));
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
  content::FirstPartySetsHandler::GetInstance()->SetGlobalSetsForTesting(
      net::GlobalFirstPartySets(
          {{associate1_site,
            {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated,
                                     0)}}},
          {}));
  // Simulate the profile set overrides are empty.
  service()->InitForTesting(
      [](PrefService* prefs,
         base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
        std::move(callback).Run(net::FirstPartySetsContextConfig());
      });

  // Simulate First-Party Sets disabled by the feature.
  features.InitAndDisableFeature(features::kFirstPartySets);
  profile()->GetPrefs()->SetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled,
                                    true);
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
  content::FirstPartySetsHandler::GetInstance()->SetGlobalSetsForTesting(
      net::GlobalFirstPartySets(
          {{associate1_site,
            {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated,
                                     0)}}},
          {}));
  // Simulate the profile set overrides are empty.
  service()->InitForTesting(
      [](PrefService* prefs,
         base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
        std::move(callback).Run(net::FirstPartySetsContextConfig());
      });

  // Simulate First-Party Sets disabled by the preference.
  features.InitAndEnableFeature(features::kFirstPartySets);
  profile()->GetPrefs()->SetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled,
                                    false);

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
  profile()->GetPrefs()->SetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled,
                                    true);
  // Verify that FindEntry returns empty if the global sets and profile sets
  // aren't ready yet.
  EXPECT_FALSE(service()->FindEntry(associate1_site));

  // Simulate the global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  content::FirstPartySetsHandler::GetInstance()->SetGlobalSetsForTesting(
      net::GlobalFirstPartySets({{associate1_site, {associate1_entry}}}, {}));

  // Verify that FindEntry returns empty if both sources of sets aren't ready
  // yet.
  EXPECT_FALSE(service()->FindEntry(associate1_site));

  // Simulate the profile set overrides are empty.
  service()->InitForTesting(
      [](PrefService* prefs,
         base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
        std::move(callback).Run(net::FirstPartySetsContextConfig());
      });

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
  profile()->GetPrefs()->SetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled,
                                    true);

  // Simulate 3 FindEntry queries which all should return empty.
  EXPECT_FALSE(service()->FindEntry(associate_site));
  EXPECT_FALSE(service()->FindEntry(associate_site));
  EXPECT_FALSE(service()->FindEntry(associate_site));

  // Simulate the global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate.test"}
  content::FirstPartySetsHandler::GetInstance()->SetGlobalSetsForTesting(
      net::GlobalFirstPartySets({{associate_site, {associate_entry}}}, {}));

  // Simulate the profile set overrides are empty.
  service()->InitForTesting(
      [](PrefService* prefs,
         base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
        std::move(callback).Run(net::FirstPartySetsContextConfig());
      });

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
  profile()->GetPrefs()->SetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled,
                                    true);

  // Simulate the global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate.test"}
  content::FirstPartySetsHandler::GetInstance()->SetGlobalSetsForTesting(
      net::GlobalFirstPartySets({{associate_site, {associate_entry}}}, {}));

  // Simulate the profile set overrides are empty.
  service()->InitForTesting(
      [](PrefService* prefs,
         base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
        std::move(callback).Run(net::FirstPartySetsContextConfig());
      });

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
  net::FirstPartySetsContextConfig test_config({{test_primary, {test_entry}}});

  service()->InitForTesting(
      [&](PrefService* prefs,
          base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
        std::move(callback).Run(test_config.Clone());
      });

  EXPECT_CALL(mock_delegate, NotifyReady(CarryingConfig(std::ref(test_config))))
      .Times(1);

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServicePrefObserverTest,
       OnFirstPartySetsEnabledChanged_Default_WithConfig) {
  service()->InitForTesting(
      [](PrefService* prefs,
         base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
        std::move(callback).Run(net::FirstPartySetsContextConfig());
      });

  EXPECT_CALL(mock_delegate, SetEnabled(_)).Times(0);
  EXPECT_CALL(mock_delegate, NotifyReady(_)).Times(1);

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServicePrefObserverTest,
       OnFirstPartySetsEnabledChanged_Default_WithoutConfig) {
  EXPECT_CALL(mock_delegate, SetEnabled(_)).Times(0);
  EXPECT_CALL(mock_delegate, NotifyReady(_)).Times(0);

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServicePrefObserverTest,
       OnFirstPartySetsEnabledChanged_Disables_WithConfig) {
  service()->InitForTesting(
      [](PrefService* prefs,
         base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
        std::move(callback).Run(net::FirstPartySetsContextConfig());
      });
  service()->OnFirstPartySetsEnabledChanged(false);

  EXPECT_CALL(mock_delegate, SetEnabled(false)).Times(1);
  EXPECT_CALL(mock_delegate, NotifyReady(_)).Times(1);

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServicePrefObserverTest,
       OnFirstPartySetsEnabledChanged_Disables_WithoutConfig) {
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
  net::FirstPartySetsContextConfig test_config({{test_primary, {test_entry}}});

  service()->InitForTesting(
      [&](PrefService* prefs,
          base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
        std::move(callback).Run(test_config.Clone());
      });
  service()->OnFirstPartySetsEnabledChanged(true);

  // Ensure access delegate is called with SetEnabled(true) and NotifyReady is
  // called with the config (during initialization -- not due to SetEnabled).
  EXPECT_CALL(mock_delegate, SetEnabled(true)).Times(1);

  EXPECT_CALL(mock_delegate, NotifyReady(CarryingConfig(std::ref(test_config))))
      .Times(1);

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServicePrefObserverTest,
       OnFirstPartySetsEnabledChanged_Enables_WithoutConfig) {
  service()->OnFirstPartySetsEnabledChanged(true);

  // NotifyReady isn't called since the config isn't ready to be sent.
  EXPECT_CALL(mock_delegate, SetEnabled(true)).Times(1);
  EXPECT_CALL(mock_delegate, NotifyReady(_)).Times(0);

  env().RunUntilIdle();
}

class FirstPartySetsPolicyServiceWithMockHandlerTest
    : public FirstPartySetsPolicyServiceTest {
 public:
  FirstPartySetsPolicyServiceWithMockHandlerTest() = default;

  void SetUp() override {
    FirstPartySetsPolicyServiceTest::SetUp();

    content::FirstPartySetsHandler::GetInstance()->SetInstanceForTesting(
        &first_party_sets_handler_);
  }

  void TearDown() override {
    FirstPartySetsPolicyServiceTest::TearDown();
    first_party_sets_handler_.ResetForTesting();
    content::FirstPartySetsHandler::GetInstance()->SetInstanceForTesting(
        nullptr);
  }

  void SetContextConfig(net::FirstPartySetsContextConfig config) {
    first_party_sets_handler_.SetContextConfig(std::move(config));
  }
  void SetCacheFilter(net::FirstPartySetsCacheFilter cache_filter) {
    first_party_sets_handler_.SetCacheFilter(std::move(cache_filter));
  }

 private:
  MockFirstPartySetsHandler first_party_sets_handler_;
  base::test::ScopedFeatureList features_;
};

TEST_F(FirstPartySetsPolicyServiceWithMockHandlerTest,
       NotifiesReadyWithConfigAndCacheFilter) {
  net::SchemefulSite test_primary(GURL("https://a.test"));
  net::FirstPartySetEntry test_entry(test_primary, net::SiteType::kPrimary,
                                     absl::nullopt);
  net::FirstPartySetsContextConfig test_config({{test_primary, {test_entry}}});
  net::FirstPartySetsCacheFilter test_cache_filter({{test_primary, 1}},
                                                   /*browser_run_id=*/1);
  SetContextConfig(test_config.Clone());
  SetCacheFilter(test_cache_filter.Clone());
  service()->InitForTesting(
      [&](PrefService* prefs,
          base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
        std::move(callback).Run(test_config.Clone());
      });

  EXPECT_CALL(mock_delegate,
              NotifyReady(CarryingConfigAndCacheFilter(
                  std::ref(test_config), std::ref(test_cache_filter))))
      .Times(1);

  env().RunUntilIdle();
}

namespace {

enum PrefState { kDefault, kDisabled, kEnabled };

}  // namespace

class FirstPartySetsPolicyServiceResumeThrottleTest
    : public FirstPartySetsPolicyServiceTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, PrefState>> {
 public:
  FirstPartySetsPolicyServiceResumeThrottleTest() {
    if (IsFeatureEnabled()) {
      features_.InitAndEnableFeatureWithParameters(
          features::kFirstPartySets,
          {{features::kFirstPartySetsClearSiteDataOnChangedSets.name,
            IsClearingFeatureEnabled() ? "true" : "false"}});
    } else {
      features_.InitAndDisableFeature(features::kFirstPartySets);
    }
  }

  bool IsPrefEnabled() { return GetPrefState() == PrefState::kEnabled; }

 private:
  bool IsFeatureEnabled() { return std::get<0>(GetParam()); }
  bool IsClearingFeatureEnabled() { return std::get<1>(GetParam()); }
  PrefState GetPrefState() { return std::get<2>(GetParam()); }

  base::test::ScopedFeatureList features_;
};

// Verify the throttle resume callback is always invoked.
TEST_P(FirstPartySetsPolicyServiceResumeThrottleTest,
       MaybeAddNavigationThrottleResumeCallback) {
  profile()->GetPrefs()->SetBoolean(prefs::kPrivacySandboxFirstPartySetsEnabled,
                                    IsPrefEnabled());
  base::RunLoop run_loop;
  service()->RegisterThrottleResumeCallback(run_loop.QuitClosure());
  service()->InitForTesting(
      [&](PrefService* prefs,
          base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
        std::move(callback).Run(net::FirstPartySetsContextConfig());
      });
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FirstPartySetsPolicyServiceResumeThrottleTest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Bool(),
                       ::testing::Values(PrefState::kDefault,
                                         PrefState::kDisabled,
                                         PrefState::kEnabled)));

}  // namespace first_party_sets
