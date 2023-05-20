// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_video_view.h"

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
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

using AmbientVideoViewTest = AmbientAshTestBase;

TEST_F(AmbientVideoViewTest, NavigatesToCorrectURL) {
  AmbientVideoView view("video.webm", base::FilePath("/path/to/html"),
                        AmbientVideo::kClouds,
                        ambient_controller()->ambient_view_delegate());
  const TestAshWebView* web_view = static_cast<const TestAshWebView*>(
      view.GetViewByID(kAmbientVideoWebView));
  ASSERT_THAT(web_view, NotNull());
  EXPECT_FALSE(web_view->init_params_for_testing().enable_wake_locks);
  EXPECT_TRUE(web_view->current_url().SchemeIsFile());
  EXPECT_THAT(web_view->current_url().path(), Eq("/path/to/html"));
  std::string video_file_requested;
  ASSERT_TRUE(net::GetValueForKeyInQuery(web_view->current_url(), "video_file",
                                         &video_file_requested));
  EXPECT_THAT(video_file_requested, Eq("video.webm"));
}

}  // namespace
}  // namespace ash
