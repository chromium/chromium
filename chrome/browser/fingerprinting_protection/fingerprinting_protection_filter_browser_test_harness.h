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

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace fingerprinting_protection_filter {

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

  // The path to a multi-frame document used for tests.
  static constexpr const char kTestFrameSetPath[] =
      "/subresource_filter/frame_set.html";

  // PageLoad histogram names.
  static constexpr const char kSubresourceLoadsTotalForPage[] =
      "FingerprintingProtection.PageLoad.NumSubresourceLoads.Total";
  static constexpr const char kSubresourceLoadsEvaluatedForPage[] =
      "FingerprintingProtection.PageLoad.NumSubresourceLoads.Evaluated";
  static constexpr const char kSubresourceLoadsMatchedRulesForPage[] =
      "FingerprintingProtection.PageLoad.NumSubresourceLoads.MatchedRules";
  static constexpr const char kSubresourceLoadsDisallowedForPage[] =
      "FingerprintingProtection.PageLoad.NumSubresourceLoads.Disallowed";

  // Names of the performance measurement histograms.
  static constexpr const char kEvaluationTotalWallDurationForPage[] =
      "FingerprintingProtection.PageLoad.SubresourceEvaluation."
      "TotalWallDuration";
  static constexpr const char kEvaluationTotalCPUDurationForPage[] =
      "FingerprintingProtection.PageLoad.SubresourceEvaluation."
      "TotalCPUDuration";

 protected:

  void SetUpOnMainThread() override;

  void SetRulesetToDisallowURLsWithPathSuffix(const std::string& suffix);

  void SetRulesetWithRules(const std::vector<proto::UrlRule>& rules);

  void AssertUrlContained(const GURL& full_url, const GURL& sub_url);

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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace fingerprinting_protection_filter

#endif  // CHROME_BROWSER_FINGERPRINTING_PROTECTION_FINGERPRINTING_PROTECTION_FILTER_BROWSER_TEST_HARNESS_H_
