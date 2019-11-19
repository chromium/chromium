// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_jstest_base.h"

class VideoPlayerJsTest : public FileManagerJsTestBase {
 protected:
  VideoPlayerJsTest()
      : FileManagerJsTestBase(base::FilePath(
            FILE_PATH_LITERAL("ui/file_manager/video_player/js"))) {}
};

IN_PROC_BROWSER_TEST_F(VideoPlayerJsTest, SaveResumePlaybackTest) {
  RunTestURL("video_player_native_controls_unittest_gen.html");
}
