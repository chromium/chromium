// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_sharing_utils.h"

#include "build/build_config.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace glic {

class GlicSharingUtilsTest : public testing::Test {
 public:
  GlicSharingUtilsTest() = default;

  content::WebContents* CreateWebContents(const GURL& url) {
    content::WebContents* web_contents =
        web_contents_factory_.CreateWebContents(&profile_);
    content::WebContentsTester::For(web_contents)->NavigateAndCommit(url);
    return web_contents;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
};

TEST_F(GlicSharingUtilsTest, IsTabValidForSharing_HttpsUrl) {
  content::WebContents* web_contents =
      CreateWebContents(GURL("https://example.com"));
  EXPECT_TRUE(IsTabValidForSharing(web_contents));
}

TEST_F(GlicSharingUtilsTest, IsTabValidForSharing_FileUrl) {
  content::WebContents* web_contents =
      CreateWebContents(GURL("file:///tmp/test.html"));
  EXPECT_TRUE(IsTabValidForSharing(web_contents));
}

TEST_F(GlicSharingUtilsTest, IsTabValidForSharing_ChromeUINewTab) {
  content::WebContents* web_contents =
      CreateWebContents(chrome::ChromeUINewTabURLAsGURL());
  EXPECT_TRUE(IsTabValidForSharing(web_contents));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(GlicSharingUtilsTest, IsTabValidForSharing_ChromeUINativeNewTab) {
  content::WebContents* web_contents =
      CreateWebContents(GURL(chrome::kChromeUINativeNewTabURL));
  EXPECT_TRUE(IsTabValidForSharing(web_contents));
}
#endif

TEST_F(GlicSharingUtilsTest, IsTabValidForSharing_InvalidSharingPage) {
  content::WebContents* web_contents =
      CreateWebContents(GURL(chrome::kChromeUIAboutURL));
  EXPECT_FALSE(IsTabValidForSharing(web_contents));
}

}  // namespace glic
