// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FINGERPRINTING_PROTECTION_FINGERPRINTING_PROTECTION_FILTER_BROWSER_TEST_HARNESS_H_
#define CHROME_BROWSER_FINGERPRINTING_PROTECTION_FINGERPRINTING_PROTECTION_FILTER_BROWSER_TEST_HARNESS_H_

#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace fingerprinting_protection_filter {

// ================= kMultiPlatformTestFrameSetPath Constants ==================

// iframe names defined in `kMultiPlatformTestFrameSetPath`.
const std::vector<const char*> kSubframeNames{"one", "two", "three"};
const std::vector<bool> kExpectAllSubframes{true, true, true};
const std::vector<bool> kExpectOnlySecondSubframe{false, true, false};

// ================ FingerprintingProtectionFilterBrowserTest =================

// Browser tests for the Fingerprinting Protection Filter component. Due to some
// shared functionality with the Subresource Filter, these tests use a
// Subresource Filter Browser Test harness as a base class.
class FingerprintingProtectionFilterBrowserTest
    : public subresource_filter::SubresourceFilterSharedBrowserTest {
 public:
  FingerprintingProtectionFilterBrowserTest();

  FingerprintingProtectionFilterBrowserTest(
      const FingerprintingProtectionFilterBrowserTest&) = delete;
  FingerprintingProtectionFilterBrowserTest& operator=(
      const FingerprintingProtectionFilterBrowserTest&) = delete;

  ~FingerprintingProtectionFilterBrowserTest() override;

  // The path to a multi-frame document used for browser tests that run on
  // Android and Desktop.
  static constexpr const char kMultiPlatformTestFrameSetPath[] =
      "/frame_set.html";

  // PageLoad histogram names.
  static constexpr const char kSubresourceLoadsTotalForPage[] =
      "FingerprintingProtection.PageLoad.NumSubresourceLoads.Total";
  static constexpr const char kSubresourceLoadsEvaluatedForPage[] =
      "FingerprintingProtection.PageLoad.NumSubresourceLoads.Evaluated";
  static constexpr const char kSubresourceLoadsMatchedRulesForPage[] =
      "FingerprintingProtection.PageLoad.NumSubresourceLoads.MatchedRules";
  static constexpr const char kSubresourceLoadsDisallowedForPage[] =
      "FingerprintingProtection.PageLoad.NumSubresourceLoads.Disallowed";

  // Incognito PageLoad histogram names.
  static constexpr const char kSubresourceLoadsTotalForIncognitoPage[] =
      "FingerprintingProtection.PageLoad.NumSubresourceLoads.Total.Incognito";
  static constexpr const char kSubresourceLoadsEvaluatedForIncognitoPage[] =
      "FingerprintingProtection.PageLoad.NumSubresourceLoads."
      "Evaluated.Incognito";
  static constexpr const char kSubresourceLoadsMatchedRulesForIncognitoPage[] =
      "FingerprintingProtection.PageLoad.NumSubresourceLoads."
      "MatchedRules.Incognito";
  static constexpr const char kSubresourceLoadsDisallowedForIncognitoPage[] =
      "FingerprintingProtection.PageLoad.NumSubresourceLoads."
      "Disallowed.Incognito";

  // Names of the performance measurement histograms.
  static constexpr const char kEvaluationTotalWallDurationForPage[] =
      "FingerprintingProtection.PageLoad.SubresourceEvaluation."
      "TotalWallDuration";
  static constexpr const char kEvaluationTotalCPUDurationForPage[] =
      "FingerprintingProtection.PageLoad.SubresourceEvaluation."
      "TotalCPUDuration";
  static constexpr const char kSubresourceLoadEvaluationWallDuration[] =
      "FingerprintingProtection.SubresourceLoad.Evaluation.WallDuration";
  static constexpr const char kSubresourceLoadEvaluationCpuDuration[] =
      "FingerprintingProtection.SubresourceLoad.Evaluation.CPUDuration";

  // Names of the performance measurement histograms for Incognito.
  static constexpr const char kEvaluationTotalWallDurationForIncognitoPage[] =
      "FingerprintingProtection.PageLoad.SubresourceEvaluation."
      "TotalWallDuration.Incognito";
  static constexpr const char kEvaluationTotalCPUDurationForIncognitoPage[] =
      "FingerprintingProtection.PageLoad.SubresourceEvaluation."
      "TotalCPUDuration.Incognito";

 protected:
  // Override to use a custom `embedder_base` url.
  GURL GetTestUrl(const std::string& relative_url) const override;

  // Same as GetTestUrl, but uses a cross-origin base url, `cross-origin.test`.
  GURL GetCrossSiteTestUrl(const std::string& relative_url) const;

  void SetUpOnMainThread() override;

  void SetRulesetToDisallowURLsWithPathSuffix(const std::string& suffix);

  void SetRulesetToDisallowURLsWithSubstring(const std::string& substring);

  void SetRulesetWithRules(const std::vector<proto::UrlRule>& rules);

  void AssertUrlContained(const GURL& full_url, const GURL& sub_url);

  bool NavigateToDestination(const GURL& url);

  // Check that UKM, logged only when a resource's load policy is either
  // `DISALLOW` or `WOULD_DISALLOW`, contains `expected_count` log entries and
  // `is_dry_run` metric value.
  void ExpectFpfActivatedUkms(const ukm::TestAutoSetUkmRecorder& recorder,
                              const unsigned long& expected_count,
                              bool is_dry_run);

  // Check that UKM, logged only when an exception to an activated filter is
  // found, is not logged; i.e. an exception is not found.
  void ExpectNoFpfExceptionUkms(const ukm::TestAutoSetUkmRecorder& recorder);

  // Check that UKM, logged only when an exception to an activated filter is
  // found, is logged `expected_count` number of times, with the source metric
  // matching `expected_source`.
  void ExpectFpfExceptionUkms(const ukm::TestAutoSetUkmRecorder& recorder,
                              const unsigned long& expected_count,
                              const int64_t& expected_source);

  // Navigate all subframes of `kMultiPlatformTestFrameSetPath` to cross-origin
  // sites, i.e. `cross-origin.test/`.
  void NavigateSubframesToCrossOriginSite();

  // Update the src of included_script.js with the input.
  void UpdateIncludedScriptSource(const GURL& url);

  // Navigate subframes in `kMultiPlatformTestFrameSetPath` and load scripts in
  // 3p (cross-origin.test) contexts.
  void NavigateMultiFrameSubframesAndLoad3pScripts();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  subresource_filter::testing::ScopedSubresourceFilterConfigurator
      scoped_configuration_;

  TestRulesetCreator ruleset_creator_;
};

