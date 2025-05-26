// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/test/fake_cws_chrome_apps.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::WaitKioskLaunched;

namespace {

using kiosk::test::CachedChromeAppVersion;
using kiosk::test::CurrentProfile;
using kiosk::test::InstalledChromeAppVersion;
using kiosk::test::TheKioskChromeApp;

// Paths used to mount fake USB drives.
const std::string_view kUpdatePassAppPath =
    "chromeos/app_mode/apps_and_extensions/external_update/update_pass";
const std::string_view kNoManifestAppPath =
    "chromeos/app_mode/apps_and_extensions/external_update/no_manifest";
const std::string_view kBadManifestAppPath =
    "chromeos/app_mode/apps_and_extensions/external_update/bad_manifest";
const std::string_view kLowerAppVersionAppPath =
    "chromeos/app_mode/apps_and_extensions/external_update/lower_app_version";
const std::string_view kLowerCrxVersionAppPath =
    "chromeos/app_mode/apps_and_extensions/external_update/lower_crx_version";
const std::string_view kBadCrxAppPath =
    "chromeos/app_mode/apps_and_extensions/external_update/bad_crx";

// Helper to mount a directory under //chrome/test/data as a fake USB disk.
class FakeUsbMountHelper {
 public:
  FakeUsbMountHelper() {
    // Created here, then owned by `DiskMountManager`.
    auto* manager = new disks::FakeDiskMountManager();
    disks::DiskMountManager::InitializeForTesting(manager);
  }

  FakeUsbMountHelper(const FakeUsbMountHelper&) = delete;
  FakeUsbMountHelper& operator=(const FakeUsbMountHelper&) = delete;

  ~FakeUsbMountHelper() { disks::DiskMountManager::Shutdown(); }

  // Mounts a fake USB disk using the given `path_relative_to_test_data_dir` as
  // the source path, runs the given function, and unmounts the fake disk.
  void MountUsbAndRun(std::string_view path_relative_to_test_data_dir,
                      base::FunctionRef<void()> to_run_while_usb_is_mounted) {
    std::string source_path =
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
            .AppendASCII(path_relative_to_test_data_dir)
            .value();

    auto& manager = CHECK_DEREF(disks::DiskMountManager::GetInstance());
    manager.MountPath(source_path,
                      /*source_format=*/"",
                      /*mount_label=*/"",
                      /*mount_options=*/{},
                      /*type=*/MountType::kDevice,
                      /*access_mode=*/MountAccessMode::kReadOnly,
                      /*callback=*/base::DoNothing());
    to_run_while_usb_is_mounted();
    manager.UnmountPath(source_path,
                        disks::DiskMountManager::UnmountPathCallback());
  }
};

// Helper to wait for external updates in `KioskChromeAppManager`.
class ExternalUpdateWaiter : public KioskAppManagerObserver {
 public:
  explicit ExternalUpdateWaiter(KioskChromeAppManager* manager) {
    observation_.Observe(manager);
  }

  ExternalUpdateWaiter(const ExternalUpdateWaiter&) = delete;
  ExternalUpdateWaiter& operator=(const ExternalUpdateWaiter&) = delete;

  ~ExternalUpdateWaiter() override = default;

  // Waits for an external update to happen and returns true if it succeeded.
  bool WaitExternalUpdate() { return success_future_.Get(); }

 private:
  // KioskAppManagerObserver overrides:
  void OnKioskAppExternalUpdateComplete(bool success) override {
    success_future_.SetValue(success);
  }

  base::test::TestFuture<bool> success_future_;

  base::ScopedObservation<KioskChromeAppManager, ExternalUpdateWaiter>
      observation_{this};
};

// Triggers a USB update using `path_relative_to_test_data_dir`, and waits the
// external cache to update. Returns true if an update happened.
bool UpdateViaUsb(FakeUsbMountHelper& usb_helper,
                  const std::string& path_relative_to_test_data_dir) {
  ExternalUpdateWaiter waiter(KioskChromeAppManager::Get());

  bool did_update = false;
  usb_helper.MountUsbAndRun(path_relative_to_test_data_dir,
                            [&] { did_update = waiter.WaitExternalUpdate(); });
  return did_update;
}

// Plain data type used in test parameters.
struct TestParam {
  TestParam(std::string_view name,
            std::string_view usb_source_path,
            KioskMixin::CwsChromeAppOption initial_app,
            bool expect_update_works,
            std::string_view final_cached_version,
            std::string_view final_installed_version)
      : name(std::string(name)),
        usb_source_path(std::string(usb_source_path)),
        initial_app(std::move(initial_app)),
        expect_update_works(expect_update_works),
        final_cached_version(std::string(final_cached_version)),
        final_installed_version(std::string(final_installed_version)) {}
  TestParam(const TestParam&) = default;
  TestParam& operator=(const TestParam&) = default;
  ~TestParam() = default;

