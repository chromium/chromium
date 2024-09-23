// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_usb_host_permission_manager.h"

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

namespace arc {

namespace {

// Test package strings.
constexpr char kAppName[] = "test.app.name";
constexpr char kAppActivity[] = "test.app.activity";
constexpr char kPackageName[] = "test.app.package.name";

}  // namespace

class ArcUsbHostPermissionTest : public InProcessBrowserTest {
 public:
  ArcUsbHostPermissionTest() = default;

  ArcUsbHostPermissionTest(const ArcUsbHostPermissionTest&) = delete;
  ArcUsbHostPermissionTest& operator=(const ArcUsbHostPermissionTest&) = delete;

  ~ArcUsbHostPermissionTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void SetUpOnMainThread() override {
    profile_ = browser()->profile();
    arc::SetArcPlayStoreEnabledForProfile(profile_, true);

    arc_app_list_pref_ = ArcAppListPrefs::Get(profile_);
    DCHECK(arc_app_list_pref_);

    base::RunLoop run_loop;
    arc_app_list_pref_->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
    run_loop.Run();

    app_instance_ = std::make_unique<arc::FakeAppInstance>(arc_app_list_pref_);
    arc_app_list_pref_->app_connection_holder()->SetInstance(
        app_instance_.get());
    WaitForInstanceReady(arc_app_list_pref_->app_connection_holder());

    arc_usb_permission_manager_ =
        ArcUsbHostPermissionManager::GetForBrowserContext(profile_);
    DCHECK(arc_usb_permission_manager_);
  }

  void TearDownOnMainThread() override {
    arc_app_list_pref_->app_connection_holder()->CloseInstance(
        app_instance_.get());
    app_instance_.reset();
    ArcSessionManager::Get()->Shutdown();
  }

 protected:
  void AddArcApp(const std::string& app_name,
                 const std::string& package_name,
                 const std::string& activity) {
    std::vector<arc::mojom::AppInfoPtr> apps;
    apps.emplace_back(
        arc::mojom::AppInfo::New(app_name, package_name, activity));

    app_instance_->SendPackageAppListRefreshed(package_name, apps);
  }

  void AddArcPackage(const std::string& package_name) {
    app_instance_->SendPackageAdded(arc::mojom::ArcPackageInfo::New(
        package_name, 0 /* package_version */, 0 /* last_backup_android_id */,
        0 /* last_backup_time */, false /* sync */));
  }

  void RemovePackage(const std::string& package_name) {
    app_instance_->UninstallPackage(package_name);
  }

  void DeviceRemoved(const std::string& guid) {
    arc_usb_permission_manager_->DeviceRemoved(guid);
  }

  void RestorePermissionFromChromePrefs() {
    arc_usb_permission_manager_->RestorePermissionFromChromePrefs();
  }

  void UpdateArcUsbScanDeviceListPermission(const std::string& package_name,
                                            bool allowed) {
    arc_usb_permission_manager_->UpdateArcUsbScanDeviceListPermission(
        package_name, allowed);
  }

  void UpdateArcUsbAccessPermission(
      const std::string& package_name,
      const ArcUsbHostPermissionManager::UsbDeviceEntry& usb_device_entry,
      bool allowed) {
    arc_usb_permission_manager_->UpdateArcUsbAccessPermission(
        package_name, usb_device_entry, allowed);
  }

  void GrantTemporayUsbAccessPermission(
      const std::string& package_name,
      const ArcUsbHostPermissionManager::UsbDeviceEntry& usb_device_entry) {
    arc_usb_permission_manager_->GrantUsbAccessPermission(
        package_name, usb_device_entry.guid, usb_device_entry.vendor_id,
        usb_device_entry.product_id);
  }

  std::unordered_set<std::string> GetEventPackageList(
      const ArcUsbHostPermissionManager::UsbDeviceEntry& usb_device_entry)
      const {
    return arc_usb_permission_manager_->GetEventPackageList(
        usb_device_entry.guid, usb_device_entry.serial_number,
        usb_device_entry.vendor_id, usb_device_entry.product_id);
  }

  bool HasUsbScanDeviceListPermission(const std::string& package_name) const {
    return arc_usb_permission_manager_->HasUsbScanDeviceListPermission(
        package_name);
  }

  bool HasUsbAccessPermission(const std::string& package_name,
                              const ArcUsbHostPermissionManager::UsbDeviceEntry&
                                  usb_device_entry) const {
    return arc_usb_permission_manager_->HasUsbAccessPermission(
        package_name, usb_device_entry);
  }

  void ClearPermissions() {
    arc_usb_permission_manager_->ClearPermissionForTesting();
  }

  ArcUsbHostPermissionManager* arc_usb_permission_manager() {
    return arc_usb_permission_manager_;
  }

