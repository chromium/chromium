// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/first_party_sets/scoped_mock_first_party_sets_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
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
#include "url/gurl.h"

using ::testing::_;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

MATCHER_P(CarryingConfig, config, "") {
  if (arg.is_null()) {
    return false;
  }
  return ExplainMatchResult(testing::Eq(config), arg->config, result_listener);
}

MATCHER_P2(CarryingConfigAndCacheFilter, config, cache_filter, "") {
  if (arg.is_null()) {
    return false;
  }
  return arg->config == config && arg->cache_filter == cache_filter;
}

namespace first_party_sets {

namespace {
base::Version GetVersion() {
  return base::Version("1.2.3");
}
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
  FirstPartySetsPolicyServiceTest() = default;

  void SetUp() override {
    DefaultFirstPartySetsPolicyServiceTest::SetUp();

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("TestProfile");
    service_ =
        FirstPartySetsPolicyServiceFactory::GetForBrowserContext(profile_);
    ASSERT_NE(service_, nullptr);

    profile_->GetPrefs()->SetBoolean(
        prefs::kPrivacySandboxRelatedWebsiteSetsEnabled, true);

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

  void SetCacheFilter(net::FirstPartySetsCacheFilter cache_filter) {
    first_party_sets_handler_.SetCacheFilter(std::move(cache_filter));
  }

  void SetGlobalSets(net::GlobalFirstPartySets global_sets) {
    first_party_sets_handler_.SetGlobalSets(std::move(global_sets));
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
  raw_ptr<FirstPartySetsPolicyService, DanglingUntriaged> service_;
};

TEST_F(FirstPartySetsPolicyServiceTest, IsSiteInManagedSet_WithoutConfig) {
  EXPECT_FALSE(service()->IsSiteInManagedSet(
      net::SchemefulSite(GURL("https://example.test"))));
}



class FirstPartySetsPolicyServicePrefTest
    : public FirstPartySetsPolicyServiceTest {
 public:
  void SetRwsEnabledViaPref(bool enabled) {
    profile()->GetPrefs()->SetBoolean(
        prefs::kPrivacySandboxRelatedWebsiteSetsEnabled, enabled);
  }
};

TEST_F(FirstPartySetsPolicyServicePrefTest, FindEntry_FpsDisabledByPref) {
  base::HistogramTester histogram_tester;
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(GURL("https://associate1.test"));

  // Create Global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  SetGlobalSets(net::GlobalFirstPartySets::CreateForTesting(
      GetVersion(),
      {
          {associate1_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated)}},
          {primary_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kPrimary)}},
      },
      {}));

  SetRwsEnabledViaPref(false);

  service()->InitForTesting();

  // Verify that FindEntry doesn't return associate1's entry when FPS is off.
  EXPECT_FALSE(service()->FindEntry(associate1_site));
  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServicePrefTest,
       FindEntry_FpsEnabled_ReturnsEmptyUntilAllSetsReady) {
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(GURL("https://associate1.test"));
  net::FirstPartySetEntry primary_entry(
      net::FirstPartySetEntry(primary_site, net::SiteType::kPrimary));
  net::FirstPartySetEntry associate1_entry(
      net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated));

  SetRwsEnabledViaPref(true);
  // Verify that FindEntry returns empty if the global sets and profile sets
  // aren't ready yet.
  EXPECT_FALSE(service()->FindEntry(associate1_site));

  // Simulate the global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  SetGlobalSets(net::GlobalFirstPartySets::CreateForTesting(
      GetVersion(),
      {
          {primary_site, {primary_entry}},
          {associate1_site, {associate1_entry}},
      },
      {}));

  // Verify that FindEntry returns empty if both sources of sets aren't ready
  // yet.
  EXPECT_FALSE(service()->FindEntry(associate1_site));

  // Simulate the profile set overrides are empty.
  service()->InitForTesting();

  // Verify that FindEntry finally returns associate1's entry.
  EXPECT_EQ(service()->FindEntry(associate1_site).value(), associate1_entry);
  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServicePrefTest,
       FindEntry_NumQueriesRecorded_BeforeConfigReady) {
  base::HistogramTester histogram_tester;

  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate_site(GURL("https://associate.test"));
  net::FirstPartySetEntry primary_entry(
      net::FirstPartySetEntry(primary_site, net::SiteType::kPrimary));
  net::FirstPartySetEntry associate_entry(
      net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated));

  SetRwsEnabledViaPref(true);

  // Simulate 3 FindEntry queries which all should return empty.
  EXPECT_FALSE(service()->FindEntry(associate_site));
  EXPECT_FALSE(service()->FindEntry(associate_site));
  EXPECT_FALSE(service()->FindEntry(associate_site));

  // Simulate the global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate.test"}
  SetGlobalSets(net::GlobalFirstPartySets::CreateForTesting(
      GetVersion(),
      {
          {primary_site, {primary_entry}},
          {associate_site, {associate_entry}},
      },
      {}));

  // Simulate the profile set overrides are empty.
  service()->InitForTesting();

  // Verify that FindEntry finally returns associate1's entry.
  EXPECT_EQ(service()->FindEntry(associate_site).value(), associate_entry);

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServicePrefTest,
       FindEntry_NumQueriesRecorded_AfterConfigReady) {
  base::HistogramTester histogram_tester;

  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate_site(GURL("https://associate.test"));
  net::FirstPartySetEntry primary_entry(
      net::FirstPartySetEntry(primary_site, net::SiteType::kPrimary));
  net::FirstPartySetEntry associate_entry(
      net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated));

  SetRwsEnabledViaPref(true);

  // Simulate the global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate.test"}
  SetGlobalSets(net::GlobalFirstPartySets::CreateForTesting(
      GetVersion(),
      {
          {primary_site, {primary_entry}},
          {associate_site, {associate_entry}},
      },
      {}));

  // Simulate the profile set overrides are empty.
  service()->InitForTesting();

  // Simulate 3 FindEntry queries which all are answered successfully.
  EXPECT_EQ(service()->FindEntry(associate_site).value(), associate_entry);
  EXPECT_EQ(service()->FindEntry(associate_site).value(), associate_entry);
  EXPECT_EQ(service()->FindEntry(associate_site).value(), associate_entry);
}

