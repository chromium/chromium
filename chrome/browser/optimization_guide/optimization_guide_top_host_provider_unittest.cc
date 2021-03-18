// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_top_host_provider.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/prefs/pref_service.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

// Class to test the TopHostProvider and the HintsFetcherTopHostBlocklist.
class OptimizationGuideTopHostProviderTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // Advance by 1-day to avoid running into null checks.
    test_clock_.Advance(base::TimeDelta::FromDays(1));

    top_host_provider_ = base::WrapUnique(
        new OptimizationGuideTopHostProvider(profile(), &test_clock_));

    service_ = site_engagement::SiteEngagementService::Get(profile());
    pref_service_ = profile()->GetPrefs();

    drp_test_context_ =
        data_reduction_proxy::DataReductionProxyTestContext::Builder()
            .WithMockConfig()
            .Build();
    // Make sure the param values are in a well known state.
    site_engagement::SiteEngagementScore::SetParamValuesForTesting();
  }

  void TearDown() override {
    drp_test_context_->DestroySettings();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetDataSaverEnabled(bool enabled) {
    drp_test_context_->SetDataReductionProxyEnabled(enabled);
  }

  void SetIsPermittedToUseTopHostProvider(bool enabled) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (enabled) {
      command_line->AppendSwitch(optimization_guide::switches::
                                     kDisableCheckingUserPermissionsForTesting);
    } else {
      command_line->RemoveSwitch(optimization_guide::switches::
                                     kDisableCheckingUserPermissionsForTesting);
    }
  }

  void AddEngagedHosts(size_t num_hosts) {
    for (size_t i = 1; i <= num_hosts; i++) {
      // Add 1 to GetFirstDailyEngagementPoints because it is 0
      // in tests (it's 1.5 in Chrome). Otherwise,
      // OptimizationGuideTopHostProvider::GetTopHosts() may filter out
      // the first host we add, because
      // MinTopHostEngagementScoreThreshold defaults to 2.
      AddEngagedHost(GURL(base::StringPrintf("https://domain%zu.com", i)),
                     static_cast<double>(i +
                                         site_engagement::SiteEngagementScore::
                                             GetFirstDailyEngagementPoints() +
                                         1));
    }
  }

  void AddEngagedHostsWithPoints(size_t num_hosts, double num_points) {
    for (size_t i = 1; i <= num_hosts; i++) {
      AddEngagedHost(GURL(base::StringPrintf("https://domain%zu.com", i)),
                     num_points);
    }
  }

  void AddEngagedHost(GURL url, double num_points) {
    service_->AddPointsForTesting(url, num_points);
  }

  bool IsHostBlocklisted(const std::string& host) const {
    const base::DictionaryValue* top_host_blocklist =
        pref_service_->GetDictionary(
            optimization_guide::prefs::kHintsFetcherTopHostBlocklist);
    return top_host_blocklist->FindKey(
        optimization_guide::HashHostForDictionary(host));
  }

  double GetHintsFetcherTopHostBlocklistMinimumEngagementScore() const {
    return pref_service_->GetDouble(
        optimization_guide::prefs::
            kHintsFetcherTopHostBlocklistMinimumEngagementScore);
  }

  void PopulateTopHostBlocklist(size_t num_hosts) {
    std::unique_ptr<base::DictionaryValue> top_host_filter =
        pref_service_
            ->GetDictionary(
                optimization_guide::prefs::kHintsFetcherTopHostBlocklist)
            ->CreateDeepCopy();

    for (size_t i = 1; i <= num_hosts; i++) {
      top_host_filter->SetBoolKey(optimization_guide::HashHostForDictionary(
                                      base::StringPrintf("domain%zu.com", i)),
                                  true);
    }
    pref_service_->Set(optimization_guide::prefs::kHintsFetcherTopHostBlocklist,
                       *top_host_filter);
  }

  void AddHostToBlockList(const std::string& host) {
    std::unique_ptr<base::DictionaryValue> top_host_filter =
        pref_service_
            ->GetDictionary(
                optimization_guide::prefs::kHintsFetcherTopHostBlocklist)
            ->CreateDeepCopy();
    top_host_filter->SetBoolKey(optimization_guide::HashHostForDictionary(host),
                                true);
    pref_service_->Set(optimization_guide::prefs::kHintsFetcherTopHostBlocklist,
                       *top_host_filter);
  }

  void SimulateUniqueNavigationsToTopHosts(size_t num_hosts) {
    for (size_t i = 1; i <= num_hosts; i++) {
      SimulateNavigation(GURL(base::StringPrintf("https://domain%zu.com", i)));
    }
  }

  void SimulateNavigation(GURL url) {
    std::unique_ptr<content::MockNavigationHandle> test_handle_ =
        std::make_unique<content::MockNavigationHandle>(url, main_rfh());
    OptimizationGuideTopHostProvider::MaybeUpdateTopHostBlocklist(
        test_handle_.get());
  }

  void RemoveHostsFromBlocklist(size_t num_hosts_navigated) {
    std::unique_ptr<base::DictionaryValue> top_host_filter =
        pref_service_
            ->GetDictionary(
                optimization_guide::prefs::kHintsFetcherTopHostBlocklist)
            ->CreateDeepCopy();

    for (size_t i = 1; i <= num_hosts_navigated; i++) {
      top_host_filter->RemoveKey(optimization_guide::HashHostForDictionary(
          base::StringPrintf("domain%zu.com", i)));
    }
    pref_service_->Set(optimization_guide::prefs::kHintsFetcherTopHostBlocklist,
                       *top_host_filter);
  }

  void SetTopHostBlocklistState(
      optimization_guide::prefs::HintsFetcherTopHostBlocklistState
          blocklist_state) {
    profile()->GetPrefs()->SetInteger(
        optimization_guide::prefs::kHintsFetcherTopHostBlocklistState,
        static_cast<int>(blocklist_state));
  }

  optimization_guide::prefs::HintsFetcherTopHostBlocklistState
  GetCurrentTopHostBlocklistState() {
    return static_cast<
        optimization_guide::prefs::HintsFetcherTopHostBlocklistState>(
        pref_service_->GetInteger(
            optimization_guide::prefs::kHintsFetcherTopHostBlocklistState));
  }

  OptimizationGuideTopHostProvider* top_host_provider() {
    return top_host_provider_.get();
  }

  base::SimpleTestClock test_clock_;

 private:
  std::unique_ptr<OptimizationGuideTopHostProvider> top_host_provider_;
  std::unique_ptr<data_reduction_proxy::DataReductionProxyTestContext>
      drp_test_context_;
  site_engagement::SiteEngagementService* service_;
  PrefService* pref_service_;
};

