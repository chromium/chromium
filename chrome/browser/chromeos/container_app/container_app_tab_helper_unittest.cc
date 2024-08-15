// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/container_app/container_app_tab_helper.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_activity_simulator.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/test/scoped_feature_list.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_init_params.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

// Aliases.
using ::base::Bucket;
using ::base::BucketsAreArray;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::WithParamInterface;

}  // namespace

// ContainerAppTabHelperTest ---------------------------------------------------

// Base class for tests of the `ContainerAppTabHelper` parameterized by:
// (a) whether the container app preinstallation feature is enabled.
// (b) whether the profile is off the record.
class ContainerAppTabHelperTest
    : public ChromeRenderViewHostTestHarness,
      public WithParamInterface<
          std::tuple</*is_container_app_preinstall_enabled=*/bool,
                     /*is_profile_off_the_record=*/bool>> {
 public:
  ContainerAppTabHelperTest() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    scoped_feature_list_.InitWithFeatureStates({
        {chromeos::features::kContainerAppPreinstall,
         IsContainerAppPreinstallEnabled()},
        {chromeos::features::kFeatureManagementContainerAppPreinstall,
         IsContainerAppPreinstallEnabled()},
    });
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
    crosapi::mojom::BrowserInitParamsPtr init_params =
        chromeos::BrowserInitParams::GetForTests()->Clone();
    init_params->is_container_app_preinstall_enabled =
        IsContainerAppPreinstallEnabled();
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif
  }

  // Returns whether the container app preinstallation feature is enabled given
  // test parameterization.
  bool IsContainerAppPreinstallEnabled() const {
    return std::get<0>(GetParam());
  }

  // Returns whether the profile is off the record given test parameterization.
  bool IsProfileOffTheRecord() const { return std::get<1>(GetParam()); }

 private:
  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    if (IsProfileOffTheRecord()) {
      Profile* const profile = Profile::FromBrowserContext(
          ChromeRenderViewHostTestHarness::GetBrowserContext());

      Profile* const otr_profile = profile->GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUniqueForTesting(),
          /*create_if_needed=*/true);

      SetContents(content::WebContentsTester::CreateTestWebContents(
          otr_profile, content::SiteInstance::Create(otr_profile)));
    }

    ContainerAppTabHelper::MaybeCreateForWebContents(web_contents());
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Used to conditionally enable/disable the container app preinstallation
  // feature based on test parameterization.
  base::test::ScopedFeatureList scoped_feature_list_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

INSTANTIATE_TEST_SUITE_P(All,
                         ContainerAppTabHelperTest,
                         Combine(/*is_container_app_preinstall_enabled=*/Bool(),
                                 /*is_profile_off_the_record=*/Bool()));

// Tests -----------------------------------------------------------------------

// Verifies that page visits are recorded to histograms as expected.
TEST_P(ContainerAppTabHelperTest, RecordsPageVisitHistograms) {
  // Aliases.
  using Page = ContainerAppTabHelper::Page;

  // Constants.
  constexpr char kBaseUrl[] = "https://example.com/";
  constexpr char kHistogramName[] = "Ash.ContainerApp.Page.Visit";
  constexpr char kRelativeFilename[] = "./filename";

  // Pages.
  const auto pages =
      base::EnumSet<Page, Page::kMinValue, Page::kMaxValue>::All();

  // Create multiple URLs for each page.
  std::map<GURL, Page> page_urls;
  for (const Page page : pages) {
    const std::string page_str = base::NumberToString(static_cast<int>(page));
    page_urls.emplace(GURL(base::StrCat({kBaseUrl, page_str, "/1/"})), page);
    page_urls.emplace(GURL(base::StrCat({kBaseUrl, page_str, "/2/"})), page);
  }

  // Replace URL for each page.
  base::AutoReset<std::map<uint64_t, Page>> page_urls_reset =
      ContainerAppTabHelper::SetPageUrlsForTesting(page_urls);

  base::HistogramTester histogram_tester;
  std::vector<Bucket> histogram_buckets;

  // Navigate to `kBaseUrl` and verify that no histograms are recorded.
  NavigateAndCommit(GURL(kBaseUrl));
  EXPECT_THAT(histogram_tester.GetAllSamples(kHistogramName),
              BucketsAreArray(histogram_buckets));

  // Navigate to `kRelativeFilename` and verify that no histograms are recorded.
  NavigateAndCommit(GURL(kBaseUrl).Resolve(kRelativeFilename));
  EXPECT_THAT(histogram_tester.GetAllSamples(kHistogramName),
              BucketsAreArray(histogram_buckets));

  // Navigate to each `page` `url` and verify that histograms are recorded iff:
  // (a) the container app preinstallation feature is enabled, and
  // (b) the profile is not off the record.
  bool record = IsContainerAppPreinstallEnabled() && !IsProfileOffTheRecord();
  for (const auto& [url, page] : page_urls) {
    auto histogram_buckets_it = base::ranges::find(
        histogram_buckets, static_cast<base::HistogramBase::Sample>(page),
        &Bucket::min);
    {
      // Check exact page match.
      NavigateAndCommit(url);
      if (record) {
        if (histogram_buckets_it == histogram_buckets.end()) {
          histogram_buckets_it =
              histogram_buckets.emplace(histogram_buckets.end(),
                                        /*sample=*/page, /*count=*/1u);
        } else {
          ++(histogram_buckets_it->count);
        }
      }
      EXPECT_THAT(histogram_tester.GetAllSamples(kHistogramName),
                  BucketsAreArray(histogram_buckets));
    }
    {
      // Check page match w/o filename.
      NavigateAndCommit(url.Resolve(kRelativeFilename));
      if (record) {
        ++(histogram_buckets_it->count);
      }
      EXPECT_THAT(histogram_tester.GetAllSamples(kHistogramName),
                  BucketsAreArray(histogram_buckets));
    }
  }
}
