// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/first_cct_page_load_marker.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using testing::IsNull;
using testing::NotNull;

TEST(FirstCctPageLoadMarkerTest, ConsumeMarkerRemovesItFromWebContents) {
  // Needed by `TestingProfile`.
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  content::TestWebContentsFactory test_web_contents_factory;
  content::WebContents* web_contents =
      test_web_contents_factory.CreateWebContents(&profile);
  FirstCctPageLoadMarker::CreateForWebContents(web_contents);
  EXPECT_THAT(FirstCctPageLoadMarker::FromWebContents(web_contents), NotNull());
  EXPECT_TRUE(FirstCctPageLoadMarker::ConsumeMarker(web_contents));
  EXPECT_THAT(FirstCctPageLoadMarker::FromWebContents(web_contents), IsNull());
}

}  // namespace
