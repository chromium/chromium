// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/interstitials/managed_profile_required_page.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/signin/interstitials/managed_profile_required_controller_client.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr char kBlockDecisionHistogram[] =
    "interstitial.managed_profile_required.decision";
constexpr char kTestUrl[] = "http://example.com";
constexpr char16_t kTestManager[] = u"manager";
constexpr char16_t kTestEmail[] = u"email@manager.com";

class ManagedProfileRequiredPageTest : public testing::Test {
 public:
  ManagedProfileRequiredPageTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  content::WebContents* web_contents() {
    if (!web_contents_) {
      content::WebContents::CreateParams params(profile_);
      web_contents_ = content::WebContents::Create(params);
    }
    return web_contents_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
};

TEST_F(ManagedProfileRequiredPageTest, ShownAndMetricsRecorded) {
  base::HistogramTester histograms;
  auto unsafe_resources =
      safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList();

  histograms.ExpectTotalCount(kBlockDecisionHistogram, 0);

  ManagedProfileRequiredPage test_page = ManagedProfileRequiredPage(
      web_contents(), GURL(kTestUrl), kTestManager, kTestEmail,
      std::make_unique<ManagedProfileRequiredControllerClient>(web_contents(),
                                                               GURL(kTestUrl)));

  // Total count = pages shown + proceeding disabled on the page that was shown
  histograms.ExpectTotalCount(kBlockDecisionHistogram, 2);
  histograms.ExpectBucketCount(kBlockDecisionHistogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectBucketCount(
      kBlockDecisionHistogram,
      security_interstitials::MetricsHelper::PROCEEDING_DISABLED, 1);
}

TEST_F(ManagedProfileRequiredPageTest, UnknownManager) {
  ManagedProfileRequiredPage test_page = ManagedProfileRequiredPage(
      web_contents(), GURL(kTestUrl), std::u16string(), kTestEmail,
      std::make_unique<ManagedProfileRequiredControllerClient>(web_contents(),
                                                               GURL(kTestUrl)));

  base::Value::Dict load_time_data = test_page.GetLoadTimeDataForTesting();
  EXPECT_EQ(base::UTF8ToUTF16(*load_time_data.FindString("heading")),
            l10n_util::GetStringUTF16(
                IDS_MANAGED_PROFILE_INTERSTITIAL_UNKNOWN_MANAGER_HEADING));

  EXPECT_EQ(
      base::UTF8ToUTF16(*load_time_data.FindString("primaryParagraph")),
      l10n_util::GetStringFUTF16(
          IDS_MANAGED_PROFILE_INTERSTITIAL_PRIMARY_PARAGRAPH, kTestEmail));
}

TEST_F(ManagedProfileRequiredPageTest, KnownManager) {
  ManagedProfileRequiredPage test_page = ManagedProfileRequiredPage(
      web_contents(), GURL(kTestUrl), kTestManager, kTestEmail,
      std::make_unique<ManagedProfileRequiredControllerClient>(web_contents(),
                                                               GURL(kTestUrl)));

  base::Value::Dict load_time_data = test_page.GetLoadTimeDataForTesting();
  EXPECT_EQ(base::UTF8ToUTF16(*load_time_data.FindString("heading")),
            l10n_util::GetStringFUTF16(IDS_MANAGED_PROFILE_INTERSTITIAL_HEADING,
                                       kTestManager));

  EXPECT_EQ(
      base::UTF8ToUTF16(*load_time_data.FindString("primaryParagraph")),
      l10n_util::GetStringFUTF16(
          IDS_MANAGED_PROFILE_INTERSTITIAL_PRIMARY_PARAGRAPH, kTestEmail));
}

}  // namespace
