// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/url_param_filter/cross_otr_observer_android.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/url_param_filter/content/cross_otr_observer.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace url_param_filter {
class CrossOtrObserverAndroidTest : public ChromeRenderViewHostTestHarness {
 public:
  CrossOtrObserverAndroidTest() = default;
};

TEST_F(CrossOtrObserverAndroidTest,
       LongPressBackgroundTabLaunchTypeNotObserved) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  MaybeCreateCrossOtrObserverForTabLaunchType(
      web_contents.get(), TabModel::TabLaunchType::FROM_LONGPRESS_BACKGROUND);

  ASSERT_EQ(CrossOtrObserver::FromWebContents(web_contents.get()), nullptr);
}

TEST_F(CrossOtrObserverAndroidTest, LongPressIncognitoTabLaunchTypeObserved) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  MaybeCreateCrossOtrObserverForTabLaunchType(
      web_contents.get(), TabModel::TabLaunchType::FROM_LONGPRESS_INCOGNITO);

  ASSERT_NE(CrossOtrObserver::FromWebContents(web_contents.get()), nullptr);
}
}  // namespace url_param_filter