 private:
  raw_ptr<ArcAppListPrefs, DanglingUntriaged> arc_app_list_pref_;
  raw_ptr<ArcUsbHostPermissionManager, DanglingUntriaged>
      arc_usb_permission_manager_;
  std::unique_ptr<FakeAppInstance> app_instance_;
  raw_ptr<Profile, DanglingUntriaged> profile_;
};

IN_PROC_BROWSER_TEST_F(ArcUsbHostPermissionTest, UsbTemporayPermissionTest) {
  AddArcApp(kAppName, kPackageName, kAppActivity);
  AddArcPackage(kPackageName);
  // Persistent device0.
  const std::string guid0 = "TestGuidXXXXXX0";
  const std::u16string device_name0 = u"TestDevice0";
  const std::u16string serial_number0 = u"TestSerialNumber0";
  const uint16_t vendor_id0 = 123;
  const uint16_t product_id0 = 456;

  ArcUsbHostPermissionManager::UsbDeviceEntry testDevice0(
      guid0, device_name0, serial_number0, vendor_id0, product_id0);

  GrantTemporayUsbAccessPermission(kPackageName, testDevice0);
  EXPECT_TRUE(HasUsbAccessPermission(kPackageName, testDevice0));
  EXPECT_EQ(1u, GetEventPackageList(testDevice0).size());

  DeviceRemoved(guid0);
  EXPECT_FALSE(HasUsbAccessPermission(kPackageName, testDevice0));
  EXPECT_EQ(0u, GetEventPackageList(testDevice0).size());
}

IN_PROC_BROWSER_TEST_F(ArcUsbHostPermissionTest, UsbChromePrefsTest) {
  AddArcApp(kAppName, kPackageName, kAppActivity);
  AddArcPackage(kPackageName);

  // Persistent device0.
  const std::string guid0 = "TestGuidXXXXXX0";
  const std::u16string device_name0 = u"TestDevice0";
  const std::u16string serial_number0 = u"TestSerialNumber0";
  const uint16_t vendor_id0 = 123;
  const uint16_t product_id0 = 456;
  // Persistent device1.
  const std::string guid1 = "TestGuidXXXXXX1";
  const std::u16string device_name1 = u"TestDevice1";
  const std::u16string serial_number1 = u"TestSerialNumber1";
  const uint16_t vendor_id1 = 234;
  const uint16_t product_id1 = 567;
  // Non persistent device2.
  const std::string guid2 = "TestGuidXXXXXX2";
  const std::u16string device_name2 = u"TestDevice2";
  const uint16_t vendor_id2 = 345;
  const uint16_t product_id2 = 678;

  ArcUsbHostPermissionManager::UsbDeviceEntry testDevice0(
      guid0, device_name0, serial_number0, vendor_id0, product_id0);
  ArcUsbHostPermissionManager::UsbDeviceEntry testDevice1(
      guid1, device_name1, serial_number1, vendor_id1, product_id1);
  ArcUsbHostPermissionManager::UsbDeviceEntry testDevice2(
      guid2, device_name2, std::u16string() /*serial_number*/, vendor_id2,
      product_id2);

  EXPECT_FALSE(HasUsbScanDeviceListPermission(kPackageName));
  EXPECT_FALSE(HasUsbAccessPermission(kPackageName, testDevice0));
  EXPECT_FALSE(HasUsbAccessPermission(kPackageName, testDevice1));
  EXPECT_FALSE(HasUsbAccessPermission(kPackageName, testDevice2));

  UpdateArcUsbScanDeviceListPermission(kPackageName, true /*allowed*/);
  UpdateArcUsbAccessPermission(kPackageName, testDevice0, true /*allowed*/);
  UpdateArcUsbAccessPermission(kPackageName, testDevice1, true /*allowed*/);
  UpdateArcUsbAccessPermission(kPackageName, testDevice2, true /*allowed*/);

  EXPECT_TRUE(HasUsbScanDeviceListPermission(kPackageName));
  EXPECT_TRUE(HasUsbAccessPermission(kPackageName, testDevice0));
  EXPECT_TRUE(HasUsbAccessPermission(kPackageName, testDevice1));
  EXPECT_TRUE(HasUsbAccessPermission(kPackageName, testDevice2));

  // Remove all devices. Permission for persistent device should remain.
  DeviceRemoved(guid0);
  DeviceRemoved(guid1);
  DeviceRemoved(guid2);
  EXPECT_TRUE(HasUsbScanDeviceListPermission(kPackageName));
  EXPECT_TRUE(HasUsbAccessPermission(kPackageName, testDevice0));
  EXPECT_TRUE(HasUsbAccessPermission(kPackageName, testDevice1));
  EXPECT_FALSE(HasUsbAccessPermission(kPackageName, testDevice2));

  ClearPermissions();
  EXPECT_FALSE(HasUsbScanDeviceListPermission(kPackageName));
  EXPECT_FALSE(HasUsbAccessPermission(kPackageName, testDevice0));
  EXPECT_FALSE(HasUsbAccessPermission(kPackageName, testDevice1));
  EXPECT_FALSE(HasUsbAccessPermission(kPackageName, testDevice2));

  // Resotre permission from Chrome prefs. Permission for persistent devices
  // should be restored.
  RestorePermissionFromChromePrefs();
  EXPECT_TRUE(HasUsbScanDeviceListPermission(kPackageName));
  EXPECT_TRUE(HasUsbAccessPermission(kPackageName, testDevice0));
  EXPECT_TRUE(HasUsbAccessPermission(kPackageName, testDevice1));
  EXPECT_FALSE(HasUsbAccessPermission(kPackageName, testDevice2));

  // Remove the package. All permission are gone.
  ClearPermissions();
  RemovePackage(kPackageName);
  RestorePermissionFromChromePrefs();
  EXPECT_FALSE(HasUsbScanDeviceListPermission(kPackageName));
  EXPECT_FALSE(HasUsbAccessPermission(kPackageName, testDevice0));
  EXPECT_FALSE(HasUsbAccessPermission(kPackageName, testDevice1));
  EXPECT_FALSE(HasUsbAccessPermission(kPackageName, testDevice2));
}

}  // namespace arc
