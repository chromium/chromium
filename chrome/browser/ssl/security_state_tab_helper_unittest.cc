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

class SecurityStateTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  SecurityStateTabHelperTest() = default;
  SecurityStateTabHelperTest(const SecurityStateTabHelperTest&) = delete;
  SecurityStateTabHelperTest& operator=(const SecurityStateTabHelperTest&) =
      delete;
  ~SecurityStateTabHelperTest() override = default;
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

}  // namespace
