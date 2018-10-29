// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_BROWSERTEST_BASE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"

class NotificationDisplayServiceTester;

namespace file_manager {

enum GuestMode { NOT_IN_GUEST_MODE, IN_GUEST_MODE, IN_INCOGNITO };

class DriveTestVolume;
class FakeTestVolume;
class DownloadsTestVolume;
class CrostiniTestVolume;
class AndroidFilesTestVolume;

class FileManagerBrowserTestBase : public extensions::ExtensionApiTest {
 protected:
  FileManagerBrowserTestBase();
  ~FileManagerBrowserTestBase() override;

  // extensions::ExtensionApiTest:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  bool SetUpUserDataDirectory() override;
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;

  // Mandatory overrides for each File Manager test extension type.
  virtual GuestMode GetGuestMode() const = 0;
  virtual const char* GetTestCaseName() const = 0;
  virtual std::string GetFullTestCaseName() const = 0;
  virtual const char* GetTestExtensionManifestName() const = 0;

  // Optional overrides for each File Manager test extension type.
  virtual bool GetTabletMode() const;
  virtual bool GetEnableDriveFs() const;
  virtual bool GetRequiresStartupBrowser() const;
  virtual bool GetNeedsZipSupport() const;
  virtual bool GetIsOffline() const;

  // Launches the test extension from GetTestExtensionManifestName() and uses
  // it to drive the testing the actual FileManager component extension under
  // test by calling RunTestMessageLoop().
  void StartTest();

 private:
  // Returns true if the test requires incognito mode.
  bool IsIncognitoModeTest() const { return GetGuestMode() == IN_INCOGNITO; }

  // Returns true if the test requires in guest mode.
  bool IsGuestModeTest() const { return GetGuestMode() == IN_GUEST_MODE; }

  // Returns true if the test runs in tablet mode (aka Ash immersive mode).
  bool IsTabletModeTest() const { return GetTabletMode(); }

  // Returns true if the test requires DriveFS.
  bool IsDriveFsTest() const { return GetEnableDriveFs(); }

  // Returns true if the test requires zip/unzip support.
  bool IsZipTest() const { return GetNeedsZipSupport(); }

  // Returns true if Drive should act as if offline.
  bool IsOfflineTest() const { return GetIsOffline(); }

  // Launches the test extension with manifest |manifest_name|. The extension
  // manifest_name file should reside in the specified |path| relative to the
  // Chromium src directory.
  void LaunchExtension(const base::FilePath& path, const char* manifest_name);

  // Runs the test: awaits chrome.test messsage commands and chrome.test PASS
  // or FAIL messsages to process. |OnCommand| is used to handle the commands
  // sent from the test extension. Returns on test PASS or FAIL.
  void RunTestMessageLoop();

  // Process test extension command |name|, with arguments |value|. Write the
  // results to |output|.
  void OnCommand(const std::string& name,
                 const base::DictionaryValue& value,
                 std::string* output);

  // Called during setup if needed, to create a drive integration service for
  // the given |profile|. Caller owns the return result.
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile);

  // Called during tests if needed to mount a crostini volume, and return the
  // mount path of the volume.
  base::FilePath MaybeMountCrostini(
      const std::string& source_path,
      const std::vector<std::string>& mount_options);

  // Called during tablet mode test setup to enable the Ash virtual keyboard.
  void EnableVirtualKeyboard();

  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<DownloadsTestVolume> local_volume_;
  std::unique_ptr<CrostiniTestVolume> crostini_volume_;
  std::unique_ptr<AndroidFilesTestVolume> android_files_volume_;
  std::map<Profile*, std::unique_ptr<DriveTestVolume>> drive_volumes_;
  DriveTestVolume* drive_volume_ = nullptr;
  std::unique_ptr<FakeTestVolume> usb_volume_;
  std::unique_ptr<FakeTestVolume> mtp_volume_;

  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;

  DISALLOW_COPY_AND_ASSIGN(FileManagerBrowserTestBase);
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_BROWSERTEST_BASE_H_
