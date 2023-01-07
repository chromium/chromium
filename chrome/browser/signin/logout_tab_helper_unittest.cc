// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/logout_tab_helper.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/navigation_simulator.h"
#include "google_apis/gaia/gaia_urls.h"

class LogoutTabHelperTest : public ChromeRenderViewHostTestHarness {};

TEST_F(LogoutTabHelperTest, SelfDeleteInPrimaryPageChanged) {
  LogoutTabHelper::CreateForWebContents(web_contents());

  EXPECT_NE(nullptr, LogoutTabHelper::FromWebContents(web_contents()));

  // Load the logout page.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GaiaUrls::GetInstance()->service_logout_url());

  // The helper was deleted in PrimaryPageChanged.
  EXPECT_EQ(nullptr, LogoutTabHelper::FromWebContents(web_contents()));
}