class OptimizationGuideTopHostProviderRemoteOptimizationEnabledTest
    : public OptimizationGuideTopHostProviderTest {
 public:
  OptimizationGuideTopHostProviderRemoteOptimizationEnabledTest() {
    // This needs to be run before any tasks run on other threads that check if
    // a feature is enabled, to avoid tsan error flakes.
    scoped_feature_list_.InitAndEnableFeature(
        {optimization_guide::features::kRemoteOptimizationGuideFetching});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OptimizationGuideTopHostProviderRemoteOptimizationEnabledTest,
       CreateIfAllowedNonDataSaverUser) {
  SetDataSaverEnabled(false);
  ASSERT_FALSE(OptimizationGuideTopHostProvider::CreateIfAllowed(profile()));
}

TEST_F(OptimizationGuideTopHostProviderRemoteOptimizationEnabledTest,
       CreateIfAllowedDataSaverUser) {
  SetDataSaverEnabled(true);

  ASSERT_TRUE(OptimizationGuideTopHostProvider::CreateIfAllowed(profile()));
}

class OptimizationGuideTopHostProviderRemoteOptimizationDisabledTest
    : public OptimizationGuideTopHostProviderTest {
 public:
  OptimizationGuideTopHostProviderRemoteOptimizationDisabledTest() {
    // This needs to be run before any tasks run on other threads that check if
    // a feature is enabled, to avoid tsan error flakes.
    scoped_feature_list_.InitAndDisableFeature(
        {optimization_guide::features::kRemoteOptimizationGuideFetching});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OptimizationGuideTopHostProviderRemoteOptimizationDisabledTest,
       CreateIfAllowedDataSaverUserButHintsFetchingNotEnabled) {
  SetDataSaverEnabled(true);

  ASSERT_FALSE(OptimizationGuideTopHostProvider::CreateIfAllowed(profile()));
}

TEST_F(OptimizationGuideTopHostProviderTest, GetTopHostsMaxSites) {
  SetIsPermittedToUseTopHostProvider(true);

  SetTopHostBlocklistState(optimization_guide::prefs::
                               HintsFetcherTopHostBlocklistState::kInitialized);
  size_t engaged_hosts = 5;
  AddEngagedHosts(engaged_hosts);

  EXPECT_EQ(engaged_hosts, top_host_provider()->GetTopHosts().size());
}

TEST_F(OptimizationGuideTopHostProviderTest,
       GetTopHostsFiltersPrivacyBlockedlistedHosts) {
  SetIsPermittedToUseTopHostProvider(true);

  SetTopHostBlocklistState(optimization_guide::prefs::
                               HintsFetcherTopHostBlocklistState::kInitialized);
  size_t engaged_hosts = 5;
  size_t num_hosts_blocklisted = 2;
  AddEngagedHosts(engaged_hosts);

  PopulateTopHostBlocklist(num_hosts_blocklisted);

  std::vector<std::string> hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(hosts.size(), engaged_hosts - num_hosts_blocklisted);
}

TEST_F(OptimizationGuideTopHostProviderTest,
       GetTopHostsInitializeBlocklistState) {
  SetIsPermittedToUseTopHostProvider(true);

  EXPECT_EQ(GetCurrentTopHostBlocklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlocklistState::
                kNotInitialized);
  size_t engaged_hosts = 5;
  AddEngagedHosts(engaged_hosts);

  std::vector<std::string> hosts = top_host_provider()->GetTopHosts();
  // On initialization, GetTopHosts should always return zero hosts.
  EXPECT_EQ(hosts.size(), 0u);
  EXPECT_EQ(GetCurrentTopHostBlocklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlocklistState::
                kInitialized);
}

TEST_F(OptimizationGuideTopHostProviderTest,
       GetTopHostsBlocklistStateNotInitializedToInitialized) {
  SetIsPermittedToUseTopHostProvider(true);

  size_t engaged_hosts = 5;
  size_t num_hosts_blocklisted = 5;
  AddEngagedHosts(engaged_hosts);

  std::vector<std::string> hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(hosts.size(), 0u);

  // Blocklist should now have items removed.
  size_t num_navigations = 2;
  RemoveHostsFromBlocklist(num_navigations);

  hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(hosts.size(),
            engaged_hosts - (num_hosts_blocklisted - num_navigations));
  EXPECT_EQ(GetCurrentTopHostBlocklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlocklistState::
                kInitialized);
}

TEST_F(OptimizationGuideTopHostProviderTest,
       GetTopHostsBlocklistStateNotInitializedToEmpty) {
  SetIsPermittedToUseTopHostProvider(true);

  size_t engaged_hosts = 5;
  size_t num_hosts_blocklisted = 5;
  AddEngagedHosts(engaged_hosts);

  std::vector<std::string> hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(hosts.size(), 0u);

  // Blocklist should now have items removed.
  size_t num_navigations = 5;
  RemoveHostsFromBlocklist(num_navigations);

  hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(hosts.size(),
            engaged_hosts - (num_hosts_blocklisted - num_navigations));
  EXPECT_EQ(
      GetCurrentTopHostBlocklistState(),
      optimization_guide::prefs::HintsFetcherTopHostBlocklistState::kEmpty);
}

TEST_F(OptimizationGuideTopHostProviderTest,
       MaybeUpdateTopHostBlocklistNavigationsOnBlocklist) {
  SetIsPermittedToUseTopHostProvider(true);

  size_t engaged_hosts = 5;
  size_t num_top_hosts = 3;
  AddEngagedHosts(engaged_hosts);

  // The blocklist should be populated on the first request.
  std::vector<std::string> hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(hosts.size(), 0u);

  // Navigate to some engaged hosts to trigger their removal from the top host
  // blocklist.
  SimulateUniqueNavigationsToTopHosts(num_top_hosts);

  hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(hosts.size(), num_top_hosts);
}

TEST_F(OptimizationGuideTopHostProviderTest,
       MaybeUpdateTopHostBlocklistEmptyBlocklist) {
  SetIsPermittedToUseTopHostProvider(true);

  size_t engaged_hosts = 5;
  size_t num_top_hosts = 5;
  AddEngagedHosts(engaged_hosts);

  std::vector<std::string> hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(hosts.size(), 0u);

  SimulateUniqueNavigationsToTopHosts(num_top_hosts);

  EXPECT_EQ(
      GetCurrentTopHostBlocklistState(),
      optimization_guide::prefs::HintsFetcherTopHostBlocklistState::kEmpty);

  hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(hosts.size(), num_top_hosts);
}

TEST_F(OptimizationGuideTopHostProviderTest,
       HintsFetcherTopHostBlocklistNonHTTPOrHTTPSHost) {
  SetIsPermittedToUseTopHostProvider(true);

  size_t engaged_hosts = 5;
  size_t num_hosts_blocklisted = 5;
  GURL http_url = GURL("http://anyscheme.com");
  GURL file_url = GURL("file://anyscheme.com");
  AddEngagedHosts(engaged_hosts);
  AddEngagedHost(http_url, 5);

  PopulateTopHostBlocklist(num_hosts_blocklisted);
  AddHostToBlockList(http_url.host());

  SetTopHostBlocklistState(optimization_guide::prefs::
                               HintsFetcherTopHostBlocklistState::kInitialized);

  // A Non HTTP/HTTPS navigation should not remove a host from the blocklist.
  SimulateNavigation(file_url);
  std::vector<std::string> hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(hosts.size(), 0u);
  // The host, anyscheme.com, should still be on the blocklist.
  EXPECT_TRUE(IsHostBlocklisted(file_url.host()));

  // HTTP/HTTPS navigation should remove the host from the blocklist and then
  // be returned.
  SimulateNavigation(http_url);
  hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(hosts.size(), 1u);

  EXPECT_FALSE(IsHostBlocklisted(http_url.host()));
}

TEST_F(OptimizationGuideTopHostProviderTest,
       IntializeTopHostBlocklistWithMaxTopSites) {
  SetIsPermittedToUseTopHostProvider(true);

  size_t engaged_hosts =
      optimization_guide::features::MaxHintsFetcherTopHostBlocklistSize() + 1;
  AddEngagedHosts(engaged_hosts);

  // Blocklist should be populated on the first request.
  std::vector<std::string> hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(hosts.size(), 0u);

  hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(
      hosts.size(),
      engaged_hosts -
          optimization_guide::features::MaxHintsFetcherTopHostBlocklistSize());
  EXPECT_EQ(GetCurrentTopHostBlocklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlocklistState::
                kInitialized);

  // The last host has the most engagement points so it will be blocklisted. The
  // first host has the lowest engagement score and will not be blocklisted
  // because it is not in the top MaxHintsFetcherTopHostBlocklistSize engaged
  // hosts by engagement score.
  EXPECT_TRUE(IsHostBlocklisted(base::StringPrintf(
      "domain%zu.com",
      optimization_guide::features::MaxHintsFetcherTopHostBlocklistSize())));
  EXPECT_FALSE(IsHostBlocklisted(base::StringPrintf("domain%u.com", 1u)));
}

TEST_F(OptimizationGuideTopHostProviderTest,
       TopHostsFilteredByEngagementThreshold) {
  SetIsPermittedToUseTopHostProvider(true);

  size_t engaged_hosts =
      optimization_guide::features::MaxHintsFetcherTopHostBlocklistSize() + 1;

  AddEngagedHosts(engaged_hosts);
  // Add two hosts with very low engagement scores that should not be returned
  // by the top host provider. Must be negative in order to have a low enough
  // score onced added to the service after the bonus scores included by the
  // site engagement service.
  AddEngagedHost(GURL("https://lowengagement1.com"), -0.5);
  AddEngagedHost(GURL("https://lowengagement2.com"), -0.5);

  // Blocklist should be populated on the first request. Set the count of
  // desired
  std::vector<std::string> hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(hosts.size(), 0u);

  hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(1u, hosts.size());
  EXPECT_EQ(GetCurrentTopHostBlocklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlocklistState::
                kInitialized);

  // The hosts with engagement scores below the minimum threshold should not be
  // returned.
  EXPECT_EQ(std::find(hosts.begin(), hosts.end(), "lowengagement1.com"),
            hosts.end());
  EXPECT_EQ(std::find(hosts.begin(), hosts.end(), "lowengagement2.com"),
            hosts.end());

  // Advance the clock by more than DurationApplyLowEngagementScoreThreshold().
  // top_host_provider() should also return hosts with low engagement score.
  test_clock_.Advance(
      optimization_guide::features::DurationApplyLowEngagementScoreThreshold() +
      base::TimeDelta::FromDays(1));
  AddEngagedHost(GURL("https://lowengagement3.com"), 1);
  AddEngagedHost(GURL("https://lowengagement4.com"), 1);

  hosts = top_host_provider()->GetTopHosts();
  // Four hosts lowengagement[1-4] should now be present in |hosts|.
  EXPECT_EQ(
      engaged_hosts + 4 -
          optimization_guide::features::MaxHintsFetcherTopHostBlocklistSize(),
      hosts.size());
  EXPECT_EQ(GetCurrentTopHostBlocklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlocklistState::
                kInitialized);

  // The hosts with engagement scores below the minimum threshold should not be
  // returned.
  EXPECT_NE(std::find(hosts.begin(), hosts.end(), "lowengagement1.com"),
            hosts.end());
  EXPECT_NE(std::find(hosts.begin(), hosts.end(), "lowengagement2.com"),
            hosts.end());
  EXPECT_NE(std::find(hosts.begin(), hosts.end(), "lowengagement3.com"),
            hosts.end());
  EXPECT_NE(std::find(hosts.begin(), hosts.end(), "lowengagement4.com"),
            hosts.end());
}

TEST_F(OptimizationGuideTopHostProviderTest,
       TopHostsFilteredByEngagementThreshold_NumPoints) {
  SetIsPermittedToUseTopHostProvider(true);

  size_t engaged_hosts =
      optimization_guide::features::MaxHintsFetcherTopHostBlocklistSize() + 1;

  AddEngagedHostsWithPoints(engaged_hosts, 15);
  // Add two hosts with engagement scores less than 15. These hosts should not
  // be returned by the top host provider because the minimum engagement score
  // threshold is set to a value larger than 5.
  AddEngagedHost(GURL("https://lowengagement1.com"), 5);
  AddEngagedHost(GURL("https://lowengagement2.com"), 5);

  // Before the blocklist is populated, the threshold should have a default
  // value.
  EXPECT_EQ(2, GetHintsFetcherTopHostBlocklistMinimumEngagementScore());

  // Blocklist should be populated on the first request.
  std::vector<std::string> hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(hosts.size(), 0u);

  hosts = top_host_provider()->GetTopHosts();
  EXPECT_NEAR(GetHintsFetcherTopHostBlocklistMinimumEngagementScore(),
              GetHintsFetcherTopHostBlocklistMinimumEngagementScore(), 1);
  EXPECT_EQ(3u, hosts.size());
  EXPECT_EQ(GetCurrentTopHostBlocklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlocklistState::
                kInitialized);
  EXPECT_NE(std::find(hosts.begin(), hosts.end(), "lowengagement1.com"),
            hosts.end());
  EXPECT_NE(std::find(hosts.begin(), hosts.end(), "lowengagement2.com"),
            hosts.end());
}

TEST_F(OptimizationGuideTopHostProviderTest,
       TopHostsFilteredByEngagementThreshold_LowScore) {
  SetIsPermittedToUseTopHostProvider(true);

  size_t engaged_hosts =
      optimization_guide::features::MaxHintsFetcherTopHostBlocklistSize() - 2;

  AddEngagedHostsWithPoints(engaged_hosts, 2);

  // Blocklist should be populated on the first request. Set the count of
  // desired
  std::vector<std::string> hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(hosts.size(), 0u);

  // Add two hosts with very low engagement scores. These hosts should be
  // returned by top_host_provider() even with low score.
  EXPECT_EQ(-1, GetHintsFetcherTopHostBlocklistMinimumEngagementScore());
  AddEngagedHost(GURL("https://lowengagement1.com"), 1);
  AddEngagedHost(GURL("https://lowengagement2.com"), 1);

  hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(2u, hosts.size());
  EXPECT_EQ(GetCurrentTopHostBlocklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlocklistState::
                kInitialized);
  EXPECT_NE(std::find(hosts.begin(), hosts.end(), "lowengagement1.com"),
            hosts.end());
  EXPECT_NE(std::find(hosts.begin(), hosts.end(), "lowengagement2.com"),
            hosts.end());
}

TEST_F(OptimizationGuideTopHostProviderTest,
       GetTopHosts_UserChangesPermissionsMidSession) {
  SetIsPermittedToUseTopHostProvider(true);

  size_t engaged_hosts =
      optimization_guide::features::MaxHintsFetcherTopHostBlocklistSize() - 2;

  AddEngagedHostsWithPoints(engaged_hosts, 2);

  // Blocklist should be populated on the first request. Set the count of
  // desired
  std::vector<std::string> hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(hosts.size(), 0u);

  // Add two hosts with very low engagement scores. These hosts should be
  // returned by top_host_provider().
  EXPECT_EQ(-1, GetHintsFetcherTopHostBlocklistMinimumEngagementScore());
  AddEngagedHost(GURL("https://lowengagement1.com"), 1);
  AddEngagedHost(GURL("https://lowengagement2.com"), 1);

  hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(2u, hosts.size());

  // Now, toggle the setting so that the user cannot fetch hints.
  SetIsPermittedToUseTopHostProvider(false);

  hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(hosts.size(), 0u);
}

TEST_F(OptimizationGuideTopHostProviderTest,
       MaybeUpdateTopHostBlocklist_UserChangesPermissionsMidSession) {
  SetIsPermittedToUseTopHostProvider(true);
  AddEngagedHost(GURL("https://someengagement.com"), 1);
  AddEngagedHost(GURL("https://someengagement2.com"), 1);

  // Make sure that the blocklist is initialized in some way.
  SetTopHostBlocklistState(optimization_guide::prefs::
                               HintsFetcherTopHostBlocklistState::kInitialized);

  // Now, toggle the setting so that the user cannot fetch hints.
  SetIsPermittedToUseTopHostProvider(false);
  // Make sure blocklist state is set to uninitialized.
  SimulateNavigation(GURL("https://whatever.com"));
  EXPECT_EQ(GetCurrentTopHostBlocklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlocklistState::
                kNotInitialized);

  // Now, toggle setting again. Make sure everything still works as normal.
  SetIsPermittedToUseTopHostProvider(true);

  std::vector<std::string> hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(0u, hosts.size());
  EXPECT_NE(GetCurrentTopHostBlocklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlocklistState::
                kNotInitialized);

  AddEngagedHost(GURL("https://newfavoritehost.com"), 5);
  hosts = top_host_provider()->GetTopHosts();
  EXPECT_EQ(1u, hosts.size());
  EXPECT_NE(std::find(hosts.begin(), hosts.end(), "newfavoritehost.com"),
            hosts.end());
}
