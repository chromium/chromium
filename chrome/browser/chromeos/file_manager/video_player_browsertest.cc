// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_browsertest_base.h"

#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_switches.h"
#include "media/base/media_switches.h"

namespace file_manager {

template <GuestMode MODE>
class VideoPlayerBrowserTestBase : public FileManagerBrowserTestBase {
 public:
  VideoPlayerBrowserTestBase() = default;

 protected:
  GuestMode GetGuestMode() const override { return MODE; }

  const char* GetTestCaseName() const override {
    return test_case_name_.c_str();
  }

  std::string GetFullTestCaseName() const override {
    return test_case_name_;
  }

  const char* GetTestExtensionManifestName() const override {
    return "video_player_test_manifest.json";
  }

  void set_test_case_name(const std::string& name) { test_case_name_ = name; }

 private:
  std::string test_case_name_;

  DISALLOW_COPY_AND_ASSIGN(VideoPlayerBrowserTestBase);
};

typedef VideoPlayerBrowserTestBase<NOT_IN_GUEST_MODE> VideoPlayerBrowserTest;
typedef VideoPlayerBrowserTestBase<IN_GUEST_MODE>
    VideoPlayerBrowserTestInGuestMode;

IN_PROC_BROWSER_TEST_F(VideoPlayerBrowserTest, OpenSingleVideoOnDownloads) {
  set_test_case_name("openSingleVideoOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(VideoPlayerBrowserTestInGuestMode,
                       OpenSingleVideoOnDownloads) {
  set_test_case_name("openSingleVideoOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(VideoPlayerBrowserTest, OpenSingleVideoOnDrive) {
  set_test_case_name("openSingleVideoOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(VideoPlayerBrowserTest, OpenVideoWithSubtitle) {
  set_test_case_name("openVideoWithSubtitle");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(VideoPlayerBrowserTest, OpenVideoWithoutSubtitle) {
  set_test_case_name("openVideoWithoutSubtitle");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(VideoPlayerBrowserTest, OpenMultipleVideosOnDownloads) {
  set_test_case_name("openMultipleVideosOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(VideoPlayerBrowserTest, CheckInitialElements) {
  set_test_case_name("checkInitialElements");
  StartTest();
}

// Flaky. Suspect due to a race when loading Chromecast integration.
// See https://crbug.com/926035.
IN_PROC_BROWSER_TEST_F(VideoPlayerBrowserTest, DISABLED_NativeMediaKey) {
  // The HardwareMediaKeyHandling feature makes key handling flaky.
  // See https://crbug.com/902519.
  base::test::ScopedFeatureList disable_media_key_handling;
  disable_media_key_handling.InitAndDisableFeature(
      media::kHardwareMediaKeyHandling);
  set_test_case_name("mediaKeyNative");
  StartTest();
}

}  // namespace file_manager