TEST_F(FirstPartySetsPolicyServicePrefTest,
       ForEachEffectiveSetEntry_FPSDisabledByPref) {
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate_site(GURL("https://associate.test"));

  // Create Global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate.test"}
  SetGlobalSets(net::GlobalFirstPartySets::CreateForTesting(
      GetVersion(),
      {
          {primary_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kPrimary)}},
          {associate_site,
           {net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated)}},
      },
      {}));

  // Simulate First-Party Sets disabled by the user pref.
  SetRwsEnabledViaPref(false);

  service()->InitForTesting();

  // Verify that ForEachEffectiveSetEntry returns false when FPS is off.
  EXPECT_FALSE(service()->ForEachEffectiveSetEntry(
      [&](const net::SchemefulSite& site,
          const net::FirstPartySetEntry& entry) {
        NOTREACHED();
        return true;
      }));
}

TEST_F(FirstPartySetsPolicyServicePrefTest,
       ForEachEffectiveSetEntry_ReturnsEmptyUntilAllSetsReady) {
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate_site(GURL("https://associate.test"));
  net::FirstPartySetEntry primary_entry(
      net::FirstPartySetEntry(primary_site, net::SiteType::kPrimary));
  net::FirstPartySetEntry associate_entry(
      net::FirstPartySetEntry(primary_site, net::SiteType::kAssociated));

  SetRwsEnabledViaPref(true);
  // Verify that ForEachEffectiveSetEntry returns false if FPS is not
  // initialized.
  EXPECT_FALSE(service()->ForEachEffectiveSetEntry(
      [&](const net::SchemefulSite& site,
          const net::FirstPartySetEntry& entry) {
        NOTREACHED();
        return true;
      }));

  // Create the global First-Party Sets with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate.test"}
  SetGlobalSets(net::GlobalFirstPartySets::CreateForTesting(
      GetVersion(),
      {{primary_site, {primary_entry}}, {associate_site, {associate_entry}}},
      {}));

  // Verify that ForEachEffectiveSetEntry returns false if service is not ready.
  EXPECT_FALSE(service()->ForEachEffectiveSetEntry(
      [&](const net::SchemefulSite& site,
          const net::FirstPartySetEntry& entry) {
        NOTREACHED();
        return true;
      }));

  // Simulate the profile set overrides are empty.
  service()->InitForTesting();

  std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>>
      set_entries;
  EXPECT_TRUE(service()->ForEachEffectiveSetEntry(
      [&](const net::SchemefulSite& site,
          const net::FirstPartySetEntry& entry) {
        set_entries.emplace_back(site, entry);
        return true;
      }));
  EXPECT_THAT(set_entries,
              UnorderedElementsAre(Pair(primary_site, primary_entry),
                                   Pair(associate_site, associate_entry)));
}



