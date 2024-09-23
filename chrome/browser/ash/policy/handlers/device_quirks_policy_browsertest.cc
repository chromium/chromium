// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/quirks/quirks_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

const int64_t kProductId = 0x0000aaaa;
const char kDisplayName[] = "FakeDisplay";
const uint8_t kFakeIccData[] = {0x00, 0x00, 0x08, 0x90, 0x20, 0x20,
                                0x20, 0x20, 0x02, 0x10, 0x00, 0x00};

class DeviceQuirksPolicyTest : public DevicePolicyCrosBrowserTest {
 public:
  DeviceQuirksPolicyTest() {}

  DeviceQuirksPolicyTest(const DeviceQuirksPolicyTest&) = delete;
  DeviceQuirksPolicyTest& operator=(const DeviceQuirksPolicyTest&) = delete;

  void SetUpOnMainThread() override {
    // NOTE: QuirksManager::Initialize() isn't necessary here, since it'll be
    // called in `ChromeBrowserMainPartsAsh::PreMainMessageLoopRun()`.

    // Create display_profiles subdirectory under temp profile directory.
    base::FilePath path =
        quirks::QuirksManager::Get()->delegate()->GetDisplayProfileDirectory();
    base::File::Error error = base::File::FILE_OK;
    bool created = base::CreateDirectoryAndGetError(path, &error);
    ASSERT_TRUE(created) << error;

    // Create fake icc file.
    path = path.Append(quirks::IdToFileName(kProductId));
    bool all_written = base::WriteFile(
        path, base::span<const uint8_t>(kFakeIccData, sizeof(kFakeIccData)));
    ASSERT_TRUE(all_written);
  }

 protected:
  void RefreshPolicyAndWaitDeviceSettingsUpdated() {
    base::RunLoop run_loop;
    base::CallbackListSubscription subscription =
        ash::CrosSettings::Get()->AddSettingsObserver(
            ash::kDeviceQuirksDownloadEnabled, run_loop.QuitWhenIdleClosure());

    RefreshDevicePolicy();
    run_loop.Run();
  }

  // Query QuirksManager for icc file, then run msg loop to wait for callback.
  // This won't actually run a Quirks client: if Quirks is enabled, it will
  // return the icc file in our fake downloads directory; if disabled, it will
  // return before looking there.
  bool TestQuirksEnabled() {
    icc_path_.clear();

    base::RunLoop run_loop;
    quirks::QuirksManager::Get()->RequestIccProfilePath(
        kProductId, kDisplayName,
        base::BindOnce(&DeviceQuirksPolicyTest::OnQuirksClientFinished,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    // Quirks only returns our fake file if it's enabled.
    return !icc_path_.empty();
  }

  // Callback from RequestIccProfilePath().
  void OnQuirksClientFinished(base::OnceClosure quit_closure,
                              const base::FilePath& path,
                              bool downloaded) {
    ASSERT_FALSE(downloaded);
    icc_path_ = path;
    std::move(quit_closure).Run();
  }

  base::FilePath icc_path_;  // Path to icc file if found or downloaded.
};

IN_PROC_BROWSER_TEST_F(DeviceQuirksPolicyTest, CheckUnset) {
  bool quirks_download_enabled;
  EXPECT_FALSE(ash::CrosSettings::Get()->GetBoolean(
      ash::kDeviceQuirksDownloadEnabled, &quirks_download_enabled));

  // No policy set, default is enabled, so Quirks should find the fake icc file.
  EXPECT_TRUE(TestQuirksEnabled());
}

IN_PROC_BROWSER_TEST_F(DeviceQuirksPolicyTest, CheckTrue) {
  bool quirks_download_enabled;
  EXPECT_FALSE(ash::CrosSettings::Get()->GetBoolean(
      ash::kDeviceQuirksDownloadEnabled, &quirks_download_enabled));

  enterprise_management::ChromeDeviceSettingsProto& proto(
      device_policy()->payload());
  proto.mutable_quirks_download_enabled()->set_quirks_download_enabled(true);
  RefreshPolicyAndWaitDeviceSettingsUpdated();

  quirks_download_enabled = false;
  EXPECT_TRUE(ash::CrosSettings::Get()->GetBoolean(
      ash::kDeviceQuirksDownloadEnabled, &quirks_download_enabled));
  EXPECT_TRUE(quirks_download_enabled);

  // With policy enabled, Quirks should find the fake icc file.
  EXPECT_TRUE(TestQuirksEnabled());
}

IN_PROC_BROWSER_TEST_F(DeviceQuirksPolicyTest, CheckFalse) {
  bool quirks_download_enabled;
  EXPECT_FALSE(ash::CrosSettings::Get()->GetBoolean(
      ash::kDeviceQuirksDownloadEnabled, &quirks_download_enabled));

  enterprise_management::ChromeDeviceSettingsProto& proto(
      device_policy()->payload());
  proto.mutable_quirks_download_enabled()->set_quirks_download_enabled(false);
  RefreshPolicyAndWaitDeviceSettingsUpdated();

  quirks_download_enabled = true;
  EXPECT_TRUE(ash::CrosSettings::Get()->GetBoolean(
      ash::kDeviceQuirksDownloadEnabled, &quirks_download_enabled));
  EXPECT_FALSE(quirks_download_enabled);

  // With policy disabled, Quirks should abort and not find the fake icc file.
  EXPECT_FALSE(TestQuirksEnabled());
}

}  // namespace policy