class FingerprintingProtectionFilterDryRunBrowserTest
    : public FingerprintingProtectionFilterBrowserTest {
 public:
  FingerprintingProtectionFilterDryRunBrowserTest();

  FingerprintingProtectionFilterDryRunBrowserTest(
      const FingerprintingProtectionFilterDryRunBrowserTest&) = delete;
  FingerprintingProtectionFilterDryRunBrowserTest& operator=(
      const FingerprintingProtectionFilterDryRunBrowserTest&) = delete;

  ~FingerprintingProtectionFilterDryRunBrowserTest() override;

 protected:
  void SetUpOnMainThread() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class FingerprintingProtectionFilterEnabledInIncognitoBrowserTest
    : public FingerprintingProtectionFilterBrowserTest {
 public:
  FingerprintingProtectionFilterEnabledInIncognitoBrowserTest();

  FingerprintingProtectionFilterEnabledInIncognitoBrowserTest(
      const FingerprintingProtectionFilterEnabledInIncognitoBrowserTest&) =
      delete;
  FingerprintingProtectionFilterEnabledInIncognitoBrowserTest& operator=(
      const FingerprintingProtectionFilterEnabledInIncognitoBrowserTest&) =
      delete;

  ~FingerprintingProtectionFilterEnabledInIncognitoBrowserTest() override;

 protected:
  void SetUpOnMainThread() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class FingerprintingProtectionFilterDisabledBrowserTest
    : public FingerprintingProtectionFilterBrowserTest {
 public:
  FingerprintingProtectionFilterDisabledBrowserTest();

  FingerprintingProtectionFilterDisabledBrowserTest(
      const FingerprintingProtectionFilterDisabledBrowserTest&) = delete;
  FingerprintingProtectionFilterDisabledBrowserTest& operator=(
      const FingerprintingProtectionFilterDisabledBrowserTest&) = delete;

  ~FingerprintingProtectionFilterDisabledBrowserTest() override;

 protected:
  void SetUpOnMainThread() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class FingerprintingProtectionFilterRefreshHeuristicExceptionBrowserTest
    : public FingerprintingProtectionFilterBrowserTest {
 public:
  FingerprintingProtectionFilterRefreshHeuristicExceptionBrowserTest();

  FingerprintingProtectionFilterRefreshHeuristicExceptionBrowserTest(
      const FingerprintingProtectionFilterRefreshHeuristicExceptionBrowserTest&) =
      delete;
  FingerprintingProtectionFilterRefreshHeuristicExceptionBrowserTest& operator=(
      const FingerprintingProtectionFilterRefreshHeuristicExceptionBrowserTest&) =
      delete;

  ~FingerprintingProtectionFilterRefreshHeuristicExceptionBrowserTest()
      override;

 protected:
  void SetUpOnMainThread() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class FingerprintingProtectionFilterTrackingProtectionSettingBrowserTest
    : public FingerprintingProtectionFilterBrowserTest {
 public:
  FingerprintingProtectionFilterTrackingProtectionSettingBrowserTest();

  FingerprintingProtectionFilterTrackingProtectionSettingBrowserTest(
      const FingerprintingProtectionFilterTrackingProtectionSettingBrowserTest&) =
      delete;
  FingerprintingProtectionFilterTrackingProtectionSettingBrowserTest& operator=(
      const FingerprintingProtectionFilterTrackingProtectionSettingBrowserTest&) =
      delete;

  ~FingerprintingProtectionFilterTrackingProtectionSettingBrowserTest()
      override;

 protected:
  void SetUpOnMainThread() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace fingerprinting_protection_filter

#endif  // CHROME_BROWSER_FINGERPRINTING_PROTECTION_FINGERPRINTING_PROTECTION_FILTER_BROWSER_TEST_HARNESS_H_
