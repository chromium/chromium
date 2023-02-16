// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_video_view.h"

#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/test/test_ash_web_view.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "base/files/file_path.h"
#include "base/test/task_environment.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::testing::Eq;
using ::testing::NotNull;

class AmbientVideoViewTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  TestAshWebViewFactory web_view_factory_;
};

TEST_F(AmbientVideoViewTest, NavigatesToCorrectURL) {
  AmbientVideoView view(base::FilePath("/path/to/video"),
                        base::FilePath("/path/to/html"));
  const TestAshWebView* web_view = static_cast<const TestAshWebView*>(
      view.GetViewByID(kAmbientVideoWebView));
  ASSERT_THAT(web_view, NotNull());
  EXPECT_TRUE(web_view->current_url().SchemeIsFile());
  EXPECT_THAT(web_view->current_url().path(), Eq("/path/to/html"));
  std::string video_path_requested;
  ASSERT_TRUE(net::GetValueForKeyInQuery(web_view->current_url(), "video_src",
                                         &video_path_requested));
  GURL video_src_url(video_path_requested);
  EXPECT_TRUE(video_src_url.SchemeIsFile());
  EXPECT_THAT(video_src_url.path(), Eq("/path/to/video"));
}

}  // namespace
}  // namespace ash
