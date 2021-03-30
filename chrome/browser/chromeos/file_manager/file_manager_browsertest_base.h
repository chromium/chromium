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
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/crostini/fake_crostini_features.h"
#include "chrome/browser/chromeos/file_manager/devtools_listener.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "content/public/browser/devtools_agent_host_observer.h"

class NotificationDisplayServiceTester;
class SelectFileDialogExtensionTestFactory;

namespace arc {
class FakeFileSystemInstance;
}  // namespace arc

namespace content {
class WebContents;
}  // namespace content

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
class SmbfsTestVolume;

class FileManagerBrowserTestBase : public content::DevToolsAgentHostObserver,
                                   public extensions::ExtensionApiTest {
 public:
  struct Options {
    Options();
    Options(const Options&);

    // Should test run in Guest or Incognito mode?
    GuestMode guest_mode = NOT_IN_GUEST_MODE;

    // Whether test runs in tablet mode.
    bool tablet_mode = false;

    // Whether test requires a generic Android documents provider.
    bool generic_documents_provider = false;

    // Whether test requires Android documents provider for Google Photos.
    bool photos_documents_provider = false;

    // Whether test requires ARC++.
    bool arc = false;

    // Whether test requires a browser to be started.
    bool browser = false;

    // Whether test requires zip/unzip support.
    // TODO(crbug.com/912236) Remove once transition to new ZIP system is done.
    bool zip = false;

    // Whether test uses the new ZIP system.
    // TODO(crbug.com/912236) Remove once transition to new ZIP system is done.
    bool zip_no_nacl = false;

    // Whether test should enable drive dss pinning.
    bool drive_dss_pin = false;

    // Whether Drive should act as if offline.
    bool offline = false;

    // Whether test needs the files-swa feature.
    bool files_swa = false;

    // Whether test needs the media-swa apps.
    bool media_swa = false;

    // Whether test needs a native SMB file system provider.
    bool native_smb = true;

    // Whether test needs smbfs for native SMB integration.
    bool smbfs = false;

    // Whether test needs the unified media view feature.
    bool unified_media_view = false;

    // Whether FilesApp should start with volumes mounted.
    bool mount_volumes = true;

    // Whether test should observe file tasks.
    bool observe_file_tasks = true;

    // Whether test should enable sharesheet.
    bool enable_sharesheet = true;

    // Whether test needs the single partition format feature.
    bool single_partition_format = false;

    // Whether test should enable trash.
    bool enable_trash = false;

    // Whether test should enable holding space.
    bool enable_holding_space = false;

    // Whether test should run Files app UI as JS modules.
    bool enable_js_modules = true;
  };

 protected:
  FileManagerBrowserTestBase();
  ~FileManagerBrowserTestBase() override;

  // content::DevToolsAgentHostObserver:
  bool ShouldForceDevToolsAgentHostCreation() override;
  void DevToolsAgentHostCreated(content::DevToolsAgentHost* host) override;
  void DevToolsAgentHostAttached(content::DevToolsAgentHost* host) override;
  void DevToolsAgentHostNavigated(content::DevToolsAgentHost* host) override;
  void DevToolsAgentHostDetached(content::DevToolsAgentHost* host) override;
  void DevToolsAgentHostCrashed(content::DevToolsAgentHost* host,
                                base::TerminationStatus status) override;

  // extensions::ExtensionApiTest:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  bool SetUpUserDataDirectory() override;
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;
  void TearDown() override;

  // Mandatory overrides for each File Manager test extension type.
  virtual Options GetOptions() const = 0;
  virtual const char* GetTestCaseName() const = 0;
  virtual std::string GetFullTestCaseName() const = 0;
  virtual const char* GetTestExtensionManifestName() const = 0;

  // Launches the test extension from GetTestExtensionManifestName() and uses
  // it to drive the testing the actual FileManager component extension under
  // test by calling RunTestMessageLoop().
  void StartTest();

 private:
  class MockFileTasksObserver;

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

  web_app::AppId files_app_swa_id_;
  content::WebContents* files_app_web_contents_ = nullptr;

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
  std::unique_ptr<DocumentsProviderTestVolume>
      generic_documents_provider_volume_;
  std::unique_ptr<DocumentsProviderTestVolume>
      photos_documents_provider_volume_;
  std::unique_ptr<MediaViewTestVolume> media_view_images_;
  std::unique_ptr<MediaViewTestVolume> media_view_videos_;
  std::unique_ptr<MediaViewTestVolume> media_view_audio_;
  std::unique_ptr<SmbfsTestVolume> smbfs_volume_;

  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;

  std::unique_ptr<arc::FakeFileSystemInstance> arc_file_system_instance_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  std::unique_ptr<MockFileTasksObserver> file_tasks_observer_;
  SelectFileDialogExtensionTestFactory* select_factory_;  // Not owned.

  base::HistogramTester histograms_;
  base::UserActionTester user_actions_;

  base::FilePath devtools_code_coverage_dir_;
  std::map<content::DevToolsAgentHost*, std::unique_ptr<DevToolsListener>>
      devtools_agent_;
  uint32_t process_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FileManagerBrowserTestBase);
};

std::ostream& operator<<(std::ostream& out, GuestMode mode);
std::ostream& operator<<(std::ostream& out,
                         const FileManagerBrowserTestBase::Options& options);

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_BROWSERTEST_BASE_H_