TEST_F(FirstPartySetsPolicyServicePrefTest,
       OnRelatedWebsiteSetsEnabledChanged_Default_WithConfig) {
  service()->InitForTesting();

  EXPECT_CALL(mock_delegate, SetEnabled(_)).Times(1);
  EXPECT_CALL(mock_delegate, NotifyReady(_)).Times(1);

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServicePrefTest,
       OnRelatedWebsiteSetsEnabledChanged_Default_WithoutConfig) {
  EXPECT_CALL(mock_delegate, SetEnabled(_)).Times(1);
  EXPECT_CALL(mock_delegate, NotifyReady(_)).Times(0);

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServicePrefTest,
       OnRelatedWebsiteSetsEnabledChanged_Disables_WithConfig) {
  service()->InitForTesting();
  EXPECT_CALL(mock_delegate, SetEnabled(true)).Times(1);

  service()->OnRelatedWebsiteSetsEnabledChanged(false);

  EXPECT_CALL(mock_delegate, SetEnabled(false)).Times(1);
  EXPECT_CALL(mock_delegate, NotifyReady(_)).Times(1);

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServicePrefTest,
       OnRelatedWebsiteSetsEnabledChanged_Disables_WithoutConfig) {
  EXPECT_CALL(mock_delegate, SetEnabled(true)).Times(1);

  service()->OnRelatedWebsiteSetsEnabledChanged(false);

  EXPECT_CALL(mock_delegate, SetEnabled(false)).Times(1);
  EXPECT_CALL(mock_delegate, NotifyReady(_)).Times(0);

  env().RunUntilIdle();
}



TEST_F(FirstPartySetsPolicyServicePrefTest,
       OnRelatedWebsiteSetsEnabledChanged_Enables_WithoutConfig) {
  service()->OnRelatedWebsiteSetsEnabledChanged(true);

  // NotifyReady isn't called since the config isn't ready to be sent.
  EXPECT_CALL(mock_delegate, SetEnabled(true)).Times(2);
  EXPECT_CALL(mock_delegate, NotifyReady(_)).Times(0);

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServicePrefTest,
       OnRelatedWebsiteSetsEnabledChanged_OTRProfile) {
  testing::NiceMock<MockFirstPartySetsAccessDelegate> mock_delegate;
  EXPECT_CALL(mock_delegate, SetEnabled(false)).Times(1);
  EXPECT_CALL(mock_delegate, SetEnabled(true)).Times(0);
  EXPECT_CALL(mock_delegate, NotifyReady(_)).Times(1);
  mojo::Receiver<network::mojom::FirstPartySetsAccessDelegate>
      mock_delegate_receiver{&mock_delegate};
  mojo::Remote<network::mojom::FirstPartySetsAccessDelegate>
      mock_delegate_remote;

  mock_delegate_receiver.Bind(
      mock_delegate_remote.BindNewPipeAndPassReceiver());

  FirstPartySetsPolicyService* otr_service =
      FirstPartySetsPolicyServiceFactory::GetForBrowserContext(
          profile()->GetOffTheRecordProfile(
              Profile::OTRProfileID::CreateUniqueForTesting(),
              /*create_if_needed=*/true));
  otr_service->ResetForTesting();

  otr_service->InitForTesting();
  otr_service->AddRemoteAccessDelegate(std::move(mock_delegate_remote));

  ASSERT_FALSE(otr_service->is_enabled());
  env().RunUntilIdle();

  otr_service->OnRelatedWebsiteSetsEnabledChanged(true);
  EXPECT_FALSE(otr_service->is_enabled());

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServiceTest, NotifiesReadyWithCacheFilter) {
  net::SchemefulSite test_primary(GURL("https://a.test"));
  net::FirstPartySetsCacheFilter test_cache_filter({{test_primary, 1}},
                                                   /*browser_run_id=*/1);
  SetCacheFilter(test_cache_filter.Clone());
  service()->InitForTesting();

  net::FirstPartySetsContextConfig empty_config;
  EXPECT_CALL(mock_delegate,
              NotifyReady(CarryingConfigAndCacheFilter(
                  std::ref(empty_config), std::ref(test_cache_filter))))
      .Times(1);

  env().RunUntilIdle();
}

