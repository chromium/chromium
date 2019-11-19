// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/quirks/quirks_manager.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

const int64_t kProductId = 0x0000aaaa;
const char kDisplayName[] = "FakeDisplay";
const char kFakeIccData[] = {0x00, 0x00, 0x08, 0x90, 0x20, 0x20,
                             0x20, 0x20, 0x02, 0x10, 0x00, 0x00};

class DeviceQuirksPolicyTest : public policy::DevicePolicyCrosBrowserTest {
 public:
  DeviceQuirksPolicyTest() {}

  void SetUpOnMainThread() override {
    // NOTE: QuirksManager::Initialize() isn't necessary here, since it'll be
    // called in ChromeBrowserMainPartsChromeos::PreMainMessageLoopRun().

    // Create display_profiles subdirectory under temp profile directory.
    base::FilePath path =
        quirks::QuirksManager::Get()->delegate()->GetDisplayProfileDirectory();
    base::File::Error error = base::File::FILE_OK;
    bool created = base::CreateDirectoryAndGetError(path, &error);
    ASSERT_TRUE(created) << error;

    // Create fake icc file.
    path = path.Append(quirks::IdToFileName(kProductId));
    int bytes_written =
        base::WriteFile(path, kFakeIccData, sizeof(kFakeIccData));
    ASSERT_EQ(sizeof(kFakeIccData), static_cast<size_t>(bytes_written));
  }

 protected:
  void RefreshPolicyAndWaitDeviceSettingsUpdated() {
    base::RunLoop run_loop;
    std::unique_ptr<CrosSettings::ObserverSubscription> observer =
        CrosSettings::Get()->AddSettingsObserver(
            kDeviceQuirksDownloadEnabled, run_loop.QuitWhenIdleClosure());

    RefreshDevicePolicy();
    run_loop.Run();
  }

  // Query QuirksManager for icc file, then run msg loop to wait for callback.
  // This won't actually run a Quirks client: if Quirks is enabled, it will
  // return the icc file in our fake downloads directory; if disabled, it will
  // return before looking there.
  bool TestQuirksEnabled() {
    base::RunLoop run_loop;
    end_message_loop_ = run_loop.QuitClosure();
    icc_path_.clear();

    quirks::QuirksManager::Get()->RequestIccProfilePath(
        kProductId, kDisplayName,
        base::Bind(&DeviceQuirksPolicyTest::OnQuirksClientFinished,
                   base::Unretained(this)));

    run_loop.Run();

    // Quirks only returns our fake file if it's enabled.
    return !icc_path_.empty();
  }

  // Callback from RequestIccProfilePath().
  void OnQuirksClientFinished(const base::FilePath& path, bool downloaded) {
    ASSERT_FALSE(downloaded);
    icc_path_ = path;
    ASSERT_TRUE(!end_message_loop_.is_null());
    end_message_loop_.Run();
  }

  base::Closure end_message_loop_;  // Callback to terminate message loop.
  base::FilePath icc_path_;         // Path to icc file if found or downloaded.

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceQuirksPolicyTest);
};

IN_PROC_BROWSER_TEST_F(DeviceQuirksPolicyTest, CheckUnset) {
  bool quirks_download_enabled;
  EXPECT_FALSE(CrosSettings::Get()->GetBoolean(kDeviceQuirksDownloadEnabled,
                                               &quirks_download_enabled));

  // No policy set, default is enabled, so Quirks should find the fake icc file.
  EXPECT_TRUE(TestQuirksEnabled());
}

IN_PROC_BROWSER_TEST_F(DeviceQuirksPolicyTest, CheckTrue) {
  bool quirks_download_enabled;
  EXPECT_FALSE(CrosSettings::Get()->GetBoolean(kDeviceQuirksDownloadEnabled,
                                               &quirks_download_enabled));

  enterprise_management::ChromeDeviceSettingsProto& proto(
      device_policy()->payload());
  proto.mutable_quirks_download_enabled()->set_quirks_download_enabled(true);
  RefreshPolicyAndWaitDeviceSettingsUpdated();

  quirks_download_enabled = false;
  EXPECT_TRUE(CrosSettings::Get()->GetBoolean(kDeviceQuirksDownloadEnabled,
                                              &quirks_download_enabled));
  EXPECT_TRUE(quirks_download_enabled);

  // With policy enabled, Quirks should find the fake icc file.
  EXPECT_TRUE(TestQuirksEnabled());
}

IN_PROC_BROWSER_TEST_F(DeviceQuirksPolicyTest, CheckFalse) {
  bool quirks_download_enabled;
  EXPECT_FALSE(CrosSettings::Get()->GetBoolean(kDeviceQuirksDownloadEnabled,
                                               &quirks_download_enabled));

  enterprise_management::ChromeDeviceSettingsProto& proto(
      device_policy()->payload());
  proto.mutable_quirks_download_enabled()->set_quirks_download_enabled(false);
  RefreshPolicyAndWaitDeviceSettingsUpdated();

  quirks_download_enabled = true;
  EXPECT_TRUE(CrosSettings::Get()->GetBoolean(kDeviceQuirksDownloadEnabled,
                                              &quirks_download_enabled));
  EXPECT_FALSE(quirks_download_enabled);

  // With policy disabled, Quirks should abort and not find the fake icc file.
  EXPECT_FALSE(TestQuirksEnabled());
}

}  // namespace chromeos
