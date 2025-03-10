// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/fingerprinting_protection/fingerprinting_protection_filter_browser_test_harness.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_page_activation_throttle.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/prefs/pref_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace fingerprinting_protection_filter {

void AllowlistViaContentSettings(HostContentSettingsMap* settings_map,
                                 const GURL& url) {
  settings_map->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURLToSchemefulSitePattern(url),
      ContentSettingsType::TRACKING_PROTECTION, CONTENT_SETTING_ALLOW,
      content_settings::ContentSettingConstraints());
}

IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterBrowserTest,
                       ActiveFilter_AllowsOnUserBypassException) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  // TODO(https://crbug.com/358371545): Test console messaging for subframe
  // blocking once its implementation is resolved.
  GURL url(GetTestUrl(kMultiPlatformTestFrameSetPath));

  // Disallow loading child frame documents that in turn would end up
  // loading included_script.js, unless the document is loaded from an allowed
  // (not in the blocklist) domain.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  NavigateSubframesToCrossOriginSite();

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Check that activated UKMs logged, one per load with "included_script.html",
  // frame "one" and it's child.
  ExpectFpfActivatedUkms(test_ukm_recorder, 2u,
                         /*is_dry_run=*/false);

  // Simulate an explicit allowlisting via content settings.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  AllowlistViaContentSettings(settings_map, url);

  // Re-do the navigation after User Bypass is enabled and assert all frames are
  // loaded despite the blocklist matching on the deactivated filter.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  NavigateSubframesToCrossOriginSite();

  histogram_tester.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::URL_ALLOWLISTED, 1);
  histogram_tester.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kDisabled, 1);

  // +0 activated UKMs, as User Bypass grants exceptions.
  ExpectFpfActivatedUkms(test_ukm_recorder, 2u,
                         /*is_dry_run=*/false);

  // Check that exception UKM is logged as User Bypass is applied.
  ExpectFpfExceptionUkms(test_ukm_recorder, 1u,
                         static_cast<int64_t>(ExceptionSource::USER_BYPASS));

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);
}

IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterEnabledInIncognitoBrowserTest,
    ActiveFilter_AllowsOnUserBypassException) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  // TODO(https://crbug.com/358371545): Test console messaging for subframe
  // blocking once its implementation is resolved.
  GURL url(GetTestUrl(kMultiPlatformTestFrameSetPath));

  // Disallow loading child frame documents that in turn would end up
  // loading included_script.js, unless the document is loaded from an allowed
  // (not in the blocklist) domain.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  NavigateSubframesToCrossOriginSite();

  histogram_tester.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::ACTIVATED, 1);
  histogram_tester.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kEnabled, 1);

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Check that activated UKMs logged, one per load with "included_script.html",
  // frame "one" and it's child.
  ExpectFpfActivatedUkms(test_ukm_recorder, 2u,
                         /*is_dry_run=*/false);

  // Simulate an explicit allowlisting via content settings.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  AllowlistViaContentSettings(settings_map, url);

  // Re-do the navigation after User Bypass is enabled and assert all frames are
  // loaded despite the blocklist matching on the deactivated filter.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  NavigateSubframesToCrossOriginSite();

  histogram_tester.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::URL_ALLOWLISTED, 1);
  histogram_tester.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kDisabled, 1);

  // +0 activated UKMs, as User Bypass grants exceptions.
  ExpectFpfActivatedUkms(test_ukm_recorder, 2u,
                         /*is_dry_run=*/false);

  // Check that exception UKM is logged as User Bypass is applied.
  ExpectFpfExceptionUkms(test_ukm_recorder, 1u,
                         static_cast<int64_t>(ExceptionSource::USER_BYPASS));

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);
}

IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterDryRunBrowserTest,
                       ActiveFilter_UserByPassException_DoesNotBlock) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  // TODO(https://crbug.com/358371545): Test console messaging for subframe
  // blocking once its implementation is resolved.
  GURL url(GetTestUrl(kMultiPlatformTestFrameSetPath));

  // Would disallow loading child frame documents that in turn would end up
  // loading included_script.js. However, in dry run mode, all frames are
  // expected as nothing is blocked.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  NavigateSubframesToCrossOriginSite();

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  // Check that UKM contains entries for each subframe with
  // "included_script.html" ("one" and "two").
  ExpectFpfActivatedUkms(test_ukm_recorder, 2u,
                         /*is_dry_run=*/true);

  // Simulate an explicit allowlisting via content settings.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  AllowlistViaContentSettings(settings_map, url);

  // Re-do the navigation after User Bypass is enabled and verify all frames are
  // still loaded as bypass exception should have no impact in dry_run mode.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  NavigateSubframesToCrossOriginSite();

  // +2 UKM logs as User Bypass has no impact in dry_run mode.
  ExpectFpfActivatedUkms(test_ukm_recorder, 4u,
                         /*is_dry_run=*/true);

  // Check that no exceptions UKMs are logged, as User Bypass has no impact in
  // dry_run mode.
  ExpectNoFpfExceptionUkms(test_ukm_recorder);

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);
}

class FingerprintingProtectionFilterEnabled3PCookiesBlockedBrowserTest
    : public FingerprintingProtectionFilterBrowserTest {
 public:
  FingerprintingProtectionFilterEnabled3PCookiesBlockedBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kEnableFingerprintingProtectionFilter,
          {{"activation_level", "enabled"},
           {"enable_only_if_3pc_blocked", "true"}}}},
        /*disabled_features=*/{});
  }

 protected:
  void SetUpOnMainThread() override {
    FingerprintingProtectionFilterBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterEnabled3PCookiesBlockedBrowserTest,
    UserBypassException_ThirdPartyCookiesBlockingPrefOn_DoNotActivateFilter) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  // TODO(https://crbug.com/358371545): Test console messaging for subframe
  // blocking once its implementation is resolved.
  GURL url(GetTestUrl(kMultiPlatformTestFrameSetPath));

  // Disallow loading child frame documents that in turn would end up
  // loading included_script.js, unless the document is loaded from an allowed
  // (not in the blocklist) domain.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.html"));

  // Simulate enabling blocking third party cookies through prefs.
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));

  // Simulate an explicit allowlisting via content settings.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  AllowlistViaContentSettings(settings_map, url);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  NavigateSubframesToCrossOriginSite();

  // Assert that FPF is not activated due to the user bypass exception.

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  histogram_tester.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::URL_ALLOWLISTED, 1);
  histogram_tester.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kDisabled, 1);

  // Check test UKM recorder contains no FingerprintingProtection ukm event,
  // i.e. no resource was (or would be) blocked
  EXPECT_EQ(0u, test_ukm_recorder
                    .GetEntriesByName(
                        ukm::builders::FingerprintingProtection::kEntryName)
                    .size());

  // Check that exception UKM is logged as User Bypass is applied..
  ExpectFpfExceptionUkms(test_ukm_recorder, 1u,
                         static_cast<int64_t>(ExceptionSource::USER_BYPASS));
}

IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterEnabled3PCookiesBlockedBrowserTest,
    ThirdPartyCookiesBlockingPrefOff_DoNotActivateFilter) {
  base::HistogramTester histogram_tester;
  // TODO(https://crbug.com/358371545): Test console messaging for subframe
  // blocking once its implementation is resolved.
  GURL url(GetTestUrl(kMultiPlatformTestFrameSetPath));

  // Disallow loading child frame documents that in turn would end up
  // loading included_script.js, unless the document is loaded from an allowed
  // (not in the blocklist) domain.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.html"));

  // Simulate 3P cookies not being disabled through prefs.
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOff));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  NavigateSubframesToCrossOriginSite();

  // Assert that FPF is not activated because conditions are not met for
  // enabling the filter.

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  histogram_tester.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET, 1);
  histogram_tester.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kDisabled, 1);
}

}  // namespace fingerprinting_protection_filter
