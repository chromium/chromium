// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_BROWSERTEST_BASE_H_

#include <stdint.h>

#include <iosfwd>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/process/kill.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/extensions/mixin_based_extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/devtools_listener.h"
#include "components/account_id/account_id.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_observer.h"
#include "storage/browser/file_system/file_system_url.h"

class NotificationDisplayServiceTester;
class SelectFileDialogExtensionTestFactory;
class Profile;

namespace base {
class CommandLine;

namespace test {
class ScopedFeatureList;
}  // namespace test
}  // namespace base

namespace arc {
class FakeFileSystemInstance;
}  // namespace arc

namespace content {
class TestNavigationObserver;
class WebContents;
}  // namespace content

namespace ui {
class KeyEvent;
}  // namespace ui

namespace file_manager {

enum GuestMode { NOT_IN_GUEST_MODE, IN_GUEST_MODE, IN_INCOGNITO };
enum TestAccountType {
  kTestAccountTypeNotSet,
  kEnterprise,
  kChild,
  kNonManaged,
  // Non-managed account as a non owner profile on a device.
  kNonManagedNonOwner,
  kGoogler,
};
enum DeviceMode { kDeviceModeNotSet, kConsumerOwned, kEnrolled };

class AndroidFilesTestVolume;
class CrostiniTestVolume;
class DocumentsProviderTestVolume;
class DownloadsTestVolume;
class DriveFsTestVolume;
class FakeTestVolume;
class FileSystemProviderTestVolume;
class GuestOsTestVolume;
class HiddenTestVolume;
class MediaViewTestVolume;
class RemovableTestVolume;
class SmbfsTestVolume;

ash::LoggedInUserMixin::LogInType LogInTypeFor(
    TestAccountType test_account_type);

std::optional<AccountId> AccountIdFor(TestAccountType test_account_type);

class FileManagerBrowserTestBase
    : public content::DevToolsAgentHostObserver,
      public extensions::MixinBasedExtensionApiTest {
 public:
  struct Options {
    Options();
    Options(const Options&);
    ~Options();

    // Should test run in Guest or Incognito mode?
    GuestMode guest_mode = NOT_IN_GUEST_MODE;

    // Locale used for this test to run.
    std::string locale;

    // A stored permanent country in `VariationsService` for this test to run.
    std::string country;

    // Account type used to log-in for a test session. This option is valid only
    // for `LoggedInUserFilesAppBrowserTest`. This won't work with `guest_mode`
    // option.
    TestAccountType test_account_type = kTestAccountTypeNotSet;

    // Device mode used for a test session. This option is valid only for
    // `LoggedInUserFilesAppBrowserTest`. This might not work with `guest_mode`
    // option.
    DeviceMode device_mode = kDeviceModeNotSet;

    // Whether test runs in tablet mode.
    bool tablet_mode = false;

    // Whether test requires a generic Android documents provider.
    bool generic_documents_provider = false;

    // Whether test requires Android documents provider for Google Photos.
    bool photos_documents_provider = false;

    // Whether test requires a fake file system provider.
    bool fake_file_system_provider = false;

    // Whether test requires ARC++.
    bool arc = false;

    // Whether test requires a browser to be started.
    bool browser = false;

    // Whether Drive should act as if offline.
    bool offline = false;

    // Whether test should enable the conflict dialog.
    bool enable_conflict_dialog = false;

    // Whether test needs a native SMB file system provider.
    bool native_smb = true;

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

    // Whether test should enable Drive trash.
    bool enable_drive_trash = false;

    // Whether test should run Files app UI as JS modules.
    bool enable_js_modules = true;

    // Whether test should run with the new Banners framework feature.
    bool enable_banners_framework = false;

    // Whether test should enable DLP (Data Leak Prevention) files restrictions
    // feature.
    bool enable_dlp_files_restriction = false;

    // Whether test should enable Files policy new UX feature.
    bool enable_files_policy_new_ux = false;

    // Whether test should run with the Upload Office to Cloud feature.
    bool enable_upload_office_to_cloud = false;

    // Whether test should run with ARCVM enabled.
    bool enable_arc_vm = false;

    // Whether test should run with the DriveFsMirroring flag.
    bool enable_mirrorsync = false;

    // Whether test should enable the file transfer connector.
    bool enable_file_transfer_connector = false;

    // Whether test should enable the new UX for the file transfer connector.
    bool enable_file_transfer_connector_new_ux = false;

    // Whether test should use report-only mode for the file transfer connector.
    bool file_transfer_connector_report_only = false;

    // Whether tests should set up justification mode for the file transfer
    // connector.
    bool bypass_requires_justification = false;

    // Whether tests should disable Google One offer Files banner. This flag is
    // disabled by default.
    bool disable_google_one_offer_files_banner = false;

    // Whether tests should enable local image search by query.
    bool enable_local_image_search = false;

    // Whether tests should enable Google One offer Files banner. This flag is
    // enabled by default.
    bool enable_google_one_offer_files_banner = true;

    // Whether tests should enable the Google Drive bulk pinning feature.
    bool enable_drive_bulk_pinning = false;

    // Whether to enable Drive shortcuts showing a badge or not.
    bool enable_drive_shortcuts = false;

    // Whether to enable jellybean UI elements.
    bool enable_cros_components = false;

    // Whether to enable the materialized views feature.
    bool enable_materialized_views = false;

    // Whether test should enable the SkyVault feature.
    bool enable_skyvault = false;

    // Feature IDs associated for mapping test cases and features.
    std::vector<std::string> feature_ids;
  };

  FileManagerBrowserTestBase(const FileManagerBrowserTestBase&) = delete;
  FileManagerBrowserTestBase& operator=(const FileManagerBrowserTestBase&) =
      delete;

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

  // Returns an account id used for a test. The base class provides a default
  // implementation.
  virtual AccountId GetAccountId();

  content::WebContents* GetWebContentsForId(const std::string& app_id);

  // Launches the test extension from GetTestExtensionManifestName() and uses
  // it to drive the testing the actual FileManager component extension under
  // test by calling RunTestMessageLoop().
  void StartTest();

 private:
  using IdToWebContents =
      std::map<std::string, raw_ptr<content::WebContents, CtnExperimental>>;

  class MockFileTasksObserver;

  // Launches the test extension with manifest `manifest_name`. The extension
  // manifest_name file should reside in the specified `path` relative to the
  // Chromium `root` directory.
  void LaunchExtension(base::BasePathKey root,
                       const base::FilePath& path,
                       const char* manifest_name);

  // Runs the test: awaits chrome.test messsage commands and chrome.test PASS
  // or FAIL messsages to process. |OnCommand| is used to handle the commands
  // sent from the test extension. Returns on test PASS or FAIL.
  void RunTestMessageLoop();

  // Process test extension command |name|, with arguments |value|. Write the
  // results to |output|.
  void OnCommand(const std::string& name,
                 const base::Value::Dict& value,
                 std::string* output);

  // Checks if the command is a GuestOs one. If so, handles it and returns
  // true, otherwise it returns false.
  bool HandleGuestOsCommands(const std::string& name,
                             const base::Value::Dict& value,
                             std::string* output);

  // Checks if the command is a DLP one. If so, handles it and returns true,
  // otherwise it returns false.
  virtual bool HandleDlpCommands(const std::string& name,
                                 const base::Value::Dict& value,
                                 std::string* output);

  // Checks if the command is from enterprise connectors. If so, handles it and
  // returns true, otherwise it returns false.
  virtual bool HandleEnterpriseConnectorCommands(const std::string& name,
                                                 const base::Value::Dict& value,
                                                 std::string* output);

  // Checks if the command is from SkyVault. If so, handles it and returns true,
  // otherwise it returns false.
  virtual bool HandleSkyVaultCommands(const std::string& name,
                                      const base::Value::Dict& value,
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

  base::FilePath MaybeMountGuestOs(
      const std::string& source_path,
      const std::vector<std::string>& mount_options);

  // Called during tablet mode test setup to enable the Ash virtual keyboard.
  void EnableVirtualKeyboard();

  // Returns the WebContents associated the last open window of the
  // File Manager app.
  content::WebContents* GetLastOpenWindowWebContents();

  // Returns appId from its WebContents.
  std::string GetSwaAppId(content::WebContents*);

  // Tries to dispatch a key event via aura::WindowTreeHost. Returns true, if
  // successful, false otherwise.
  bool PostKeyEvent(ui::KeyEvent* key_event);

  // Returns all active web_contents.
  std::vector<content::WebContents*> GetAllWebContents();

  // Maps the app_id to WebContents* for all launched SWA apps. NOTE: if the
  // window is closed in the JS the WebContents* will remain invalid here.
  IdToWebContents swa_web_contents_;

  std::unique_ptr<base::test::ScopedFeatureList> feature_list_;
  crostini::FakeCrostiniFeatures crostini_features_;

  std::unique_ptr<DownloadsTestVolume> local_volume_;
  std::unique_ptr<CrostiniTestVolume> crostini_volume_;
  std::unique_ptr<AndroidFilesTestVolume> android_files_volume_;
  std::map<Profile*, std::unique_ptr<DriveFsTestVolume>> drive_volumes_;
  raw_ptr<DriveFsTestVolume> drive_volume_ = nullptr;
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
  std::unique_ptr<MediaViewTestVolume> media_view_documents_;
  std::unique_ptr<SmbfsTestVolume> smbfs_volume_;
  std::unique_ptr<HiddenTestVolume> hidden_volume_;
  std::unique_ptr<FileSystemProviderTestVolume> file_system_provider_volume_;
  std::unique_ptr<content::TestNavigationObserver> test_navigation_observer_;

  // Map from source path (e.g. sftp://1:2) to volume.
  base::flat_map<std::string, std::unique_ptr<GuestOsTestVolume>>
      guest_os_volumes_;

  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;

  std::unique_ptr<arc::FakeFileSystemInstance> arc_file_system_instance_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  std::unique_ptr<MockFileTasksObserver> file_tasks_observer_;
  raw_ptr<SelectFileDialogExtensionTestFactory> select_factory_;  // Not owned.

  base::HistogramTester histograms_;
  base::UserActionTester user_actions_;

  using DevToolsAgentMap =
      std::map<content::DevToolsAgentHost*,
               std::unique_ptr<coverage::DevToolsListener>>;
  base::FilePath devtools_code_coverage_dir_;
  DevToolsAgentMap devtools_agent_;
  uint32_t process_id_ = 0;

  storage::FileSystemURL error_url_;
};

std::ostream& operator<<(std::ostream& out, GuestMode mode);
std::ostream& operator<<(std::ostream& out,
                         const FileManagerBrowserTestBase::Options& options);

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_BROWSERTEST_BASE_H_
