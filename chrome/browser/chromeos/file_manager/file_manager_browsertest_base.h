// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_BROWSERTEST_BASE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/crostini/fake_crostini_features.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"

class NotificationDisplayServiceTester;
class SelectFileDialogExtensionTestFactory;

namespace arc {
class FakeFileSystemInstance;
}  // namespace arc

namespace file_manager {

enum GuestMode { NOT_IN_GUEST_MODE, IN_GUEST_MODE, IN_INCOGNITO };

class DriveFsTestVolume;
class FakeTestVolume;
class DownloadsTestVolume;
class CrostiniTestVolume;
class AndroidFilesTestVolume;
class RemovableTestVolume;
class DocumentsProviderTestVolume;
class MediaViewTestVolume;

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
  void TearDownOnMainThread() override;
  void TearDown() override;

  // Mandatory overrides for each File Manager test extension type.
  virtual GuestMode GetGuestMode() const = 0;
  virtual const char* GetTestCaseName() const = 0;
  virtual std::string GetFullTestCaseName() const = 0;
  virtual const char* GetTestExtensionManifestName() const = 0;

  // Optional overrides for each File Manager test extension type.
  virtual bool GetTabletMode() const;
  virtual bool GetEnableDocumentsProvider() const;
  virtual bool GetEnableArc() const;
  virtual bool GetRequiresStartupBrowser() const;
  virtual bool GetNeedsZipSupport() const;
  virtual bool GetIsOffline() const;
  virtual bool GetEnableFilesNg() const;
  virtual bool GetEnableNativeSmb() const;
  virtual bool GetStartWithNoVolumesMounted() const;

  // Launches the test extension from GetTestExtensionManifestName() and uses
  // it to drive the testing the actual FileManager component extension under
  // test by calling RunTestMessageLoop().
  void StartTest();

 private:
  class MockFileTasksObserver;

  // Returns true if the test requires incognito mode.
  bool IsIncognitoModeTest() const { return GetGuestMode() == IN_INCOGNITO; }

  // Returns true if the test requires in guest mode.
  bool IsGuestModeTest() const { return GetGuestMode() == IN_GUEST_MODE; }

  // Returns true if the test runs in tablet mode.
  bool IsTabletModeTest() const { return GetTabletMode(); }

  // Returns true if the test requires Android documents providers.
  bool IsDocumentsProviderTest() const { return GetEnableDocumentsProvider(); }

  // Returns true if the test requires ARC++.
  bool IsArcTest() const { return GetEnableArc(); }

  // Returns true if the test requires zip/unzip support.
  bool IsZipTest() const { return GetNeedsZipSupport(); }

  // Returns true if Drive should act as if offline.
  bool IsOfflineTest() const { return GetIsOffline(); }

  // Returns true if the test needs the files-ng feature.
  bool IsFilesNgTest() const { return GetEnableFilesNg(); }

  // Returns true if the test needs a native SMB file system provider.
  bool IsNativeSmbTest() const { return GetEnableNativeSmb(); }

  // Returns true if FilesApp should start with no volumes mounted.
  bool DoesTestStartWithNoVolumesMounted() const {
    return GetStartWithNoVolumesMounted();
  }

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

  // Called during tests to determine if SMB file shares is enabled.
  bool IsSmbEnabled() const;

  std::unique_ptr<base::test::ScopedFeatureList> feature_list_;
  crostini::FakeCrostiniFeatures crostini_features_;

  std::unique_ptr<DownloadsTestVolume> local_volume_;
  std::unique_ptr<CrostiniTestVolume> crostini_volume_;
  std::unique_ptr<AndroidFilesTestVolume> android_files_volume_;
  std::map<Profile*, std::unique_ptr<DriveFsTestVolume>> drive_volumes_;
  DriveFsTestVolume* drive_volume_ = nullptr;
  std::unique_ptr<FakeTestVolume> usb_volume_;
  std::unique_ptr<FakeTestVolume> mtp_volume_;
  std::unique_ptr<RemovableTestVolume> partition_1_;
  std::unique_ptr<RemovableTestVolume> partition_2_;
  std::unique_ptr<RemovableTestVolume> partition_3_;
  std::unique_ptr<DocumentsProviderTestVolume> documents_provider_volume_;
  std::unique_ptr<MediaViewTestVolume> media_view_images_;
  std::unique_ptr<MediaViewTestVolume> media_view_videos_;
  std::unique_ptr<MediaViewTestVolume> media_view_audio_;

  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  std::unique_ptr<arc::FakeFileSystemInstance> arc_file_system_instance_;

  std::unique_ptr<MockFileTasksObserver> file_tasks_observer_;

  base::HistogramTester histograms_;
  base::UserActionTester user_actions_;

  // Not owned.
  SelectFileDialogExtensionTestFactory* select_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileManagerBrowserTestBase);
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_BROWSERTEST_BASE_H_
