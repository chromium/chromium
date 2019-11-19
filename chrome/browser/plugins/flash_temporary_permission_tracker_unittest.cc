// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/flash_temporary_permission_tracker.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char* kOrigin1 = "https://google.com";
const char* kOrigin2 = "https://maps.google.com";
const char* kOrigin3 = "https://example.com";

}  // namespace

class FlashTemporaryPermissionTrackerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    tracker_ = new FlashTemporaryPermissionTracker(profile());
  }

 protected:
  FlashTemporaryPermissionTracker* tracker() { return tracker_.get(); }

  content::RenderFrameHost* GetMainRFH(const char* origin) {
    return content::NavigationSimulator::NavigateAndCommitFromDocument(
        GURL(origin), web_contents()->GetMainFrame());
  }

  content::RenderFrameHost* AddChildRFH(content::RenderFrameHost* parent,
                                        const char* origin) {
    content::RenderFrameHost* subframe =
        content::RenderFrameHostTester::For(parent)->AppendChild("");
    return content::NavigationSimulator::NavigateAndCommitFromDocument(
        GURL(origin), subframe);
  }

 private:
  scoped_refptr<FlashTemporaryPermissionTracker> tracker_;
};

TEST_F(FlashTemporaryPermissionTrackerTest, GrantSurvivesReloads) {
  GetMainRFH(kOrigin1);

  // Flash shouldn't be enabled initially.
  EXPECT_FALSE(tracker()->IsFlashEnabled(GURL(kOrigin1)));
  tracker()->FlashEnabledForWebContents(web_contents());

  // Flash should be enabled now.
  EXPECT_TRUE(tracker()->IsFlashEnabled(GURL(kOrigin1)));

  // Refresh the page.
  content::NavigationSimulator::Reload(web_contents());
  // Flash should still be enabled after a single refresh.
  EXPECT_TRUE(tracker()->IsFlashEnabled(GURL(kOrigin1)));

  // Refresh again.
  content::NavigationSimulator::Reload(web_contents());
  // Flash should still be enabled.
  EXPECT_TRUE(tracker()->IsFlashEnabled(GURL(kOrigin1)));
}

TEST_F(FlashTemporaryPermissionTrackerTest, GrantSurvivesNavigations) {
  content::RenderFrameHost* rfh = GetMainRFH(kOrigin1);

  tracker()->FlashEnabledForWebContents(web_contents());
  content::NavigationSimulator::Reload(web_contents());
  EXPECT_TRUE(tracker()->IsFlashEnabled(GURL(kOrigin1)));

  // Navigate to another origin. Flash should still be enabled.
  content::NavigationSimulator::NavigateAndCommitFromDocument(GURL(kOrigin2),
                                                              rfh);
  EXPECT_TRUE(tracker()->IsFlashEnabled(GURL(kOrigin1)));
}

TEST_F(FlashTemporaryPermissionTrackerTest,
       GrantSurvivesChildFrameNavigations) {
  content::RenderFrameHost* rfh = GetMainRFH(kOrigin1);
  content::RenderFrameHost* child = AddChildRFH(rfh, kOrigin2);
  EXPECT_EQ(2u, web_contents()->GetAllFrames().size());

  tracker()->FlashEnabledForWebContents(web_contents());
  content::NavigationSimulator::Reload(web_contents());
  EXPECT_TRUE(tracker()->IsFlashEnabled(GURL(kOrigin1)));

  // Recreate the child frame after the reload.
  EXPECT_EQ(1u, web_contents()->GetAllFrames().size());
  child = AddChildRFH(rfh, kOrigin2);

  // Navigate the child frame. Flash should still be enabled after this.
  content::NavigationSimulator::NavigateAndCommitFromDocument(GURL(kOrigin3),
                                                              child);
  EXPECT_TRUE(tracker()->IsFlashEnabled(GURL(kOrigin1)));
}

TEST_F(FlashTemporaryPermissionTrackerTest,
       GrantRevokedWhenWebContentsDestroyed) {
  GetMainRFH(kOrigin1);
  EXPECT_FALSE(tracker()->IsFlashEnabled(GURL(kOrigin1)));

  std::unique_ptr<content::WebContents> temporary_web_contents(
      CreateTestWebContents());
  content::WebContentsTester::For(temporary_web_contents.get())
      ->NavigateAndCommit(GURL(kOrigin1));

  tracker()->FlashEnabledForWebContents(temporary_web_contents.get());
  EXPECT_TRUE(tracker()->IsFlashEnabled(GURL(kOrigin1)));

  temporary_web_contents.reset();
  EXPECT_FALSE(tracker()->IsFlashEnabled(GURL(kOrigin1)));
}

TEST_F(FlashTemporaryPermissionTrackerTest,
       GrantRevokedOnLastGrantingWebContentsDestruction) {
  EXPECT_FALSE(tracker()->IsFlashEnabled(GURL(kOrigin1)));

  std::unique_ptr<content::WebContents> temporary_web_contents(
      CreateTestWebContents());
  content::WebContentsTester::For(temporary_web_contents.get())
      ->NavigateAndCommit(GURL(kOrigin1));

  tracker()->FlashEnabledForWebContents(temporary_web_contents.get());
  EXPECT_TRUE(tracker()->IsFlashEnabled(GURL(kOrigin1)));

  // Make a second WebContents that also grants Flash permission on the origin.
  std::unique_ptr<content::WebContents> second_web_contents(
      CreateTestWebContents());
  content::WebContentsTester::For(second_web_contents.get())
      ->NavigateAndCommit(GURL(kOrigin1));
  tracker()->FlashEnabledForWebContents(second_web_contents.get());
  EXPECT_TRUE(tracker()->IsFlashEnabled(GURL(kOrigin1)));

  // Now destroying the original WebContents should no longer revoke the grant.
  temporary_web_contents.reset();
  EXPECT_TRUE(tracker()->IsFlashEnabled(GURL(kOrigin1)));

  // And destroying the second WebContents should revoke the grant.
  second_web_contents.reset();
  EXPECT_FALSE(tracker()->IsFlashEnabled(GURL(kOrigin1)));
}
