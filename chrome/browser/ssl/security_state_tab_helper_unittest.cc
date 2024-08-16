// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kFormSubmissionSecurityLevelHistogram[] =
    "Security.SecurityLevel.FormSubmission";
const char kInsecureMainFrameFormSubmissionSecurityLevelHistogram[] =
    "Security.SecurityLevel.InsecureMainFrameFormSubmission";

class SecurityStateTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  SecurityStateTabHelperTest() = default;
  SecurityStateTabHelperTest(const SecurityStateTabHelperTest&) = delete;
  SecurityStateTabHelperTest& operator=(const SecurityStateTabHelperTest&) =
      delete;
  ~SecurityStateTabHelperTest() override {}
};

TEST_F(SecurityStateTabHelperTest, DoesNotRecreateHelper) {
  ASSERT_FALSE(SecurityStateTabHelper::FromWebContents(web_contents()));
  SecurityStateTabHelper::CreateForWebContents(web_contents());
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents());
  EXPECT_FALSE(helper->uses_embedder_information());

  // This should be a noop.
  SecurityStateTabHelper::CreateForWebContents(web_contents());
  EXPECT_EQ(helper, SecurityStateTabHelper::FromWebContents(web_contents()));
}

TEST_F(SecurityStateTabHelperTest, DoesNotRecreateChromeHelper) {
  ASSERT_FALSE(SecurityStateTabHelper::FromWebContents(web_contents()));
  ChromeSecurityStateTabHelper::CreateForWebContents(web_contents());
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents());
  EXPECT_TRUE(helper->uses_embedder_information());

  // This should be a noop.
  ChromeSecurityStateTabHelper::CreateForWebContents(web_contents());
  EXPECT_EQ(helper, SecurityStateTabHelper::FromWebContents(web_contents()));
}

class ChromeSecurityStateTabHelperHistogramTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeSecurityStateTabHelperHistogramTest() : helper_(nullptr) {}

  ChromeSecurityStateTabHelperHistogramTest(
      const ChromeSecurityStateTabHelperHistogramTest&) = delete;
  ChromeSecurityStateTabHelperHistogramTest& operator=(
      const ChromeSecurityStateTabHelperHistogramTest&) = delete;

  ~ChromeSecurityStateTabHelperHistogramTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ChromeSecurityStateTabHelper::CreateForWebContents(web_contents());
    helper_ = static_cast<ChromeSecurityStateTabHelper*>(
        ChromeSecurityStateTabHelper::FromWebContents(web_contents()));
    NavigateToHTTP();
  }

 protected:
  // content::MockNavigationHandle doesn't have a test helper for setting
  // whether a navigation is in the top frame, so subclass it to override
  // IsInMainFrame() for testing subframe navigations.
  class MockNavigationHandle : public content::MockNavigationHandle {
   public:
    MockNavigationHandle(const GURL& url,
                         content::RenderFrameHost* render_frame_host)
        : content::MockNavigationHandle(url, render_frame_host) {}

    bool IsInMainFrame() const override { return is_in_main_frame_; }

    void set_is_in_main_frame(bool is_in_main_frame) {
      is_in_main_frame_ = is_in_main_frame;
    }

   private:
    bool is_in_main_frame_ = true;
  };

  void StartNavigation(bool is_form, bool is_main_frame) {
    testing::NiceMock<MockNavigationHandle> handle(
        GURL("http://example.test"), web_contents()->GetPrimaryMainFrame());
    handle.set_is_form_submission(is_form);
    handle.set_is_in_main_frame(is_main_frame);
    helper_->DidStartNavigation(&handle);

    handle.set_has_committed(true);
    helper_->DidFinishNavigation(&handle);
  }

  void NavigateToHTTP() { NavigateAndCommit(GURL("http://example.test")); }
  void NavigateToHTTPS() { NavigateAndCommit(GURL("https://example.test")); }

 private:
  raw_ptr<ChromeSecurityStateTabHelper, DanglingUntriaged> helper_;
};

TEST_F(ChromeSecurityStateTabHelperHistogramTest, FormSubmissionHistogram) {
  base::HistogramTester histograms;
  StartNavigation(/*is_form=*/true, /*is_main_frame=*/true);
  histograms.ExpectUniqueSample(kFormSubmissionSecurityLevelHistogram,
                                security_state::WARNING, 1);
}

// Tests that insecure mainframe form submission histograms are recorded
// correctly.
TEST_F(ChromeSecurityStateTabHelperHistogramTest,
       InsecureMainFrameFormSubmissionHistogram) {
  base::HistogramTester histograms;
  // Subframe submissions should not be logged.
  StartNavigation(/*is_form=*/true, /*is_main_frame=*/false);
  histograms.ExpectTotalCount(
      kInsecureMainFrameFormSubmissionSecurityLevelHistogram, 0);

  // Only form submissions from non-HTTPS pages should be logged.
  StartNavigation(/*is_form=*/true, /*is_main_frame=*/true);
  histograms.ExpectUniqueSample(
      kInsecureMainFrameFormSubmissionSecurityLevelHistogram,
      security_state::WARNING, 1);
  NavigateToHTTPS();
  StartNavigation(/*is_form=*/true, /*is_main_frame=*/true);
  histograms.ExpectTotalCount(
      kInsecureMainFrameFormSubmissionSecurityLevelHistogram, 1);
}

}  // namespace