TEST_F(FirstPartySetsPolicyServiceTest,
       ComputeFirstPartySetMetadata_BeforeInitialization) {
  net::SchemefulSite test_primary(GURL("https://a.test"));
  net::SchemefulSite test_associated(GURL("https://b.test"));
  net::FirstPartySetEntry primary_entry(test_primary, net::SiteType::kPrimary);
  net::FirstPartySetEntry associated_entry(test_primary,
                                           net::SiteType::kAssociated);

  base::test::TestFuture<net::FirstPartySetMetadata> future;
  service()->ComputeFirstPartySetMetadata(test_associated, &test_primary,
                                          future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  SetGlobalSets(net::GlobalFirstPartySets::CreateForTesting(
      GetVersion(),
      {
          {test_primary, primary_entry},
          {test_associated, associated_entry},
      },
      {}));
  SetInvokeCallbacksAsynchronously(/*asynchronous=*/true);
  service()->InitForTesting();

  EXPECT_NE(future.Take(), net::FirstPartySetMetadata());
}

TEST_F(FirstPartySetsPolicyServiceTest,
       ComputeFirstPartySetMetadata_AfterInitialization_StillAsync) {
  net::SchemefulSite test_primary(GURL("https://a.test"));
  net::SchemefulSite test_associated(GURL("https://b.test"));
  net::FirstPartySetEntry primary_entry(test_primary, net::SiteType::kPrimary);
  net::FirstPartySetEntry associated_entry(test_primary,
                                           net::SiteType::kAssociated);

  SetGlobalSets(net::GlobalFirstPartySets::CreateForTesting(
      GetVersion(),
      {
          {test_primary, primary_entry},
          {test_associated, associated_entry},
      },
      {}));
  SetInvokeCallbacksAsynchronously(/*asynchronous=*/true);
  service()->InitForTesting();

  base::test::TestFuture<net::FirstPartySetMetadata> future;
  service()->ComputeFirstPartySetMetadata(test_associated, &test_primary,
                                          future.GetCallback());
  EXPECT_NE(future.Take(), net::FirstPartySetMetadata());
}

TEST_F(FirstPartySetsPolicyServiceTest,
       ComputeFirstPartySetMetadata_AfterInitialization_Sync) {
  net::SchemefulSite test_primary(GURL("https://a.test"));
  net::SchemefulSite test_associated(GURL("https://b.test"));
  net::FirstPartySetEntry primary_entry(test_primary, net::SiteType::kPrimary);
  net::FirstPartySetEntry associated_entry(test_primary,
                                           net::SiteType::kAssociated);

  SetGlobalSets(net::GlobalFirstPartySets::CreateForTesting(
      GetVersion(),
      {
          {test_primary, primary_entry},
          {test_associated, associated_entry},
      },
      {}));
  SetInvokeCallbacksAsynchronously(/*asynchronous=*/false);
  service()->InitForTesting();

  base::test::TestFuture<net::FirstPartySetMetadata> future;
  service()->ComputeFirstPartySetMetadata(test_associated, &test_primary,
                                          future.GetCallback());
  EXPECT_TRUE(future.IsReady());
  EXPECT_NE(future.Take(), net::FirstPartySetMetadata());
}

TEST_F(FirstPartySetsPolicyServicePrefTest,
       ComputeFirstPartySetMetadata_PrefDisabled) {
  net::SchemefulSite test_primary(GURL("https://a.test"));
  net::SchemefulSite test_associated(GURL("https://b.test"));
  net::FirstPartySetEntry primary_entry(test_primary, net::SiteType::kPrimary);
  net::FirstPartySetEntry associated_entry(test_primary,
                                           net::SiteType::kAssociated);

  SetGlobalSets(net::GlobalFirstPartySets::CreateForTesting(
      GetVersion(),
      {
          {test_primary, primary_entry},
          {test_associated, associated_entry},
      },
      {}));
  SetInvokeCallbacksAsynchronously(/*asynchronous=*/false);
  SetRwsEnabledViaPref(false);

  service()->InitForTesting();

  base::test::TestFuture<net::FirstPartySetMetadata> future;
  service()->ComputeFirstPartySetMetadata(test_associated, &test_primary,
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
  service->OnRelatedWebsiteSetsEnabledChanged(false);
  service->OnRelatedWebsiteSetsEnabledChanged(true);

  env().RunUntilIdle();
}

// Verify the throttle resume callback is always invoked.
TEST_F(FirstPartySetsPolicyServiceTest, RegisterThrottleResumeCallback) {
  SetInvokeCallbacksAsynchronously(true);
  service()->InitForTesting();
  base::RunLoop run_loop;
  service()->RegisterThrottleResumeCallback(run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace first_party_sets
