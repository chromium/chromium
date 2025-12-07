// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "base/check_deref.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::LaunchAppManually;

namespace {

// The "Get Volume List" Chrome app to be configured in `KioskMixin`.
//
// This app verifies the file system volumes available in Kiosk via the
// chrome.fileSystem.getVolumeList API.
//
// The code is in:
//   //chrome/test/data/chromeos/app_mode/apps_and_extensions/get_volume_list
KioskMixin::CwsChromeAppOption VolumeListChromeApp() {
  constexpr static std::string_view kAppId = "enelnimkndkcejhjnpaofdlbbfmdnagi";
  return KioskMixin::CwsChromeAppOption{
      /*account_id=*/"volume-list-chrome-app@localhost",
      /*app_id=*/kAppId,
      /*crx_filename=*/base::StrCat({kAppId, ".crx"}),
      /*crx_version=*/"0.1"};
}

}  // namespace

// Verifies the chrome.fileSystem.getVolumeList extension API works in Kiosk.
class KioskFileSystemVolumesTest : public MixinBasedInProcessBrowserTest {
 public:
  KioskFileSystemVolumesTest() = default;
  KioskFileSystemVolumesTest(const KioskFileSystemVolumesTest&) = delete;
  KioskFileSystemVolumesTest& operator=(const KioskFileSystemVolumesTest&) =
      delete;

  ~KioskFileSystemVolumesTest() override = default;

  KioskMixin kiosk_{
      &mixin_host_,
      /*cached_configuration=*/KioskMixin::Config{/*name=*/{},
                                                  /*auto_launch_account_id=*/{},
                                                  {VolumeListChromeApp()}}};
};

IN_PROC_BROWSER_TEST_F(KioskFileSystemVolumesTest, GetVolumeList) {
  extensions::ResultCatcher catcher;
  ASSERT_TRUE(LaunchAppManually(VolumeListChromeApp().account_id));
  // Note this Chrome app has no UI. That means `kiosk_.WaitSessionLaunched()`
  // would fail since Kiosk launch waits for the app window to appear. This test
  // remains in splash screen.
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace ash
