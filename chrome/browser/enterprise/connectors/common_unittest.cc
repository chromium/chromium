// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/common.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/connectors/core/features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/fake_download_item.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/range/range.h"

namespace enterprise_connectors {

namespace {

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

constexpr char kTestUrl[] = "http://example.com/";

class BaseTest : public testing::Test {
 public:
  BaseTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  void EnableFeatures() { scoped_feature_list_.Reset(); }

  Profile* profile() { return profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
};
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

}  // namespace

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
class CollectFrameUrlsTest : public BaseTest {
 public:
  CollectFrameUrlsTest() = default;

 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(kEnterpriseIframeDlpRulesSupport);
    BaseTest::SetUp();

    // Create test web contents as we need to navigate to a URL to get a valid
    // frame chain.
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile(), content::SiteInstance::Create(profile()));
    content::WebContentsTester::For(web_contents_.get())
        ->NavigateAndCommit(GURL(kTestUrl));
  }

  void TearDown() override {
    // `WebContentsTester` needs to be destroyed before the
    // `RenderViewHostTestEnabler`.
    if (web_contents_) {
      web_contents_.reset();
    }
    BaseTest::TearDown();
  }

  content::WebContents* web_contents() { return web_contents_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::WebContents> web_contents_;
  // Needed for frame tree and navigation operations.
  content::RenderViewHostTestEnabler rvh_test_enabler_;
};

TEST_F(CollectFrameUrlsTest, NestedFramesWithUninterestingUrl) {
  base::HistogramTester histogram_tester;

  // Create test URLs.
  const GURL child_frame_url1("http://child1.example.com/");
  const GURL child_frame_url2("http://child2.example.com/");
  const GURL uninteresting_url("about:blank");

  // Create main frame.
  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHostTester* main_frame_tester =
      content::RenderFrameHostTester::For(main_frame);

  // Create and navigate the first child frame.
  content::RenderFrameHost* child_frame1 =
      main_frame_tester->AppendChild("child1");
  content::NavigationSimulator::NavigateAndCommitFromDocument(child_frame_url1,
                                                              child_frame1);

  // Create and navigate the second (nested) child frame.
  content::RenderFrameHostTester* child_frame1_tester =
      content::RenderFrameHostTester::For(child_frame1);
  content::RenderFrameHost* child_frame2 =
      child_frame1_tester->AppendChild("child2");
  content::NavigationSimulator::NavigateAndCommitFromDocument(child_frame_url2,
                                                              child_frame2);

  // Create and navigate the third (nested) child frame with uninteresting URL.
  content::RenderFrameHostTester* child_frame2_tester =
      content::RenderFrameHostTester::For(child_frame2);
  content::RenderFrameHost* child_frame3 =
      child_frame2_tester->AppendChild("child3");
  content::NavigationSimulator::NavigateAndCommitFromDocument(uninteresting_url,
                                                              child_frame3);

  // Set focus on the innermost frame.
  content::FocusWebContentsOnFrame(web_contents(), child_frame3);

  google::protobuf::RepeatedPtrField<std::string> frame_urls =
      CollectFrameUrls(web_contents(), DeepScanAccessPoint::DOWNLOAD);

  // Verify that the URL chain is listed in the right order and chain size
  // histogram is recorded properly. The uninteresting URL and the tab URL
  // should not be included in the chain.
  ASSERT_EQ(2, frame_urls.size());
  EXPECT_EQ(child_frame_url2.spec(), frame_urls[0]);
  EXPECT_EQ(child_frame_url1.spec(), frame_urls[1]);

  // We still include the tab URL in the histogram chain size.
  histogram_tester.ExpectTotalCount(
      "Enterprise.IframeDlpRulesSupport.Download.UrlChainSize", 1);
  histogram_tester.ExpectBucketCount(
      "Enterprise.IframeDlpRulesSupport.Download.UrlChainSize", 3, 1);
}

TEST_F(CollectFrameUrlsTest, TabUrlOnly) {
  base::HistogramTester histogram_tester;

  google::protobuf::RepeatedPtrField<std::string> frame_urls =
      CollectFrameUrls(web_contents(), DeepScanAccessPoint::DOWNLOAD);

  // The URL chain should be empty since there are no iframes.
  EXPECT_EQ(0, frame_urls.size());

  // The histogram should have a value of 1 to account for the tab's URL.
  histogram_tester.ExpectTotalCount(
      "Enterprise.IframeDlpRulesSupport.Download.UrlChainSize", 1);
  histogram_tester.ExpectBucketCount(
      "Enterprise.IframeDlpRulesSupport.Download.UrlChainSize", 1, 1);
}

TEST_F(CollectFrameUrlsTest, NoWebContents) {
  base::HistogramTester histogram_tester;

  google::protobuf::RepeatedPtrField<std::string> frame_urls =
      CollectFrameUrls(nullptr, DeepScanAccessPoint::DOWNLOAD);

  // Since there are no tabs, there should not be any URLs recorded.
  EXPECT_EQ(0, frame_urls.size());

  histogram_tester.ExpectTotalCount(
      "Enterprise.IframeDlpRulesSupport.Download.UrlChainSize", 1);
  histogram_tester.ExpectBucketCount(
      "Enterprise.IframeDlpRulesSupport.Download.UrlChainSize", 0, 1);
}
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

}  // namespace enterprise_connectors
