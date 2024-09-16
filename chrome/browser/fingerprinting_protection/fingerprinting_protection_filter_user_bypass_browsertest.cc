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
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
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
  base::HistogramTester histogram_tester;
  // TODO(https://crbug.com/358371545): Test console messaging for subframe
  // blocking once its implementation is resolved.
  GURL url(GetTestUrl(kTestFrameSetPath));

  // Disallow loading child frame documents that in turn would end up
  // loading included_script.js, unless the document is loaded from an allowed
  // (not in the blocklist) domain.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  const std::vector<const char*> kSubframeNames{"one", "two"};
  const std::vector<bool> kExpectOnlySecondSubframe{false, true};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Simulate an explicit allowlisting via content settings.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  AllowlistViaContentSettings(settings_map, url);

  // Re-do the navigation after User Bypass is enabled and assert all frames are
  // loaded despite the blocklist matching on the deactivated filter.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  histogram_tester.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::URL_ALLOWLISTED, 1);
  histogram_tester.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kDisabled, 1);
  // TODO(https://crbug.com/366267410): Add testing for UKM.

  const std::vector<bool> kExpectAllSubframes{true, true};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);
}

IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterEnabledInIncognitoBrowserTest,
    ActiveFilter_AllowsOnUserBypassException) {
  base::HistogramTester histogram_tester;
  // TODO(https://crbug.com/358371545): Test console messaging for subframe
  // blocking once its implementation is resolved.
  GURL url(GetTestUrl(kTestFrameSetPath));

  // Disallow loading child frame documents that in turn would end up
  // loading included_script.js, unless the document is loaded from an allowed
  // (not in the blocklist) domain.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  const std::vector<const char*> kSubframeNames{"one", "two"};
  const std::vector<bool> kExpectOnlySecondSubframe{false, true};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Simulate an explicit allowlisting via content settings.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  AllowlistViaContentSettings(settings_map, url);

  // Re-do the navigation after User Bypass is enabled and assert all frames are
  // loaded despite the blocklist matching on the deactivated filter.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  histogram_tester.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::URL_ALLOWLISTED, 1);
  histogram_tester.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kEnabled, 1);

  const std::vector<bool> kExpectAllSubframes{true, true};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);
}

IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterDryRunBrowserTest,
                       ActiveFilter_UserByPassException_DoesNotBlock) {
  base::HistogramTester histogram_tester;
  // TODO(https://crbug.com/358371545): Test console messaging for subframe
  // blocking once its implementation is resolved.
  GURL url(GetTestUrl(kTestFrameSetPath));

  // Would disallow loading child frame documents that in turn would end up
  // loading included_script.js. However, in dry run mode, all frames are
  // expected as nothing is blocked.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  const std::vector<const char*> kSubframeNames{"one", "two"};
  const std::vector<bool> kExpectAllSubframes{true, true};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  // Simulate an explicit allowlisting via content settings.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  AllowlistViaContentSettings(settings_map, url);

  // Re-do the navigation after User Bypass is enabled and verify all frames are
  // still loaded as bypass exception should have no impact.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // TODO(https://crbug.com/366267410): Add testing for UKM.

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
           {"enable_on_3pc_blocked", "true"}}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterEnabled3PCookiesBlockedBrowserTest,
    UserBypassException_ThirdPartyCookiesBlockingPrefOn_DoNotActivateFilter) {
  base::HistogramTester histogram_tester;
  // TODO(https://crbug.com/358371545): Test console messaging for subframe
  // blocking once its implementation is resolved.
  GURL url(GetTestUrl(kTestFrameSetPath));

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

  // Assert that FPF is not activated due to the user bypass exception.
  const std::vector<const char*> kSubframeNames{"one", "two"};
  const std::vector<bool> kExpectAllSubframes{true, true};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  histogram_tester.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::URL_ALLOWLISTED, 1);
  histogram_tester.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kDisabled, 1);
}

IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterEnabled3PCookiesBlockedBrowserTest,
    ThirdPartyCookiesBlockingPrefOff_DoNotActivateFilter) {
  base::HistogramTester histogram_tester;
  // TODO(https://crbug.com/358371545): Test console messaging for subframe
  // blocking once its implementation is resolved.
  GURL url(GetTestUrl(kTestFrameSetPath));

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

  // Assert that FPF is not activated because conditions are not met for
  // enabling the filter.
  const std::vector<const char*> kSubframeNames{"one", "two"};
  const std::vector<bool> kExpectAllSubframes{true, true};
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