  // The name of the test instance shown by gtest.
  std::string name;
  // The path relative to //chrome/test/data to mount as a fake USB disk.
  std::string usb_source_path;
  // The Chrome app to be launched in the beginning of the test.
  KioskMixin::CwsChromeAppOption initial_app;
  // True if the USB update is expected to work.
  bool expect_update_works;
  // The app version that should be cached at the end of the test.
  std::string final_cached_version;
  // The app version that should be installed at the end of the test.
  std::string final_installed_version;
};

// Returns the `TestParam` name to be used in gtest.
std::string ParamName(const testing::TestParamInfo<TestParam>& info) {
  return info.param.name;
}

// Converts the given `param` into a `Config` used to auto launch the initial
// app with `KioskMixin`.
KioskMixin::Config ToKioskConfig(const TestParam& param) {
  return KioskMixin::Config{
      param.name,
      KioskMixin::AutoLaunchAccount{param.initial_app.account_id},
      {param.initial_app}};
}

}  // namespace

// Verifies Chrome apps can be updated via USB in Kiosk.
class KioskChromeAppUsbUpdateTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<TestParam> {
 public:
  KioskChromeAppUsbUpdateTest() = default;
  KioskChromeAppUsbUpdateTest(const KioskChromeAppUsbUpdateTest&) = delete;
  KioskChromeAppUsbUpdateTest& operator=(const KioskChromeAppUsbUpdateTest&) =
      delete;

  ~KioskChromeAppUsbUpdateTest() override = default;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(WaitKioskLaunched());
  }

  FakeUsbMountHelper usb_helper_;

  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/ToKioskConfig(GetParam())};
};

IN_PROC_BROWSER_TEST_P(KioskChromeAppUsbUpdateTest, UpdatesViaUsb) {
  // Expect initial version of the app is cached and launched.
  EXPECT_EQ(GetParam().initial_app.crx_version,
            CachedChromeAppVersion(TheKioskChromeApp()));
  EXPECT_EQ(GetParam().initial_app.crx_version,
            InstalledChromeAppVersion(CurrentProfile(), TheKioskChromeApp()));

  // Try the USB update.
  EXPECT_EQ(GetParam().expect_update_works,
            UpdateViaUsb(usb_helper_, GetParam().usb_source_path));

  // Verify the final version of the app is correct.
  EXPECT_EQ(GetParam().final_cached_version,
            CachedChromeAppVersion(TheKioskChromeApp()));
  EXPECT_EQ(GetParam().final_installed_version,
            InstalledChromeAppVersion(CurrentProfile(), TheKioskChromeApp()));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskChromeAppUsbUpdateTest,
    testing::Values(
        // The app version 2.0.0 from USB is saved in the external cache
        // successfully. Kiosk will install it during next launch.
        TestParam{/*name=*/"UsbWithValidV2App",
                  /*usb_source_path=*/kUpdatePassAppPath,
                  /*initial_app=*/kiosk::test::OfflineEnabledChromeAppV1(),
                  /*expect_update_works=*/true,
                  /*final_cached_version=*/"2.0.0",
                  /*final_installed_version=*/"1.0.0"},
        // The USB does not have an external_update.json file.
        TestParam{/*name=*/"UsbWithoutExternalUpdateJson",
                  /*usb_source_path=*/kNoManifestAppPath,
                  /*initial_app=*/kiosk::test::OfflineEnabledChromeAppV1(),
                  /*expect_update_works=*/false,
                  /*final_cached_version=*/"1.0.0",
                  /*final_installed_version=*/"1.0.0"},
        // The USB has an external_update.json that is not valid JSON.
        TestParam{/*name=*/"UsbWithInvalidExternalUpdateJson",
                  /*usb_source_path=*/kBadManifestAppPath,
                  /*initial_app=*/kiosk::test::OfflineEnabledChromeAppV1(),
                  /*expect_update_works=*/false,
                  /*final_cached_version=*/"1.0.0",
                  /*final_installed_version=*/"1.0.0"},
        // The external_update.json points to the wrong CRX file.
        TestParam{/*name=*/"UsbWithWrongAppCrx",
                  /*usb_source_path=*/kBadCrxAppPath,
                  /*initial_app=*/kiosk::test::OfflineEnabledChromeAppV1(),
                  /*expect_update_works=*/false,
                  /*final_cached_version=*/"1.0.0",
                  /*final_installed_version=*/"1.0.0"},
        // The USB has version 1.0.0, but Kiosk already has version 2.0.0.
        TestParam{/*name=*/"UsbWithLowerVersionApp",
                  /*usb_source_path=*/kLowerAppVersionAppPath,
                  /*initial_app=*/kiosk::test::OfflineEnabledChromeAppV2(),
                  /*expect_update_works=*/false,
                  /*final_cached_version=*/"2.0.0",
                  /*final_installed_version=*/"2.0.0"},
        // The external_update.json says the app is version 3.0.0, but the app
        // manifest in the CRX has version 1.0.0.
        TestParam{/*name=*/"UsbWithWrongCrxVersion",
                  /*usb_source_path=*/kLowerCrxVersionAppPath,
                  /*initial_app=*/kiosk::test::OfflineEnabledChromeAppV2(),
                  /*expect_update_works=*/false,
                  /*final_cached_version=*/"2.0.0",
                  /*final_installed_version=*/"2.0.0"}),
    ParamName);

}  // namespace ash
