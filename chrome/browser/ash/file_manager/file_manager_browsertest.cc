// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>

#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/gtest_prod_util.h"
#include "base/immediate_crash.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/config/coverage/buildflags.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task_policy_impl.h"
#include "chrome/browser/ash/file_manager/file_manager_browsertest_base.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "chrome/browser/enterprise/connectors/analysis/mock_file_transfer_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/test/fake_files_request_handler.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/account_id/account_id.h"
#include "components/file_access/test/mock_scoped_file_access_delegate.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_base.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace file_manager {
namespace {
constexpr char kOwnerEmail[] = "owner@example.com";

// Compares DLP AddFilesRequests ignoring the order of repeated fields.
MATCHER_P(EqualsAddFilesRequestsProto, add_files, "") {
  ::dlp::AddFilesRequest reference(add_files);
  ::dlp::AddFilesRequest actual(arg);

  auto CompareAddFileRequest = [](const ::dlp::AddFileRequest& add1,
                                  const ::dlp::AddFileRequest& add2) {
    return std::make_tuple(add1.file_path(), add1.source_url(),
                           add1.referrer_url()) <
           std::make_tuple(add2.file_path(), add2.source_url(),
                           add2.referrer_url());
  };

  std::sort(reference.mutable_add_file_requests()->begin(),
            reference.mutable_add_file_requests()->end(),
            CompareAddFileRequest);
  std::sort(actual.mutable_add_file_requests()->begin(),
            actual.mutable_add_file_requests()->end(), CompareAddFileRequest);

  std::string expected_serialized, actual_serialized;
  reference.SerializeToString(&expected_serialized);
  actual.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}
}  // namespace

// FilesAppBrowserTest parameters.
struct TestCase {
  explicit TestCase(const char* const name) : name(name) {
    CHECK(name && *name) << "no test case name";
  }

  TestCase& InGuestMode() {
    options.guest_mode = IN_GUEST_MODE;
    return *this;
  }

  TestCase& InIncognito() {
    options.guest_mode = IN_INCOGNITO;
    return *this;
  }

  TestCase& TabletMode() {
    options.tablet_mode = true;
    return *this;
  }

  TestCase& EnableGenericDocumentsProvider() {
    options.arc = true;
    options.generic_documents_provider = true;
    return *this;
  }

  TestCase& DisableGenericDocumentsProvider() {
    options.generic_documents_provider = false;
    return *this;
  }

  TestCase& EnablePhotosDocumentsProvider() {
    options.arc = true;
    options.photos_documents_provider = true;
    return *this;
  }

  TestCase& DisablePhotosDocumentsProvider() {
    options.photos_documents_provider = false;
    return *this;
  }

  TestCase& EnableArc() {
    options.arc = true;
    return *this;
  }

  TestCase& Offline() {
    options.offline = true;
    return *this;
  }

  TestCase& FilesExperimental() {
    options.files_experimental = true;
    return *this;
  }

  TestCase& EnableConflictDialog() {
    options.enable_conflict_dialog = true;
    return *this;
  }

  TestCase& DisableNativeSmb() {
    options.native_smb = false;
    return *this;
  }

  TestCase& DontMountVolumes() {
    options.mount_volumes = false;
    return *this;
  }

  TestCase& DontObserveFileTasks() {
    options.observe_file_tasks = false;
    return *this;
  }

  TestCase& EnableSinglePartitionFormat() {
    options.single_partition_format = true;
    return *this;
  }

  // Show the startup browser. Some tests invoke the file picker dialog during
  // the test. Requesting a file picker from a background page is forbidden by
  // the apps platform, and it's a bug that these tests do so.
  // FindRuntimeContext() in select_file_dialog_extension.cc will use the last
  // active browser in this case, which requires a Browser to be present. See
  // https://crbug.com/736930.
  TestCase& WithBrowser() {
    options.browser = true;
    return *this;
  }

  TestCase& EnableDlp() {
    options.enable_dlp_files_restriction = true;
    return *this;
  }

  TestCase& EnableDriveTrash() {
    options.enable_drive_trash = true;
    return *this;
  }

  TestCase& EnableUploadOfficeToCloud() {
    options.enable_upload_office_to_cloud = true;
    return *this;
  }

  TestCase& EnableArcVm() {
    options.enable_arc_vm = true;
    return *this;
  }

  TestCase& EnableMirrorSync() {
    options.enable_mirrorsync = true;
    return *this;
  }

  TestCase& EnableInlineSyncStatus() {
    options.enable_inline_sync_status = true;
    return *this;
  }

  TestCase& EnableInlineSyncStatusProgressEvents() {
    options.enable_inline_sync_status = true;
    options.enable_inline_sync_status_progress_events = true;
    return *this;
  }

  TestCase& EnableFileTransferConnector() {
    options.enable_file_transfer_connector = true;
    return *this;
  }

  TestCase& FileTransferConnectorReportOnlyMode() {
    options.file_transfer_connector_report_only = true;
    return *this;
  }

  TestCase& EnableSearchV2() {
    options.enable_search_v2 = true;
    return *this;
  }

  TestCase& EnableOsFeedback() {
    options.enable_os_feedback = true;
    return *this;
  }

  TestCase& EnableGoogleOneOfferFilesBanner() {
    options.enable_google_one_offer_files_banner = true;
    return *this;
  }

  TestCase& FeatureIds(const std::vector<std::string>& ids) {
    options.feature_ids = ids;
    return *this;
  }

  TestCase& EnableBulkPinning() {
    options.enable_drive_bulk_pinning = true;
    return *this;
  }

  TestCase& EnableDriveShortcuts() {
    options.enable_drive_shortcuts = true;
    return *this;
  }

  TestCase& SetDeviceMode(DeviceMode device_mode) {
    options.device_mode = device_mode;
    return *this;
  }

  TestCase& SetTestAccountType(TestAccountType test_account_type) {
    options.test_account_type = test_account_type;
    return *this;
  }

  TestCase& EnableCrosComponents() {
    options.enable_cros_components = true;
    options.enable_jellybean = true;
    return *this;
  }

  std::string GetFullName() const {
    std::string full_name = name;

    if (options.guest_mode == IN_GUEST_MODE) {
      full_name += "_GuestMode";
    }

    if (options.guest_mode == IN_INCOGNITO) {
      full_name += "_Incognito";
    }

    if (options.tablet_mode) {
      full_name += "_TabletMode";
    }

    if (options.files_experimental) {
      full_name += "_FilesExperimental";
    }

    if (options.enable_conflict_dialog) {
      full_name += "_ConflictDialog";
    }

    if (!options.native_smb) {
      full_name += "_DisableNativeSmb";
    }

    if (options.generic_documents_provider) {
      full_name += "_GenericDocumentsProvider";
    }

    if (options.photos_documents_provider) {
      full_name += "_PhotosDocumentsProvider";
    }

    if (options.single_partition_format) {
      full_name += "_SinglePartitionFormat";
    }

    if (options.enable_drive_trash) {
      full_name += "_DriveTrash";
    }

    if (options.enable_mirrorsync) {
      full_name += "_MirrorSync";
    }

    if (options.enable_inline_sync_status) {
      full_name += "_InlineSyncStatus";
    }

    if (options.file_transfer_connector_report_only) {
      full_name += "_ReportOnly";
    }

    if (options.enable_search_v2) {
      full_name += "_SearchV2";
    }

    if (options.enable_os_feedback) {
      full_name += "_OsFeedback";
    }

    if (options.enable_google_one_offer_files_banner) {
      full_name += "_GoogleOneOfferFilesBanner";
    }

    if (options.enable_drive_bulk_pinning) {
      full_name += "_DriveBulkPinning";
    }

    if (options.enable_drive_shortcuts) {
      full_name += "_DriveShortcuts";
    }

    if (options.enable_cros_components) {
      full_name += "_CrosComponents";
    }

    switch (options.device_mode) {
      case kDeviceModeNotSet:
        break;
      case kConsumerOwned:
        full_name += "_DeviceModeConsumerOwned";
        break;
      case kEnrolled:
        full_name += "_DeviceModeEnrolled";
    }

    switch (options.test_account_type) {
      case kTestAccountTypeNotSet:
        break;
      case kEnterprise:
        full_name += "_AccountTypeEnterprise";
        break;
      case kChild:
        full_name += "_AccountTypeChild";
        break;
      case kNonManaged:
        full_name += "_AccountTypeNonManaged";
        break;
      case kNonManagedNonOwner:
        full_name += "_AccountTypeNonManagedNonOwner";
        break;
      case kGoogler:
        full_name += "_AccountTypeGoogler";
        break;
    }

    return full_name;
  }

  const char* const name;
  FileManagerBrowserTestBase::Options options;
};

std::ostream& operator<<(std::ostream& out, const TestCase& test_case) {
  return out << test_case.options;
}

// FilesApp browser test.
class FilesAppBrowserTest : public FileManagerBrowserTestBase,
                            public ::testing::WithParamInterface<TestCase> {
 public:
  FilesAppBrowserTest() = default;

  FilesAppBrowserTest(const FilesAppBrowserTest&) = delete;
  FilesAppBrowserTest& operator=(const FilesAppBrowserTest&) = delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FileManagerBrowserTestBase::SetUpCommandLine(command_line);
    // Default mode is clamshell: force Ash into tablet mode if requested,
    // and enable the Ash virtual keyboard sub-system therein.
    if (GetOptions().tablet_mode) {
      command_line->AppendSwitchASCII("force-tablet-mode", "touch_view");
      command_line->AppendSwitch(keyboard::switches::kEnableVirtualKeyboard);
    }
  }

  const char* GetTestCaseName() const override { return GetParam().name; }

  std::string GetFullTestCaseName() const override {
    return GetParam().GetFullName();
  }

  const char* GetTestExtensionManifestName() const override {
    return "file_manager_test_manifest.json";
  }

  Options GetOptions() const override { return GetParam().options; }
};

IN_PROC_BROWSER_TEST_P(FilesAppBrowserTest, Test) {
  StartTest();
}

// `FilesAppBrowserTest` with `LoggedInUserMixin` and `DeviceStateMixin`. This
// test provides additional two options from `FilesAppBrowserTest`. Both options
// must be explicitly set for this test.
//
// - test_account_type: Account type used for a test.
// - device_mode: Status of a device, e.g. a device is enrolled.
class LoggedInUserFilesAppBrowserTest : public FilesAppBrowserTest {
 public:
  LoggedInUserFilesAppBrowserTest() {
    // ChromeOS user will be set by `LoggedInUserMixin`.
    set_chromeos_user_ = false;

    device_state_mixin_ = std::make_unique<ash::DeviceStateMixin>(
        &mixin_host_, DeviceStateFor(GetOptions().device_mode));

    logged_in_user_mixin_ = std::make_unique<ash::LoggedInUserMixin>(
        &mixin_host_, LogInTypeFor(GetOptions().test_account_type),
        embedded_test_server(), this, /*should_launch_browser=*/false,
        AccountIdFor(GetOptions().test_account_type));

    // Set up owner email of a device. We set up owner email only if a device is
    // kConsumerOwned. If a device is enrolled, an account cannot be an owner of
    // a device.
    if (GetOptions().device_mode == kConsumerOwned) {
      std::string owner_email;

      switch (GetOptions().test_account_type) {
        case kTestAccountTypeNotSet:
        case kEnterprise:
        case kChild:
        case kNonManaged:
        case kGoogler:
          owner_email = logged_in_user_mixin_->GetAccountId().GetUserEmail();
          break;
        case kNonManagedNonOwner:
          owner_email = kOwnerEmail;
          break;
      }

      scoped_testing_cros_settings_.device_settings()->Set(
          ash::kDeviceOwner, base::Value(owner_email));
    }
  }

  void SetUpOnMainThread() override {
    logged_in_user_mixin_->LogInUser();
    FilesAppBrowserTest::SetUpOnMainThread();
  }

  AccountId GetAccountId() override {
    return logged_in_user_mixin_->GetAccountId();
  }

 private:
  ash::DeviceStateMixin::State DeviceStateFor(DeviceMode device_mode) {
    switch (device_mode) {
      case kDeviceModeNotSet:
        CHECK(false) << "device_mode option must be set for "
                        "LoggedInUserFilesAppBrowserTest";
        // TODO(crbug.com/1061742): `base::ImmediateCrash` is necessary.
        base::ImmediateCrash();
      case kConsumerOwned:
        return ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED;
      case kEnrolled:
        return ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED;
    }
  }

  std::unique_ptr<ash::LoggedInUserMixin> logged_in_user_mixin_;
  std::unique_ptr<ash::DeviceStateMixin> device_state_mixin_;

  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_P(LoggedInUserFilesAppBrowserTest, Test) {
  StartTest();
}

// A version of the FilesAppBrowserTest that supports spanning browser restart
// to allow testing prefs and other things.
class ExtendedFilesAppBrowserTest : public FilesAppBrowserTest {
 public:
  ExtendedFilesAppBrowserTest() = default;

  ExtendedFilesAppBrowserTest(const ExtendedFilesAppBrowserTest&) = delete;
  ExtendedFilesAppBrowserTest& operator=(const ExtendedFilesAppBrowserTest&) =
      delete;
};

IN_PROC_BROWSER_TEST_P(ExtendedFilesAppBrowserTest, PRE_Test) {
  profile()->GetPrefs()->SetBoolean(prefs::kNetworkFileSharesAllowed,
                                    GetOptions().native_smb);
}

IN_PROC_BROWSER_TEST_P(ExtendedFilesAppBrowserTest, Test) {
  StartTest();
}

// DLP source URLs
constexpr char kBlockedSourceUrl[] = "https://blocked.com";
constexpr char kWarnSourceUrl[] = "https://warned.com";
constexpr char kNotSetSourceUrl[] = "https://not-set.com";
constexpr char kNotBlockedSourceUrl[] = "https://allowed.com";

// A version of FilesAppBrowserTest that supports DLP files restrictions.
class DlpFilesAppBrowserTest : public FilesAppBrowserTest {
 public:
  DlpFilesAppBrowserTest(const DlpFilesAppBrowserTest&) = delete;
  DlpFilesAppBrowserTest& operator=(const DlpFilesAppBrowserTest&) = delete;

 protected:
  DlpFilesAppBrowserTest() = default;
  ~DlpFilesAppBrowserTest() override = default;

  void SetUpOnMainThread() override {
    FilesAppBrowserTest::SetUpOnMainThread();
    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&DlpFilesAppBrowserTest::SetDlpRulesManager,
                            base::Unretained(this)));
  }

  bool HandleDlpCommands(const std::string& name,
                         const base::Value::Dict& value,
                         std::string* output) override {
    if (name == "setGetFilesSourcesMock") {
      base::FilePath result =
          file_manager::util::GetDownloadsFolderForProfile(profile());
      const base::Value::List* file_names = value.FindList("fileNames");
      auto* source_urls = value.FindList("sourceUrls");
      EXPECT_TRUE(file_names);
      EXPECT_TRUE(source_urls);
      EXPECT_EQ(file_names->size(), source_urls->size());

      ::dlp::GetFilesSourcesResponse response;
      for (unsigned long i = 0; i < file_names->size(); i++) {
        auto* metadata = response.add_files_metadata();
        metadata->set_path(result.Append((*file_names)[i].GetString()).value());
        metadata->set_source_url((*source_urls)[i].GetString());
      }

      chromeos::DlpClient::Get()->GetTestInterface()->SetGetFilesSourceMock(
          base::BindRepeating(&DlpFilesAppBrowserTest::GetFilesSourcesMock,
                              base::Unretained(this), response));
      return true;
    }
    if (name == "setBlockedFilesTransfer") {
      base::FilePath result =
          file_manager::util::GetDownloadsFolderForProfile(profile());
      auto* file_names = value.FindList("fileNames");
      EXPECT_TRUE(file_names);
      ::dlp::CheckFilesTransferResponse check_files_transfer_response;
      for (const auto& file_name : *file_names) {
        check_files_transfer_response.add_files_paths(
            result.Append(file_name.GetString()).value());
      }
      chromeos::DlpClient::Get()->GetTestInterface()->SetIsAlive(true);
      chromeos::DlpClient::Get()
          ->GetTestInterface()
          ->SetCheckFilesTransferResponse(check_files_transfer_response);
      return true;
    }
    if (name == "setIsRestrictedDestinationRestriction") {
      EXPECT_CALL(
          *mock_rules_manager_,
          IsRestrictedDestination(GURL(kBlockedSourceUrl), testing::_,
                                  policy::DlpRulesManager::Restriction::kFiles,
                                  testing::_, testing::_, testing::_))
          .WillRepeatedly(
              testing::Return(policy::DlpRulesManager::Level::kBlock));
      EXPECT_CALL(
          *mock_rules_manager_,
          IsRestrictedDestination(GURL(kNotBlockedSourceUrl), testing::_,
                                  policy::DlpRulesManager::Restriction::kFiles,
                                  testing::_, testing::_, testing::_))
          .WillRepeatedly(
              ::testing::Return(policy::DlpRulesManager::Level::kAllow));
      return true;
    }
    if (name == "setBlockedComponent") {
      auto* component_str = value.FindString("component");
      EXPECT_TRUE(component_str);
      auto component = MapToPolicyComponent(*component_str);
      EXPECT_NE(data_controls::Component::kUnknownComponent, component);
      policy::DlpRulesManager::AggregatedComponents components;
      components[policy::DlpRulesManager::Level::kBlock].insert(component);
      EXPECT_CALL(*mock_rules_manager_, GetAggregatedComponents)
          .WillOnce(testing::Return(components));
      return true;
    }
    if (name == "setIsRestrictedByAnyRuleRestrictions") {
      EXPECT_CALL(
          *mock_rules_manager_,
          IsRestrictedByAnyRule(GURL(kNotBlockedSourceUrl),
                                policy::DlpRulesManager::Restriction::kFiles,
                                testing::_, testing::_))
          .WillRepeatedly(
              testing::Return(policy::DlpRulesManager::Level::kAllow));

      EXPECT_CALL(
          *mock_rules_manager_,
          IsRestrictedByAnyRule(GURL(kBlockedSourceUrl),
                                policy::DlpRulesManager::Restriction::kFiles,
                                testing::_, testing::_))
          .WillRepeatedly(
              testing::Return(policy::DlpRulesManager::Level::kBlock));

      EXPECT_CALL(
          *mock_rules_manager_,
          IsRestrictedByAnyRule(GURL(kNotSetSourceUrl),
                                policy::DlpRulesManager::Restriction::kFiles,
                                testing::_, testing::_))
          .WillRepeatedly(
              testing::Return(policy::DlpRulesManager::Level::kNotSet));

      EXPECT_CALL(
          *mock_rules_manager_,
          IsRestrictedByAnyRule(GURL(kWarnSourceUrl),
                                policy::DlpRulesManager::Restriction::kFiles,
                                testing::_, testing::_))
          .WillRepeatedly(
              testing::Return(policy::DlpRulesManager::Level::kWarn));
      return true;
    }
    if (name == "setIsRestrictedByAnyRuleBlocked") {
      EXPECT_CALL(*mock_rules_manager_, IsRestrictedByAnyRule)
          .WillRepeatedly(
              ::testing::Return(policy::DlpRulesManager::Level::kBlock));
      return true;
    }
    if (name == "setupScopedFileAccessDelegateAllowed") {
      scoped_file_access_delegate_ =
          std::make_unique<file_access::MockScopedFileAccessDelegate>();
      EXPECT_CALL(*scoped_file_access_delegate_, RequestFilesAccessForSystem)
          .WillOnce([](const std::vector<base::FilePath>& paths,
                       base::OnceCallback<void(file_access::ScopedFileAccess)>
                           callback) {
            std::move(callback).Run(file_access::ScopedFileAccess::Allowed());
          });
      return true;
    }
    if (name == "expectFilesAdditionToDaemon") {
      base::FilePath download_path =
          file_manager::util::GetDownloadsFolderForProfile(profile());
      const base::Value::List* file_names = value.FindList("fileNames");
      auto* source_urls = value.FindList("sourceUrls");
      EXPECT_TRUE(file_names);
      EXPECT_TRUE(source_urls);
      EXPECT_EQ(file_names->size(), source_urls->size());
      ::dlp::AddFilesRequest expected_request;
      for (unsigned long i = 0; i < file_names->size(); i++) {
        ::dlp::AddFileRequest* file_request =
            expected_request.add_add_file_requests();
        file_request->set_file_path(download_path.value() + "/" +
                                    (*file_names)[i].GetString());
        file_request->set_source_url((*source_urls)[i].GetString());
      }
      EXPECT_CALL(add_files_cb,
                  Run(EqualsAddFilesRequestsProto(expected_request),
                      base::test::IsNotNullCallback()));
      chromeos::DlpClient::Get()->GetTestInterface()->SetIsAlive(true);
      chromeos::DlpClient::Get()->GetTestInterface()->SetAddFilesMock(
          add_files_cb.Get());
      return true;
    }
    if (name == "enableNewFilesPolicyUX") {
      policy::DlpFilesController::SetNewFilesPolicyUXEnabledForTesting(true);
      return true;
    }
    if (name == "setCheckFilesTransferMockToPause") {
      base::FilePath download_path =
          file_manager::util::GetDownloadsFolderForProfile(profile());
      absl::optional<int> task_id = value.FindInt("taskId");
      EXPECT_TRUE(task_id.has_value() && task_id.value() > 0);
      const base::Value::List* file_names = value.FindList("fileNames");
      EXPECT_TRUE(file_names);
      std::vector<base::FilePath> warning_files;
      for (const auto& file_name : *file_names) {
        warning_files.emplace_back(download_path.value() + "/" +
                                   file_name.GetString());
      }
      const std::string* action_str = value.FindString("action");
      EXPECT_TRUE(action_str);
      EXPECT_TRUE(*action_str == "copy" || *action_str == "move");
      policy::dlp::FileAction action = *action_str == "copy"
                                           ? policy::dlp::FileAction::kCopy
                                           : policy::dlp::FileAction::kMove;
      // FPNM is created lazily, so call it here to make sure it's created and
      // starts tracking the tasks.
      policy::FilesPolicyNotificationManager* fpnm =
          policy::FilesPolicyNotificationManagerFactory::GetForBrowserContext(
              profile());
      EXPECT_TRUE(fpnm);

      auto cb = base::BindLambdaForTesting(
          [this, task_id, warning_files, action](
              const dlp::CheckFilesTransferRequest,
              chromeos::DlpClient::CheckFilesTransferCallback) {
            policy::FilesPolicyNotificationManager* fpnm =
                policy::FilesPolicyNotificationManagerFactory::
                    GetForBrowserContext(profile());
            ASSERT_TRUE(fpnm);
            ASSERT_TRUE(fpnm->HasIOTask(task_id.value()));
            // Call FPNM to show the warning, which pauses the task.
            fpnm->ShowDlpWarning(base::DoNothing(), task_id,
                                 std::move(warning_files),
                                 policy::DlpFileDestination(), action);
          });
      chromeos::DlpClient::Get()->GetTestInterface()->SetIsAlive(true);
      chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferMock(
          cb);
      return true;
    }
    return false;
  }

  // MockDlpRulesManager is owned by KeyedService and is guaranteed to outlive
  // this class.
  raw_ptr<policy::MockDlpRulesManager, ExperimentalAsh> mock_rules_manager_ =
      nullptr;

  std::unique_ptr<file_access::MockScopedFileAccessDelegate>
      scoped_file_access_delegate_;

  // The callback needs to survive the setup method.
  base::MockRepeatingCallback<void(
      const ::dlp::AddFilesRequest request,
      chromeos::DlpClient::AddFilesCallback callback)>
      add_files_cb;

 private:
  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager =
        std::make_unique<testing::NiceMock<policy::MockDlpRulesManager>>();
    mock_rules_manager_ = dlp_rules_manager.get();
    ON_CALL(*mock_rules_manager_, IsFilesPolicyEnabled)
        .WillByDefault(testing::Return(true));

    files_controller_ =
        std::make_unique<policy::DlpFilesControllerAsh>(*mock_rules_manager_);
    ON_CALL(*mock_rules_manager_, GetDlpFilesController)
        .WillByDefault(testing::Return(files_controller_.get()));

    return dlp_rules_manager;
  }

  // Maps |component| to data_controls::Component.
  data_controls::Component MapToPolicyComponent(const std::string& component) {
    if (component == "arc") {
      return data_controls::Component::kArc;
    }
    if (component == "crostini") {
      return data_controls::Component::kCrostini;
    }
    if (component == "pluginVm") {
      return data_controls::Component::kPluginVm;
    }
    if (component == "usb") {
      return data_controls::Component::kUsb;
    }
    if (component == "drive") {
      return data_controls::Component::kDrive;
    }
    return data_controls::Component::kUnknownComponent;
  }

  // Invokes `callback` with the previously constructed `response`. Note that
  // the result doesn't depend on the value of `request`.
  void GetFilesSourcesMock(
      const dlp::GetFilesSourcesResponse response,
      const dlp::GetFilesSourcesRequest request,
      chromeos::DlpClient::GetFilesSourcesCallback callback) {
    std::move(callback).Run(response);
  }

  std::unique_ptr<policy::DlpFilesControllerAsh> files_controller_;
};

IN_PROC_BROWSER_TEST_P(DlpFilesAppBrowserTest, Test) {
  ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
  ON_CALL(*mock_rules_manager_, IsRestricted)
      .WillByDefault(::testing::Return(policy::DlpRulesManager::Level::kAllow));
  ON_CALL(*mock_rules_manager_, GetReportingManager)
      .WillByDefault(::testing::Return(nullptr));

  StartTest();
}

constexpr char kFileTransferConnectorSettingsForDlp[] = R"(
{
  "service_provider": "google",
  "enable": [
    {
      "source_destination_list": [
        {
          "sources": [{
            "file_system_type": "%s"
          }],
          "destinations": [{
            "file_system_type": "%s"
          }]
        }
      ],
      "tags": ["dlp"]
    }
  ],
  "block_until_verdict": %s
})";

base::TimeDelta kResponseDelay = base::Seconds(0);

const std::set<std::string>* JpgMimeTypes() {
  static std::set<std::string> set = {"image/jpeg"};
  return &set;
}

// A version of FilesAppBrowserTest that supports the file transfer enterprise
// connector.
class FileTransferConnectorFilesAppBrowserTest : public FilesAppBrowserTest {
 public:
  FileTransferConnectorFilesAppBrowserTest(
      const FileTransferConnectorFilesAppBrowserTest&) = delete;
  FileTransferConnectorFilesAppBrowserTest& operator=(
      const FileTransferConnectorFilesAppBrowserTest&) = delete;

 protected:
  FileTransferConnectorFilesAppBrowserTest() = default;
  ~FileTransferConnectorFilesAppBrowserTest() override = default;

  void SetUpOnMainThread() override {
    FilesAppBrowserTest::SetUpOnMainThread();

    // Set a device management token. It is required to enable scanning.
    // Without it, FileTransferAnalysisDelegate::IsEnabled() always
    // returns absl::nullopt.
    SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));

    // Enable reporting.
    enterprise_connectors::test::SetOnSecurityEventReporting(
        profile()->GetPrefs(),
        /*enabled*/ true,
        /*enabled_event_names*/ {},
        /*enabled_opt_in_events*/ {},
        /*machine_scope*/ false);
    // Add mock to check reports.
    cloud_policy_client_ = std::make_unique<policy::MockCloudPolicyClient>();
    cloud_policy_client_->SetDMToken("dm_token");
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        profile())
        ->SetBrowserCloudPolicyClientForTesting(cloud_policy_client_.get());
    // Add IdentityTestEnvironment to verify user name.
    identity_test_environment_ =
        std::make_unique<signin::IdentityTestEnvironment>();
    identity_test_environment_->MakePrimaryAccountAvailable(
        kUserName, signin::ConsentLevel::kSync);
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        profile())
        ->SetIdentityManagerForTesting(
            identity_test_environment_->identity_manager());
  }

  std::string GetScanIDForFileName(std::string file_name) {
    return std::string(kScanId) + file_name;
  }

  bool IsReportOnlyMode() {
    return GetOptions().file_transfer_connector_report_only;
  }

  void ScanningHasCompletedCallback() {
    DCHECK(run_loop_)
        << "run loop not configured, missing call to `setupScanningRunLoop`";
    ++finished_file_transfer_analysis_delegates_;
    DCHECK_LE(finished_file_transfer_analysis_delegates_,
              expected_number_of_file_transfer_analysis_delegates_);

    if (finished_file_transfer_analysis_delegates_ ==
        expected_number_of_file_transfer_analysis_delegates_) {
      // If all FileTransferAnalysisDelegates finished, scanning has been
      // completed.
      run_loop_->QuitClosure().Run();
    }
  }

  // Setup the expectations of the mock.
  // This function uses the stored expectations from the
  // `scanning_expectations_` map.
  void SetupMock(
      enterprise_connectors::MockFileTransferAnalysisDelegate* delegate) {
    // Expect one call to UploadData.
    EXPECT_CALL(*delegate, UploadData(::testing::_))
        .WillOnce(testing::Invoke([this, delegate](base::OnceClosure callback) {
          // When scanning is started, start the normal scan.
          // We modify the callback such that in addition to the normal callback
          // we also call `ScanningHasCompletedCallback()` to notify the test
          // that scanning has completed.
          delegate->FileTransferAnalysisDelegate::UploadData(base::BindOnce(
              [](base::OnceClosure callback,
                 base::OnceClosure scanning_has_completed_callback) {
                // Call the callback
                std::move(callback).Run();
                // Notify that scanning of this delegate has completed.
                std::move(scanning_has_completed_callback).Run();
              },
              std::move(callback),
              base::BindOnce(&FileTransferConnectorFilesAppBrowserTest::
                                 ScanningHasCompletedCallback,
                             base::Unretained(this))));
        }));

    // Call GetAnalysisResultAfterScan from the base class.
    EXPECT_CALL(*delegate, GetAnalysisResultAfterScan(::testing::_))
        .WillRepeatedly(testing::Invoke([delegate](storage::FileSystemURL url) {
          return delegate
              ->FileTransferAnalysisDelegate::GetAnalysisResultAfterScan(url);
        }));
  }

  bool HandleEnterpriseConnectorCommands(const std::string& name,
                                         const base::Value::Dict& value,
                                         std::string* output) override {
    if (name == "setupFileTransferPolicy") {
      // Set the analysis connector (enterprise_connectors) for FILE_TRANSFER.
      // It is also required for FileTransferAnalysisDelegate::IsEnabled() to
      // return a meaningful result.
      const std::string* source = value.FindString("source");
      CHECK(source);
      const std::string* destination = value.FindString("destination");
      CHECK(destination);
      LOG(INFO) << "Setting file transfer policy for transfers from " << *source
                << " to " << *destination;
      enterprise_connectors::test::SetAnalysisConnector(
          profile()->GetPrefs(), enterprise_connectors::FILE_TRANSFER,
          base::StringPrintf(kFileTransferConnectorSettingsForDlp,
                             source->c_str(), destination->c_str(),
                             IsReportOnlyMode() ? "0" : "1"));

      // Create a FakeFilesRequestHandler that intercepts uploads and fakes
      // responses.
      enterprise_connectors::FilesRequestHandler::SetFactoryForTesting(
          base::BindRepeating(
              &enterprise_connectors::test::FakeFilesRequestHandler::Create,
              base::BindRepeating(&FileTransferConnectorFilesAppBrowserTest::
                                      FakeFileUploadCallback,
                                  base::Unretained(this), *source,
                                  *destination)));

      // Setup FileTransferAnalysisDelegate mock.
      enterprise_connectors::FileTransferAnalysisDelegate::SetFactorForTesting(
          base::BindRepeating(
              [](base::RepeatingCallback<void(
                     enterprise_connectors::MockFileTransferAnalysisDelegate*)>
                     mock_setup_callback,
                 safe_browsing::DeepScanAccessPoint access_point,
                 storage::FileSystemURL source_url,
                 storage::FileSystemURL destination_url, Profile* profile,
                 storage::FileSystemContext* file_system_context,
                 enterprise_connectors::AnalysisSettings settings)
                  -> std::unique_ptr<
                      enterprise_connectors::FileTransferAnalysisDelegate> {
                auto delegate = std::make_unique<::testing::StrictMock<
                    enterprise_connectors::MockFileTransferAnalysisDelegate>>(
                    access_point, source_url, destination_url, profile,
                    file_system_context, std::move(settings));

                mock_setup_callback.Run(delegate.get());

                return delegate;
              },
              base::BindRepeating(
                  &FileTransferConnectorFilesAppBrowserTest::SetupMock,
                  base::Unretained(this))));

      return true;
    }
    if (name == "issueFileTransferResponses") {
      // Issue all saved responses and issue all future responses directly.
      IssueResponses();
      return true;
    }
    if (name == "isReportOnlyFileTransferConnector") {
      *output = IsReportOnlyMode() ? "true" : "false";
      return true;
    }
    if (name == "setupScanningRunLoop") {
      // Set the number of expected `FileTransferAnalysisDelegate`s. This is
      // done to correctly notify when scanning has completed.
      auto maybe_int = value.FindInt("number_of_expected_delegates");
      DCHECK(maybe_int.has_value());
      expected_number_of_file_transfer_analysis_delegates_ = maybe_int.value();
      DCHECK(!run_loop_);
      run_loop_ = std::make_unique<base::RunLoop>();
      return true;
    }
    if (name == "waitForFileTransferScanningToComplete") {
      DCHECK(run_loop_);
      // Wait until the scanning is complete.
      run_loop_->Run();
      return true;
    }
    if (name == "expectFileTransferReports") {
      // Setup expectations for the deep scan reports.

      const std::string* source_volume_name = value.FindString("source_volume");
      CHECK(source_volume_name);
      const std::string* destination_volume_name =
          value.FindString("destination_volume");
      CHECK(destination_volume_name);
      const base::Value::List* entry_paths = value.FindList("entry_paths");
      CHECK(entry_paths);

      std::vector<std::string> file_names;
      std::vector<std::string> shas;
      std::vector<enterprise_connectors::ContentAnalysisResponse::Result>
          expected_dlp_verdicts;
      std::vector<std::string> expected_results;
      std::vector<std::string> expected_scan_ids;

      for (const auto& path : *entry_paths) {
        const std::string* path_str = path.GetIfString();
        CHECK(path_str);
        auto file_name = base::FilePath(*path_str).BaseName().AsUTF8Unsafe();
        if (!base::Contains(file_name, "blocked")) {
          // If a file name does not contain blocked, expect no report.
          continue;
        }

        file_names.push_back(file_name);
        // sha256sum chrome/test/data/chromeos/file_manager/small.jpg |  tr
        // '[:lower:]' '[:upper:]'
        shas.push_back(
            "28F5754447BBA26238B93B820DFFCB6743876F8A82077BA1ABB0F4B2529AE5BE");

        // Get the expected verdict from the ConnectorStatusCallback.
        expected_dlp_verdicts.push_back(
            ConnectorStatusCallback(base::FilePath(*path_str)).results()[0]);

        // For report-only mode, the transfer is always allowed. It's blocked,
        // otherwise.
        expected_results.push_back(safe_browsing::EventResultToString(
            IsReportOnlyMode() ? safe_browsing::EventResult::ALLOWED
                               : safe_browsing::EventResult::BLOCKED));
        expected_scan_ids.push_back(GetScanIDForFileName(file_name));
      }

      validator_ =
          std::make_unique<enterprise_connectors::test::EventReportValidator>(
              cloud_policy_client());
      validator_->ExpectSensitiveDataEvents(
          /*url*/ "",
          /*source*/ *source_volume_name,
          /*destination*/ *destination_volume_name,
          /*filenames*/ file_names,
          /*sha*/
          shas,
          /*trigger*/
          extensions::SafeBrowsingPrivateEventRouter::kTriggerFileTransfer,
          /*dlp_verdict*/ expected_dlp_verdicts,
          /*mimetype*/ JpgMimeTypes(),
          /*size*/ 886,
          /*result*/
          expected_results,
          /*username*/ kUserName,
          /*profile_identifier*/ profile()->GetPath().AsUTF8Unsafe(),
          /*scan_ids*/ expected_scan_ids);

      return true;
    }

    return false;
  }

  // Upload callback to issue responses.
  void FakeFileUploadCallback(
      const std::string& expected_source,
      const std::string& expected_destination,
      safe_browsing::BinaryUploadService::Result result,
      const base::FilePath& path,
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request,
      enterprise_connectors::test::FakeFilesRequestHandler::
          FakeFileRequestCallback callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    EXPECT_FALSE(path.empty());
    EXPECT_EQ(request->device_token(), "dm_token");

    // Verify source and destination of the request.
    EXPECT_EQ(request->content_analysis_request().request_data().source(),
              expected_source);
    EXPECT_EQ(request->content_analysis_request().request_data().destination(),
              expected_destination);

    // Simulate a response.
    base::OnceClosure response =
        base::BindOnce(std::move(callback), path,
                       safe_browsing::BinaryUploadService::Result::SUCCESS,
                       ConnectorStatusCallback(path));
    if (save_response_for_later_) {
      // We save the responses for later such that we can check the scanning
      // label.
      // `await sendTestMessage({name: 'issueFileTransferResponses'})` is
      // required from the test to issue the requests.
      saved_responses_.push_back(std::move(response));
    } else {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, std::move(response), kResponseDelay);
    }
  }

  // Issues the saved responses and sets `save_response_for_later_` to `false`.
  // After this method is called, no more responses will be saved. Instead, the
  // responses will be issued directly.
  void IssueResponses() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    save_response_for_later_ = false;
    for (auto&& response : saved_responses_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, std::move(response), kResponseDelay);
    }
    saved_responses_.clear();
  }

  enterprise_connectors::ContentAnalysisResponse ConnectorStatusCallback(
      const base::FilePath& path) {
    enterprise_connectors::ContentAnalysisResponse response;
    // We return a block verdict if the basename contains "blocked".
    if (base::Contains(path.BaseName().value(), "blocked")) {
      response = enterprise_connectors::test::FakeContentAnalysisDelegate::
          FakeContentAnalysisDelegate::DlpResponse(
              enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS,
              "rule", enterprise_connectors::TriggeredRule::BLOCK);
    } else {
      response = enterprise_connectors::test::FakeContentAnalysisDelegate::
          SuccessfulResponse({"dlp"});
    }
    response.set_request_token(
        GetScanIDForFileName(path.BaseName().AsUTF8Unsafe()));
    return response;
  }

  policy::MockCloudPolicyClient* cloud_policy_client() {
    return cloud_policy_client_.get();
  }

  // Used to test reporting.
  std::unique_ptr<policy::MockCloudPolicyClient> cloud_policy_client_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_environment_;
  std::unique_ptr<enterprise_connectors::test::EventReportValidator> validator_;
  static constexpr char kUserName[] = "test@chromium.org";
  static constexpr char kScanId[] = "scan id";

  // The saved scanning responses.
  std::vector<base::OnceClosure> saved_responses_;
  // Determines whether a current scanning response should be saved for later or
  // issued directly.
  bool save_response_for_later_ = true;

  size_t finished_file_transfer_analysis_delegates_ = 0;
  size_t expected_number_of_file_transfer_analysis_delegates_ = 0;

  std::unique_ptr<base::RunLoop> run_loop_;
};

// TODO(crbug.com/1425820): Re-enable this test
IN_PROC_BROWSER_TEST_P(FileTransferConnectorFilesAppBrowserTest,
                       DISABLED_Test) {
  StartTest();
}

// INSTANTIATE_TEST_SUITE_P expands to code that stringizes the arguments. Thus
// macro parameters such as |prefix| and |test_class| won't be expanded by the
// macro pre-processor. To work around this, indirect INSTANTIATE_TEST_SUITE_P,
// as WRAPPED_INSTANTIATE_TEST_SUITE_P here, so the pre-processor expands macro
// defines used to disable tests, MAYBE_prefix for example.
#define WRAPPED_INSTANTIATE_TEST_SUITE_P(prefix, test_class, generator) \
  INSTANTIATE_TEST_SUITE_P(prefix, test_class, generator, &PostTestCaseName)

std::string PostTestCaseName(const ::testing::TestParamInfo<TestCase>& test) {
  return test.param.GetFullName();
}

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileDisplay, /* file_display.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("fileDisplayDownloads")
            .FeatureIds({"screenplay-ade01078-3b79-41d2-953e-e22a544a28b3"}),
        TestCase("fileDisplayDownloads")
            .InGuestMode()
            .FeatureIds({"screenplay-ade01078-3b79-41d2-953e-e22a544a28b3"}),
        TestCase("fileDisplayDownloads")
            .TabletMode()
            .FeatureIds({"screenplay-ade01078-3b79-41d2-953e-e22a544a28b3"}),
        TestCase("fileDisplayLaunchOnDrive").DontObserveFileTasks(),
        TestCase("fileDisplayLaunchOnLocalFolder").DontObserveFileTasks(),
        TestCase("fileDisplayLaunchOnLocalFile").DontObserveFileTasks(),
        TestCase("fileDisplayDrive")
            .TabletMode()
            .FeatureIds({"screenplay-ade01078-3b79-41d2-953e-e22a544a28b3"}),
        TestCase("fileDisplayDrive")
            .FeatureIds({"screenplay-ade01078-3b79-41d2-953e-e22a544a28b3"}),
        TestCase("fileDisplayDriveOffline").Offline(),
        TestCase("fileDisplayDriveOnline"),
        TestCase("fileDisplayDriveOnlineNewWindow").DontObserveFileTasks(),
        TestCase("fileDisplayComputers"),
        TestCase("fileDisplayMtp")
            .FeatureIds({"screenplay-e920978b-0184-4665-98a3-acc46dc48ce9",
                         "screenplay-ade01078-3b79-41d2-953e-e22a544a28b3"}),
        TestCase("fileDisplayUsb")
            .FeatureIds({"screenplay-ade01078-3b79-41d2-953e-e22a544a28b3"}),
        TestCase("fileDisplayUsbPartition"),
        TestCase("fileDisplayUsbPartition").EnableSinglePartitionFormat(),
        TestCase("fileDisplayUsbPartitionSort"),
        TestCase("fileDisplayPartitionFileTable"),
        TestCase("fileSearch"),
        TestCase("fileDisplayWithoutDownloadsVolume").DontMountVolumes(),
        TestCase("fileDisplayWithoutVolumes").DontMountVolumes(),
        TestCase("fileDisplayWithoutVolumesThenMountDownloads")
            .DontMountVolumes(),
        TestCase("fileDisplayWithoutVolumesThenMountDrive").DontMountVolumes(),
        TestCase("fileDisplayWithoutDrive").DontMountVolumes(),
        // Test is failing (crbug.com/1097013)
        // TestCase("fileDisplayWithoutDriveThenDisable").DontMountVolumes(),
        TestCase("fileDisplayWithHiddenVolume"),
        TestCase("fileDisplayMountWithFakeItemSelected"),
        TestCase("fileDisplayUnmountDriveWithSharedWithMeSelected"),
        TestCase("fileDisplayUnmountRemovableRoot"),
        TestCase("fileDisplayUnmountFirstPartition"),
        TestCase("fileDisplayUnmountLastPartition"),
        TestCase("fileSearchCaseInsensitive"),
        TestCase("fileSearchNotFound"),
        TestCase("fileDisplayDownloadsWithBlockedFileTaskRunner"),
        TestCase("fileDisplayCheckSelectWithFakeItemSelected"),
        TestCase("fileDisplayCheckReadOnlyIconOnFakeDirectory"),
        TestCase("fileDisplayCheckNoReadOnlyIconOnDownloads"),
        TestCase("fileDisplayCheckNoReadOnlyIconOnLinuxFiles"),
        TestCase("fileDisplayCheckNoReadOnlyIconOnGuestOs")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenVideoMediaApp, /* open_video_media_app.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("videoOpenDownloads").InGuestMode(),
                      TestCase("videoOpenDownloads"),
                      TestCase("videoOpenDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenAudioMediaApp, /* open_audio_media_app.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("audioOpenDownloads").InGuestMode(),
                      TestCase("audioOpenDownloads"),
                      TestCase("audioOpenDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenImageMediaApp, /* open_image_media_app.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("imageOpenMediaAppDownloads").InGuestMode(),
                      TestCase("imageOpenMediaAppDownloads"),
                      TestCase("imageOpenMediaAppDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenSniffedFiles, /* open_sniffed_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("pdfOpenDownloads"),
                      TestCase("pdfOpenDrive"),
                      TestCase("textOpenDownloads"),
                      TestCase("textOpenDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    OpenFilesInWebDrive, /* open_files_in_web_drive.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("hostedOpenDrive"),
                      TestCase("encryptedHostedOpenDrive"),
                      TestCase("encryptedNonHostedOpenDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    ZipFiles, /* zip_files.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("zipFileOpenDownloads"),
        TestCase("zipFileOpenDownloads").InGuestMode(),
        TestCase("zipFileOpenDrive"),
        TestCase("zipFileOpenUsb"),
        TestCase("zipNotifyFileTasks"),
        TestCase("zipCreateFileDownloads"),
        TestCase("zipCreateFileDownloads").InGuestMode(),
        TestCase("zipCreateFileDrive"),
        TestCase("zipCreateFileDriveOffice"),
        TestCase("zipCreateFileUsb"),
        TestCase("zipDoesntCreateFileEncrypted"),
        TestCase("zipExtractA11y")
            .FeatureIds({"screenplay-af443ca0-6d9f-4cb3-af8f-0939c37833db"}),
        TestCase("zipExtractCheckContent"),
        TestCase("zipExtractCheckDuplicates"),
        TestCase("zipExtractCheckEncodings"),
        TestCase("zipExtractNotEnoughSpace"),
        TestCase("zipExtractFromReadOnly"),
        TestCase("zipExtractShowPanel"),
        TestCase("zipExtractShowMultiPanel"),
        TestCase("zipExtractSelectionMenus")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    CreateNewFolder, /* create_new_folder.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("selectCreateFolderDownloads")
            .FeatureIds({"screenplay-d9f79e27-bec2-4d15-9ba3-ae2bcd1e4bb5"}),
        TestCase("selectCreateFolderDownloads")
            .InGuestMode()
            .FeatureIds({"screenplay-d9f79e27-bec2-4d15-9ba3-ae2bcd1e4bb5"}),
        TestCase("createFolderDownloads")
            .FeatureIds({"screenplay-d9f79e27-bec2-4d15-9ba3-ae2bcd1e4bb5",
                         "screenplay-11d2d28c-28bf-430c-8dd1-c747c6c2f228"}),
        TestCase("createFolderDownloads")
            .InGuestMode()
            .FeatureIds({"screenplay-d9f79e27-bec2-4d15-9ba3-ae2bcd1e4bb5",
                         "screenplay-11d2d28c-28bf-430c-8dd1-c747c6c2f228"}),
        TestCase("createFolderNestedDownloads")
            .FeatureIds({"screenplay-d9f79e27-bec2-4d15-9ba3-ae2bcd1e4bb5",
                         "screenplay-11d2d28c-28bf-430c-8dd1-c747c6c2f228"}),
        TestCase("createFolderDrive")
            .FeatureIds({"screenplay-d9f79e27-bec2-4d15-9ba3-ae2bcd1e4bb5",
                         "screenplay-11d2d28c-28bf-430c-8dd1-c747c6c2f228"})));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    KeyboardOperations, /* keyboard_operations.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("keyboardDeleteDownloads").InGuestMode(),
                      TestCase("keyboardDeleteDownloads"),
                      TestCase("keyboardDeleteDrive"),
                      TestCase("keyboardDeleteFolderDownloads").InGuestMode(),
                      TestCase("keyboardDeleteFolderDownloads"),
                      TestCase("keyboardDeleteFolderDrive"),
                      TestCase("keyboardCopyDownloads").InGuestMode(),
                      TestCase("keyboardCopyDownloads"),
                      TestCase("keyboardCopyDownloads").EnableConflictDialog(),
                      TestCase("keyboardCopyDrive"),
                      TestCase("keyboardCopyDrive").EnableConflictDialog(),
// TODO(crbug.com/1236842): Remove flakiness and enable this test.
#if !defined(ADDRESS_SANITIZER) && defined(NDEBUG)
                      TestCase("keyboardFocusOutlineVisible"),
                      TestCase("keyboardFocusOutlineVisibleMouse"),
#endif
                      TestCase("keyboardSelectDriveDirectoryTree"),
                      TestCase("keyboardDisableCopyWhenDialogDisplayed"),
                      TestCase("keyboardOpenNewWindow"),
                      TestCase("keyboardOpenNewWindow").InGuestMode(),
                      TestCase("noPointerActiveOnTouch"),
                      TestCase("pointerActiveRemovedByTouch"),
                      TestCase("renameFileDownloads"),
                      TestCase("renameFileDownloads").InGuestMode(),
                      TestCase("renameFileDrive"),
                      TestCase("renameNewFolderDownloads"),
                      TestCase("renameNewFolderDownloads").InGuestMode(),
                      TestCase("renameRemovableWithKeyboardOnFileList")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    ContextMenu, /* context_menu.js for file list */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("checkDeleteEnabledForReadWriteFile"),
        TestCase("checkDeleteDisabledForReadOnlyDocument"),
        TestCase("checkDeleteDisabledForReadOnlyFile"),
        TestCase("checkDeleteDisabledForReadOnlyFolder"),
        TestCase("checkRenameEnabledForReadWriteFile"),
        TestCase("checkRenameDisabledForReadOnlyDocument"),
        TestCase("checkRenameDisabledForReadOnlyFile"),
        TestCase("checkRenameDisabledForReadOnlyFolder"),
        TestCase("checkContextMenuForRenameInput"),
        TestCase("checkShareEnabledForReadWriteFile"),
        TestCase("checkShareEnabledForReadOnlyDocument"),
        TestCase("checkShareDisabledForStrictReadOnlyDocument"),
        TestCase("checkShareEnabledForReadOnlyFile"),
        TestCase("checkShareEnabledForReadOnlyFolder"),
        TestCase("checkCopyEnabledForReadWriteFile"),
        TestCase("checkCopyEnabledForReadOnlyDocument"),
        TestCase("checkCopyDisabledForStrictReadOnlyDocument"),
        TestCase("checkCopyEnabledForReadOnlyFile"),
        TestCase("checkCopyEnabledForReadOnlyFolder"),
        TestCase("checkCutEnabledForReadWriteFile"),
        TestCase("checkCutDisabledForReadOnlyDocument"),
        TestCase("checkCutDisabledForReadOnlyFile"),
        TestCase("checkDlpRestrictionDetailsDisabledForNonDlpFiles"),
        TestCase("checkCutDisabledForReadOnlyFolder"),
        TestCase("checkPasteIntoFolderEnabledForReadWriteFolder"),
        TestCase("checkPasteIntoFolderDisabledForReadOnlyFolder"),
        // TODO(b/189173190): Enable
        // TestCase("checkInstallWithLinuxDisabledForDebianFile"),
        TestCase("checkInstallWithLinuxEnabledForDebianFile"),
        TestCase("checkImportCrostiniImageEnabled"),
        // TODO(b/189173190): Enable
        // TestCase("checkImportCrostiniImageDisabled"),
        TestCase("checkNewFolderEnabledInsideReadWriteFolder"),
        TestCase("checkNewFolderDisabledInsideReadOnlyFolder"),
        TestCase("checkPasteEnabledInsideReadWriteFolder"),
        TestCase("checkPasteDisabledInsideReadOnlyFolder"),
        TestCase("checkDownloadsContextMenu"),
        TestCase("checkPlayFilesContextMenu"),
        TestCase("checkLinuxFilesContextMenu"),
        TestCase("checkDeleteDisabledInDocProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("checkDeleteEnabledInDocProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("checkRenameDisabledInDocProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("checkRenameEnabledInDocProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("checkContextMenuFocus"),
        TestCase("checkContextMenusForInputElements"),
        TestCase("checkDeleteEnabledInRecents"),
        TestCase("checkGoToFileLocationEnabledInRecents"),
        TestCase("checkGoToFileLocationDisabledInMultipleSelection"),
        TestCase("checkDefaultTask"),
        TestCase("checkPolicyAssignedDefaultHasManagedIcon")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Toolbar, /* toolbar.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("toolbarAltACommand"),
        TestCase("toolbarDeleteWithMenuItemNoEntrySelected"),
        TestCase("toolbarDeleteButtonOpensDeleteConfirmDialog"),
        TestCase("toolbarDeleteButtonKeepFocus"),
        TestCase("toolbarDeleteEntry"),
        TestCase("toolbarDeleteEntry").InGuestMode(),
        TestCase("toolbarMultiMenuFollowsButton"),
        TestCase("toolbarRefreshButtonHiddenInRecents"),
        TestCase("toolbarRefreshButtonHiddenForWatchableVolume"),
        TestCase("toolbarRefreshButtonShownForNonWatchableVolume")
            .EnableGenericDocumentsProvider(),
        TestCase("toolbarRefreshButtonWithSelection")
            .EnableGenericDocumentsProvider(),
        TestCase("toolbarSharesheetButtonWithSelection")
            .FeatureIds({"screenplay-195b5b1d-2f7f-45ae-be5c-18b9c5d17674",
                         "screenplay-54b29b90-e689-4745-af0d-f8d336be2d13"}),
        TestCase("toolbarSharesheetContextMenuWithSelection"),
        TestCase("toolbarSharesheetNoEntrySelected"),
        TestCase("toolbarCloudIconShouldNotShowWhenBulkPinningDisabled"),
        TestCase("toolbarCloudIconShouldNotShowIfPreferenceDisabledAndNoUIState"
                 "Available")
            .EnableBulkPinning(),
        TestCase("toolbarCloudIconShouldShowForInProgress").EnableBulkPinning(),
        TestCase("toolbarCloudIconShowsWhenNotEnoughDiskSpaceIsReturned")
            .EnableBulkPinning(),
        TestCase("toolbarCloudIconShouldNotShowWhenCannotGetFreeSpace")
            .EnableBulkPinning(),
        TestCase("toolbarCloudIconWhenPressedShouldOpenCloudPanel")
            .EnableBulkPinning(),
        TestCase("toolbarCloudIconShouldNotShowWhenPrefDisabled")
            .EnableBulkPinning(),
        TestCase("toolbarCloudIconShouldShowOnStartupEvenIfSyncing")
            .EnableBulkPinning(),
        TestCase("toolbarCloudIconShouldShowWhenPausedState")
            .EnableBulkPinning()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    QuickView, /* quick_view.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("openQuickView"),
        TestCase("openQuickViewDialog"),
        TestCase("openQuickViewAndEscape"),
        TestCase("openQuickView").InGuestMode(),
        TestCase("openQuickView").TabletMode(),
        TestCase("openQuickViewViaContextMenuSingleSelection"),
        TestCase("openQuickViewViaContextMenuCheckSelections"),
        TestCase("openQuickViewAudio"),
        TestCase("openQuickViewAudioOnDrive"),
        TestCase("openQuickViewAudioWithImageMetadata"),
        TestCase("openQuickViewImageJpg"),
        TestCase("openQuickViewImageJpeg"),
        TestCase("openQuickViewImageJpeg").InGuestMode(),
        TestCase("openQuickViewImageExif"),
        TestCase("openQuickViewImageRaw"),
        TestCase("openQuickViewImageRawWithOrientation"),
        TestCase("openQuickViewImageWebp"),
        TestCase("openQuickViewBrokenImage"),
        TestCase("openQuickViewImageClick"),
        TestCase("openQuickViewVideo"),
        TestCase("openQuickViewVideoOnDrive"),
        TestCase("openQuickViewPdf"),
        TestCase("openQuickViewPdfPopup"),
#if !defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
        // TODO(http://crbug.com/1291090): Flaky on ASan non-DEBUG.
        TestCase("openQuickViewPdfPreviewsDisabled"),
#endif
        TestCase("openQuickViewKeyboardUpDownChangesView"),
        TestCase("openQuickViewKeyboardLeftRightChangesView"),
        TestCase("openQuickViewSniffedText"),
        TestCase("openQuickViewTextFileWithUnknownMimeType"),
        TestCase("openQuickViewUtf8Text"),
        TestCase("openQuickViewScrollText"),
        TestCase("openQuickViewScrollHtml"),
        TestCase("openQuickViewMhtml"),
        TestCase("openQuickViewBackgroundColorHtml"),
        TestCase("openQuickViewDrive"),
        TestCase("openQuickViewSmbfs"),
        TestCase("openQuickViewAndroid"),
        TestCase("openQuickViewAndroidGuestOs").EnableArcVm(),
        TestCase("openQuickViewDocumentsProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("openQuickViewCrostini"),
        TestCase("openQuickViewGuestOs"),
        TestCase("openQuickViewLastModifiedMetaData")
            .EnableGenericDocumentsProvider(),
        TestCase("openQuickViewUsb"),
        TestCase("openQuickViewRemovablePartitions"),
        TestCase("openQuickViewTrash")
            .FeatureIds({"screenplay-42720cab-fbc3-4ca2-bcc9-35d74c084bdc"}),
        TestCase("openQuickViewMtp"),
        TestCase("openQuickViewTabIndexImage"),
        TestCase("openQuickViewTabIndexText"),
        TestCase("openQuickViewTabIndexHtml"),
        TestCase("openQuickViewTabIndexAudio"),
        TestCase("openQuickViewTabIndexVideo"),
        TestCase("openQuickViewTabIndexDeleteDialog")
            .FeatureIds({"screenplay-42720cab-fbc3-4ca2-bcc9-35d74c084bdc"}),
        TestCase("openQuickViewToggleInfoButtonKeyboard"),
        TestCase("openQuickViewToggleInfoButtonClick"),
        TestCase("openQuickViewWithMultipleFiles"),
        TestCase("openQuickViewWithMultipleFilesText"),
        TestCase("openQuickViewWithMultipleFilesPdf"),
        TestCase("openQuickViewWithMultipleFilesKeyboardUpDown"),
        TestCase("openQuickViewWithMultipleFilesKeyboardLeftRight"),
        TestCase("openQuickViewFromDirectoryTree"),
        TestCase("openQuickViewAndDeleteSingleSelection")
            .FeatureIds({"screenplay-42720cab-fbc3-4ca2-bcc9-35d74c084bdc"}),
        TestCase("openQuickViewAndDeleteCheckSelection")
            .FeatureIds({"screenplay-42720cab-fbc3-4ca2-bcc9-35d74c084bdc"}),
        TestCase("openQuickViewDeleteEntireCheckSelection")
            .FeatureIds({"screenplay-42720cab-fbc3-4ca2-bcc9-35d74c084bdc"}),
        TestCase("openQuickViewClickDeleteButton")
            .FeatureIds({"screenplay-42720cab-fbc3-4ca2-bcc9-35d74c084bdc"}),
        TestCase("openQuickViewDeleteButtonNotShown"),
        TestCase("openQuickViewUmaViaContextMenu"),
        TestCase("openQuickViewUmaForCheckSelectViaContextMenu"),
        TestCase("openQuickViewUmaViaSelectionMenu"),
        TestCase("openQuickViewUmaViaSelectionMenuKeyboard"),
        TestCase("openQuickViewEncryptedFile"),
        TestCase("closeQuickView")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DirectoryTree, /* directory_tree.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("directoryTreeActiveDirectory"),
        TestCase("directoryTreeSelectedDirectory"),
        // TODO(b/189173190): Enable
        // TestCase("directoryTreeRecentsSubtypeScroll"),
        TestCase("directoryTreeHorizontalScroll"),
        TestCase("directoryTreeExpandHorizontalScroll"),
        TestCase("directoryTreeExpandHorizontalScrollRTL"),
        TestCase("directoryTreeVerticalScroll"),
        TestCase("directoryTreeExpandFolder"),
        TestCase("directoryTreeExpandFolder").FilesExperimental(),
        TestCase(
            "directoryTreeExpandFolderWithHiddenFileAndShowHiddenFilesOff"),
        TestCase("directoryTreeExpandFolderWithHiddenFileAndShowHiddenFilesOn"),
        TestCase("directoryTreeExpandFolderOnNonDelayExpansionVolume"),
        TestCase("directoryTreeExpandFolderOnDelayExpansionVolume")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DirectoryTreeContextMenu, /* directory_tree_context_menu.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("dirCopyWithContextMenu").InGuestMode(),
        TestCase("dirCopyWithContextMenu"),
        TestCase("dirCopyWithKeyboard").InGuestMode(),
        TestCase("dirCopyWithKeyboard"),
        TestCase("dirCopyWithoutChangingCurrent"),
        // TODO(b/189173190): Enable
        // TestCase("dirCutWithContextMenu"),
        // TODO(b/189173190): Enable
        // TestCase("dirCutWithContextMenu").InGuestMode(),
        // TODO(b/189173190): Enable
        // TestCase("dirCutWithKeyboard"),
        // TODO(b/189173190): Enable
        // TestCase("dirCutWithKeyboard").InGuestMode(),
        TestCase("dirPasteWithContextMenu"),
        TestCase("dirPasteWithContextMenu").InGuestMode(),
        TestCase("dirPasteWithoutChangingCurrent"),
        // TODO(b/189173190): Enable
        // TestCase("dirPasteWithoutChangingCurrent"),
        TestCase("dirRenameWithContextMenu"),
        TestCase("dirRenameWithContextMenu").InGuestMode(),
        TestCase("dirRenameUpdateChildrenBreadcrumbs"),
        TestCase("dirRenameWithKeyboard"),
        TestCase("dirRenameWithKeyboard").InGuestMode(),
        TestCase("dirRenameWithoutChangingCurrent"),
        TestCase("dirRenameToEmptyString"),
        TestCase("dirRenameToEmptyString").InGuestMode(),
        TestCase("dirRenameToExisting"),
#if !defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
        // TODO(http://crbug.com/1230054): Flaky on ASan non-DEBUG.
        TestCase("dirRenameToExisting").InGuestMode(),
#endif
        TestCase("dirRenameRemovableWithKeyboard"),
        TestCase("dirRenameRemovableWithKeyboard").InGuestMode(),
        TestCase("dirRenameRemovableWithContentMenu"),
        TestCase("dirRenameRemovableWithContentMenu").InGuestMode(),
        TestCase("dirContextMenuForRenameInput"),
        TestCase("dirCreateWithContextMenu"),
        TestCase("dirCreateWithKeyboard"),
        TestCase("dirCreateWithoutChangingCurrent"),
        TestCase("dirCreateMultipleFolders"),
        TestCase("dirContextMenuZip"),
        TestCase("dirContextMenuZipEject"),
        TestCase("dirContextMenuRecent"),
        TestCase("dirContextMenuMyFiles"),
        TestCase("dirContextMenuMyFilesWithPaste"),
        TestCase("dirContextMenuCrostini"),
        TestCase("dirContextMenuPlayFiles"),
        TestCase("dirContextMenuUsbs"),
        TestCase("dirContextMenuUsbs").EnableSinglePartitionFormat(),
        TestCase("dirContextMenuFsp"),
        TestCase("dirContextMenuDocumentsProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("dirContextMenuUsbDcim"),
        TestCase("dirContextMenuUsbDcim").EnableSinglePartitionFormat(),
        TestCase("dirContextMenuMtp"),
        TestCase("dirContextMenuMyDrive"),
        TestCase("dirContextMenuSharedDrive"),
        TestCase("dirContextMenuSharedWithMe"),
        TestCase("dirContextMenuOffline"),
        TestCase("dirContextMenuComputers"),
        TestCase("dirContextMenuTrash"),
        TestCase("dirContextMenuShortcut"),
        TestCase("dirContextMenuFocus"),
        TestCase("dirContextMenuKeyboardNavigation")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DriveSpecific, /* drive_specific.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("driveOpenSidebarOffline").EnableGenericDocumentsProvider(),
        TestCase("driveOpenSidebarSharedWithMe"),
        TestCase("driveAutoCompleteQuery"),
        TestCase("drivePinMultiple"),
        TestCase("drivePinHosted"),
        // TODO(b/189173190): Enable
        // TestCase("drivePinFileMobileNetwork"),
        TestCase("drivePinToggleUpdatesInFakeEntries"),
        TestCase("drivePinToggleUpdatesInFakeEntries").EnableCrosComponents(),
        TestCase("drivePinToggleIsDisabledAndHiddenWhenBulkPinningEnabled")
            .EnableBulkPinning(),
        TestCase("drivePinToggleIsDisabledAndHiddenWhenBulkPinningEnabled")
            .EnableBulkPinning()
            .EnableCrosComponents(),
        TestCase("driveClickFirstSearchResult"),
        TestCase("drivePressEnterToSearch"),
        TestCase("drivePressClearSearch"),
        TestCase("driveSearchAlwaysDisplaysMyDrive"),
        TestCase("drivePressCtrlAFromSearch"),
        TestCase("driveAvailableOfflineGearMenu"),
        TestCase("driveAvailableOfflineDirectoryGearMenu"),
        TestCase("driveAvailableOfflineActionBar"),
        TestCase("driveAvailableOfflineActionBar").EnableCrosComponents(),
        TestCase("driveLinkToDirectory"),
        TestCase("driveLinkToDirectory").EnableDriveShortcuts(),
        TestCase("driveLinkOpenFileThroughLinkedDirectory"),
        TestCase("driveLinkOpenFileThroughTransitiveLink"),
        TestCase("driveWelcomeBanner"),
        TestCase("driveWelcomeBanner").EnableCrosComponents(),
        TestCase("driveOfflineInfoBanner"),
        TestCase("driveEncryptionBadge"),
        TestCase("driveDeleteDialogDoesntMentionPermanentDelete"),
        TestCase("driveInlineSyncStatusSingleFile").EnableInlineSyncStatus(),
        TestCase("driveInlineSyncStatusParentFolder").EnableInlineSyncStatus(),
        TestCase("driveInlineSyncStatusSingleFileProgressEvents")
            .EnableInlineSyncStatusProgressEvents(),
        TestCase("driveInlineSyncStatusParentFolderProgressEvents")
            .EnableInlineSyncStatusProgressEvents(),
        TestCase("driveFoldersRetainPinnedPropertyWhenBulkPinningEnabled")
            .EnableBulkPinning(),
        TestCase("drivePinToggleIsEnabledInSharedWithMeWhenBulkPinningEnabled")
            .EnableBulkPinning(),
        TestCase("drivePinToggleIsEnabledInSharedWithMeWhenBulkPinningEnabled")
            .EnableBulkPinning()
            .EnableCrosComponents(),
        TestCase("driveCantPinItemsShouldHaveClassNameAndGetUpdatedWhenCanPin")
            .EnableBulkPinning(),
        TestCase("driveItemsOutOfViewportShouldUpdateTheirSyncStatus")
            .EnableBulkPinning()
            .EnableInlineSyncStatusProgressEvents(),
        TestCase("driveAllItemsShouldBeQueuedIfTrackedByPinManager")
            .EnableBulkPinning()
            .EnableInlineSyncStatusProgressEvents(),
        TestCase("driveDirtyItemsShouldBeDisplayedAsQueued")
            .EnableInlineSyncStatusProgressEvents(),
        TestCase("openDriveDocWhenOffline")
            .EnableBulkPinning()
            .EnableInlineSyncStatusProgressEvents(),
        TestCase("completedSyncStatusDismissesAfter300Ms")
            .EnableInlineSyncStatusProgressEvents(),
        TestCase("driveOutOfOrganizationSpaceBanner")
        // TODO(b/189173190): Enable
        // TestCase("driveEnableDocsOfflineDialog"),
        // TODO(b/189173190): Enable
        // TestCase("driveEnableDocsOfflineDialogWithoutWindow"),
        // TODO(b/189173190): Enable
        // TestCase("driveEnableDocsOfflineDialogMultipleWindows"),
        // TODO(b/189173190): Enable
        // TestCase("driveEnableDocsOfflineDialogDisappearsOnUnmount")
        ));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    HoldingSpace, /* holding_space.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("holdingSpaceWelcomeBanner"),
        TestCase("holdingSpaceWelcomeBanner").EnableCrosComponents(),
        TestCase("holdingSpaceWelcomeBannerWillShowForModalDialogs")
            .WithBrowser(),
        TestCase("holdingSpaceWelcomeBannerOnTabletModeChanged")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Transfer, /* transfer.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("transferFromDriveToDownloads")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferOfficeFileFromDriveToDownloads")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToMyFiles")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToMyFilesMove")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToDrive")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromSharedWithMeToDownloads")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromSharedWithMeToDrive")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToSharedFolder")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToSharedFolderMove")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromSharedFolderToDownloads")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromOfflineToDownloads")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromOfflineToDrive")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromTeamDriveToDrive")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDriveToTeamDrive")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromTeamDriveToDownloads")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferHostedFileFromTeamDriveToDownloads")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToTeamDrive")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferBetweenTeamDrives")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferDragDropActiveLeave")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferDragDropActiveDrop")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
// TODO(crbug.com/1236842): Remove flakiness and enable this test.
#if !defined(ADDRESS_SANITIZER) && defined(NDEBUG)
        TestCase("transferDragDropTreeItemDenies")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
#endif
        TestCase("transferDragAndHoverTreeItemEntryList")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
// TODO(crbug.com/1236842): Remove flakiness and enable this test.
#if !defined(ADDRESS_SANITIZER) && defined(NDEBUG)
        TestCase("transferDragAndHoverTreeItemFakeEntry")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferDragAndHoverTreeItemFakeEntry")
            .EnableSinglePartitionFormat()
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
#endif
        TestCase("transferDragFileListItemSelects")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferDragAndDrop")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferDragAndDropFolder")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferDragAndHover")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferDropBrowserFile")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferFromDownloadsToDownloads")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferDeletedFile")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        // TODO(b/189173190): Enable
        // TestCase("transferInfoIsRemembered"),
        // TODO(lucmult): Re-enable this once SWA uses the feedback panel.
        // TestCase("transferToUsbHasDestinationText"),
        // TODO(lucmult): Re-enable this once SWA uses the feedback panel.
        // TestCase("transferDismissedErrorIsRemembered"),
        TestCase("transferNotSupportedOperationHasNoRemainingTimeText")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferUpdateSamePanelItem")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"}),
        TestCase("transferShowPreparingMessageForZeroRemainingTime")
            .FeatureIds({"screenplay-9e3628b5-86db-481f-8623-f13eac08d61a"})));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DLP, /* dlp.js */
    DlpFilesAppBrowserTest,
    ::testing::Values(
        TestCase("transferShowDlpToast").EnableDlp(),
        TestCase("dlpShowManagedIcon").EnableDlp(),
        TestCase("dlpContextMenuRestrictionDetails").EnableDlp(),
        TestCase("saveAsDlpRestrictedAndroid").EnableArcVm().EnableDlp(),
        TestCase("saveAsDlpRestrictedCrostini").EnableDlp(),
        TestCase("saveAsDlpRestrictedVm").EnableDlp(),
        TestCase("saveAsDlpRestrictedUsb").EnableDlp(),
        TestCase("saveAsDlpRestrictedDrive").EnableDlp(),
        TestCase("saveAsNonDlpRestricted").EnableDlp(),
        TestCase("saveAsDlpRestrictedRedirectsToMyFiles").EnableDlp(),
        TestCase("openDlpRestrictedFile").EnableDlp(),
// TODO(b/290329625): Enable this once we identify a way to collect coverage
// when windows are closed before the test finishes.
#if !BUILDFLAG(USE_JAVASCRIPT_COVERAGE)
        TestCase("openFolderDlpRestricted").EnableDlp(),
#endif
        TestCase("fileTasksDlpRestricted").EnableDlp(),
        TestCase("zipExtractRestrictedArchiveCheckContent").EnableDlp(),
        TestCase("blockShowsPanelItem").EnableDlp(),
        TestCase("warnShowsPanelItem").EnableDlp()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DriveSpecific, /* drive_specific.js */
    LoggedInUserFilesAppBrowserTest,
    ::testing::Values(
        // Google One offer banner checks device state. Device state is NOT set
        // to `policy::DeviceMode::DEVICE_MODE_CONSUMER` in
        // `FilesAppBrowserTest`.
        TestCase("driveGoogleOneOfferBannerEnabled")
            .SetDeviceMode(DeviceMode::kConsumerOwned)
            .SetTestAccountType(TestAccountType::kNonManaged)
            .EnableGoogleOneOfferFilesBanner(),
        // Google One offer banner is disabled by default.
        TestCase("driveGoogleOneOfferBannerDisabled")
            .SetDeviceMode(DeviceMode::kConsumerOwned)
            .SetTestAccountType(TestAccountType::kNonManaged),
        TestCase("driveGoogleOneOfferBannerDismiss")
            .SetDeviceMode(DeviceMode::kConsumerOwned)
            .SetTestAccountType(TestAccountType::kNonManaged)
            .EnableGoogleOneOfferFilesBanner(),
        TestCase("driveGoogleOneOfferBannerDismiss")
            .SetDeviceMode(DeviceMode::kConsumerOwned)
            .SetTestAccountType(TestAccountType::kNonManaged)
            .EnableGoogleOneOfferFilesBanner()
            .EnableCrosComponents(),
        TestCase("driveGoogleOneOfferBannerDisabled")
            .EnableGoogleOneOfferFilesBanner()
            .SetDeviceMode(DeviceMode::kConsumerOwned)
            .SetTestAccountType(TestAccountType::kEnterprise),
        TestCase("driveGoogleOneOfferBannerDisabled")
            .EnableGoogleOneOfferFilesBanner()
            .SetDeviceMode(DeviceMode::kConsumerOwned)
            .SetTestAccountType(TestAccountType::kChild),
        // Google One offer is for a device. The banner will not
        // be shown for an enrolled device.
        TestCase("driveGoogleOneOfferBannerDisabled")
            .EnableGoogleOneOfferFilesBanner()
            .SetDeviceMode(DeviceMode::kEnrolled)
            .SetTestAccountType(TestAccountType::kNonManaged),
        // We do not show a banner if a profile is not an owner profile.
        TestCase("driveGoogleOneOfferBannerDisabled")
            .EnableGoogleOneOfferFilesBanner()
            .SetDeviceMode(kConsumerOwned)
            .SetTestAccountType(kNonManagedNonOwner),
        TestCase("driveBulkPinningBannerDisabled")
            .EnableBulkPinning()
            .SetDeviceMode(DeviceMode::kConsumerOwned)
            .SetTestAccountType(TestAccountType::kEnterprise),
        TestCase("driveBulkPinningBannerDisabled")
            .EnableBulkPinning()
            .SetDeviceMode(DeviceMode::kConsumerOwned)
            .SetTestAccountType(TestAccountType::kChild),
        TestCase("driveBulkPinningBannerDisabled")
            .EnableBulkPinning()
            .SetDeviceMode(DeviceMode::kEnrolled)
            .SetTestAccountType(TestAccountType::kChild),
        TestCase("driveBulkPinningBannerDisabled")
            .EnableBulkPinning()
            .SetDeviceMode(DeviceMode::kEnrolled)
            .SetTestAccountType(TestAccountType::kEnterprise),
        TestCase("driveBulkPinningBannerDisabled")
            .EnableBulkPinning()
            .SetDeviceMode(DeviceMode::kConsumerOwned)
            .SetTestAccountType(TestAccountType::kEnterprise)
            .EnableCrosComponents(),
        TestCase("driveBulkPinningBannerDisabled")
            .EnableBulkPinning()
            .SetDeviceMode(DeviceMode::kConsumerOwned)
            .SetTestAccountType(TestAccountType::kChild)
            .EnableCrosComponents(),
        TestCase("driveBulkPinningBannerDisabled")
            .EnableBulkPinning()
            .SetDeviceMode(DeviceMode::kEnrolled)
            .SetTestAccountType(TestAccountType::kChild)
            .EnableCrosComponents(),
        TestCase("driveBulkPinningBannerDisabled")
            .EnableBulkPinning()
            .SetDeviceMode(DeviceMode::kEnrolled)
            .SetTestAccountType(TestAccountType::kEnterprise)
            .EnableCrosComponents(),
        TestCase("driveBulkPinningBannerEnabled")
            .EnableBulkPinning()
            .SetDeviceMode(DeviceMode::kConsumerOwned)
            .SetTestAccountType(TestAccountType::kNonManaged),
        TestCase("driveBulkPinningBannerEnabled")
            .EnableBulkPinning()
            .SetDeviceMode(DeviceMode::kConsumerOwned)
            .SetTestAccountType(TestAccountType::kNonManagedNonOwner),
        TestCase("driveBulkPinningBannerEnabled")
            .EnableBulkPinning()
            .SetDeviceMode(DeviceMode::kEnrolled)
            .SetTestAccountType(TestAccountType::kGoogler),
        TestCase("driveBulkPinningBannerEnabled")
            .EnableBulkPinning()
            .SetDeviceMode(DeviceMode::kConsumerOwned)
            .SetTestAccountType(TestAccountType::kNonManaged)
            .EnableCrosComponents(),
        TestCase("driveBulkPinningBannerEnabled")
            .EnableBulkPinning()
            .SetDeviceMode(DeviceMode::kConsumerOwned)
            .SetTestAccountType(TestAccountType::kNonManagedNonOwner)
            .EnableCrosComponents(),
        TestCase("driveBulkPinningBannerEnabled")
            .EnableBulkPinning()
            .SetDeviceMode(DeviceMode::kEnrolled)
            .SetTestAccountType(TestAccountType::kGoogler)
            .EnableCrosComponents()));

#define FILE_TRANSFER_TEST_CASE(name) \
  TestCase(name).EnableFileTransferConnector()

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileTransferConnector, /* file_transfer_connector.js */
    FileTransferConnectorFilesAppBrowserTest,
    ::testing::Values(
        FILE_TRANSFER_TEST_CASE(
            "transferConnectorFromAndroidFilesToDownloadsDeep"),
        FILE_TRANSFER_TEST_CASE(
            "transferConnectorFromAndroidFilesToDownloadsFlat"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromCrostiniToDownloadsDeep"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromCrostiniToDownloadsFlat"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromDriveToDownloadsDeep"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromDriveToDownloadsDeep")
            .FileTransferConnectorReportOnlyMode(),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromDriveToDownloadsFlat"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromDriveToDownloadsFlat")
            .FileTransferConnectorReportOnlyMode(),
        FILE_TRANSFER_TEST_CASE(
            "transferConnectorFromDriveToDownloadsMoveDeep"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromDriveToDownloadsMoveDeep")
            .FileTransferConnectorReportOnlyMode(),
        FILE_TRANSFER_TEST_CASE(
            "transferConnectorFromDriveToDownloadsMoveFlat"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromDriveToDownloadsMoveFlat")
            .FileTransferConnectorReportOnlyMode(),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromMtpToDownloadsDeep"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromMtpToDownloadsFlat"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromSmbfsToDownloadsDeep"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromSmbfsToDownloadsFlat"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromUsbToDownloadsDeep"),
        FILE_TRANSFER_TEST_CASE("transferConnectorFromUsbToDownloadsFlat")));

#undef FILE_TRANSFER_TEST_CASE

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    RestorePrefs, /* restore_prefs.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("restoreSortColumn").InGuestMode(),
                      TestCase("restoreSortColumn"),
                      TestCase("restoreCurrentView").InGuestMode(),
                      TestCase("restoreCurrentView")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    ShareAndManageDialog, /* share_and_manage_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("shareFileDrive"),
        TestCase("shareDirectoryDrive"),
        TestCase("shareHostedFileDrive"),
        TestCase("manageHostedFileDrive")
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"}),
        TestCase("manageFileDrive")
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"}),
        TestCase("manageDirectoryDrive")
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"}),
        TestCase("shareFileTeamDrive"),
        TestCase("shareDirectoryTeamDrive"),
        TestCase("shareHostedFileTeamDrive"),
        TestCase("shareTeamDrive"),
        TestCase("manageHostedFileTeamDrive")
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"}),
        TestCase("manageFileTeamDrive")
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"}),
        TestCase("manageDirectoryTeamDrive")
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"}),
        TestCase("manageTeamDrive")
            .FeatureIds({"screenplay-c8094019-e19b-4a03-8085-83bc29f1dad6"})));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Traverse, /* traverse.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("traverseDownloads").InGuestMode(),
                      TestCase("traverseDownloads"),
                      TestCase("traverseDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Tasks, /* tasks.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("executeDefaultTaskDownloads"),
                      TestCase("executeDefaultTaskDownloads").InGuestMode(),
                      TestCase("executeDefaultTaskDrive"),
                      TestCase("defaultTaskForPdf"),
                      TestCase("defaultTaskForTextPlain"),
                      TestCase("defaultTaskDialogDownloads"),
                      TestCase("defaultTaskDialogDownloads").InGuestMode(),
                      TestCase("defaultTaskDialogDrive"),
                      TestCase("changeDefaultDialogScrollList"),
                      TestCase("genericTaskIsNotExecuted"),
                      TestCase("genericTaskAndNonGenericTask"),
                      TestCase("executeViaDblClick"),
                      TestCase("noActionBarOpenForDirectories")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FolderShortcuts, /* folder_shortcuts.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("traverseFolderShortcuts")
            .FeatureIds({"screenplay-42c556fb-303c-45b2-910b-3ecc5ec71b92"}),
        TestCase("addRemoveFolderShortcuts")
            .FeatureIds({"screenplay-1ae94bd0-60a7-4bb9-925d-78312d7c045d"})));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    SortColumns, /* sort_columns.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("sortColumns"),
                      TestCase("sortColumns").InGuestMode()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    TabIndex, /* tab_index.js: */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("tabindexSearchBoxFocus"),
        TestCase("tabindexFocus"),
        TestCase("tabindexFocus").EnableCrosComponents(),
        TestCase("tabindexFocusDownloads"),
        TestCase("tabindexFocusDownloads").EnableCrosComponents(),
        TestCase("tabindexFocusDownloads").InGuestMode(),
        TestCase("tabindexFocusDirectorySelected"),
        TestCase("tabindexFocusDirectorySelected").EnableCrosComponents(),
        TestCase("tabindexOpenDialogDownloads").WithBrowser(),
        TestCase("tabindexOpenDialogDownloads")
            .WithBrowser()
            .EnableCrosComponents()
        // TODO(b/189173190): Enable
        // TestCase("tabindexOpenDialogDownloads").WithBrowser(),
        // TODO(b/189173190): Enable
        // TestCase("tabindexOpenDialogDownloads").WithBrowser().InGuestMode(),
        // TODO(crbug.com/1236842): Remove flakiness and enable this test.
        //      ,
        //      TestCase("tabindexSaveFileDialogDrive").WithBrowser(),
        //      TestCase("tabindexSaveFileDialogDownloads").WithBrowser(),
        //      TestCase("tabindexSaveFileDialogDownloads").WithBrowser().InGuestMode()
        ));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileDialog, /* file_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("openFileDialogUnload").WithBrowser(),
        TestCase("openFileDialogDownloads")
            .WithBrowser()
            .FeatureIds({"screenplay-a63f2d5c-2cf8-4b5d-97fa-cd1f34004556"}),
        TestCase("openFileDialogDownloads")
            .WithBrowser()
            .InGuestMode()
            .FeatureIds({"screenplay-a63f2d5c-2cf8-4b5d-97fa-cd1f34004556"}),
        // TestCase("openFileDialogDownloads").WithBrowser().InIncognito(),
        // TestCase("openFileDialogDownloads")
        //     .WithBrowser()
        //     .InIncognito()
        TestCase("openFileDialogPanelsDisabled").WithBrowser(),
        TestCase("openFileDialogAriaMultipleSelect")
            .WithBrowser()
            .FeatureIds({"screenplay-af443ca0-6d9f-4cb3-af8f-0939c37833db"}),
        TestCase("saveFileDialogAriaSingleSelect")
            .WithBrowser()
            .FeatureIds({"screenplay-af443ca0-6d9f-4cb3-af8f-0939c37833db"}),
        TestCase("saveFileDialogDownloads")
            .WithBrowser()
            .FeatureIds({"screenplay-17a056b4-ed53-415f-a186-99204a7c2a21"}),
        TestCase("saveFileDialogDownloads")
            .WithBrowser()
            .InGuestMode()
            .FeatureIds({"screenplay-17a056b4-ed53-415f-a186-99204a7c2a21"}),
        // TODO(b/194255793): Fix this.
        // TestCase("saveFileDialogDownloads")
        //     .WithBrowser()
        //     .InIncognito()
        // TODO(crbug.com/1236842): Remove flakiness and enable this test.
        // TestCase("saveFileDialogDownloadsNewFolderButton").WithBrowser(),
        TestCase("saveFileDialogDownloadsNewFolderButton").WithBrowser(),
        TestCase("saveFileDialogPanelsDisabled").WithBrowser(),
        TestCase("openFileDialogCancelDownloads").WithBrowser(),
        TestCase("openFileDialogEscapeDownloads").WithBrowser(),
        TestCase("openFileDialogDrive")
            .WithBrowser()
            .FeatureIds({"screenplay-a63f2d5c-2cf8-4b5d-97fa-cd1f34004556"}),
        // TODO(b/194255793): Fix this.
        // TestCase("openFileDialogDrive").WithBrowser().InIncognito(),
        TestCase("saveFileDialogDrive")
            .WithBrowser()
            .FeatureIds({"screenplay-17a056b4-ed53-415f-a186-99204a7c2a21"}),
        // TODO(b/194255793): Fix this.
        // TestCase("saveFileDialogDrive").WithBrowser().InIncognito(),
        // TODO(b/194255793): Fix this.
        // TestCase("openFileDialogDriveFromBrowser").WithBrowser(),
        // TODO(b/194255793): Fix this.
        // TestCase("openFileDialogDriveHostedDoc").WithBrowser(),
        TestCase("openFileDialogDriveHostedNeedsFile").WithBrowser(),
        TestCase("saveFileDialogDriveHostedNeedsFile").WithBrowser(),
        TestCase("openFileDialogDriveCSEGrey").WithBrowser(),
        TestCase("openFileDialogDriveCSENeedsFile").WithBrowser(),
        TestCase("openFileDialogDriveOfficeFile").WithBrowser(),
        TestCase("openMultiFileDialogDriveOfficeFile")
            .WithBrowser()
            .FeatureIds({"screenplay-3337ab4d-3c77-4908-a9ec-e43d2f52cd1f"}),
        TestCase("openFileDialogCancelDrive").WithBrowser(),
        TestCase("openFileDialogEscapeDrive").WithBrowser(),
        TestCase("openFileDialogDriveOffline").WithBrowser().Offline(),
        TestCase("saveFileDialogDriveOffline").WithBrowser().Offline(),
        TestCase("openFileDialogDriveOfflinePinned").WithBrowser().Offline(),
        TestCase("saveFileDialogDriveOfflinePinned").WithBrowser().Offline(),
        TestCase("openFileDialogDefaultFilter")
            .WithBrowser()
            .FeatureIds({"screenplay-29790711-ea7b-4ad2-9596-83b98edbca8d"}),
        TestCase("saveFileDialogDefaultFilter").WithBrowser(),
        TestCase("saveFileDialogDefaultFilterKeyNavigation").WithBrowser(),
        TestCase("saveFileDialogSingleFilterNoAcceptAll").WithBrowser(),
        TestCase("saveFileDialogExtensionNotAddedWithNoFilter").WithBrowser(),
        TestCase("saveFileDialogExtensionAddedWithJpegFilter").WithBrowser(),
        TestCase("saveFileDialogExtensionNotAddedWhenProvided").WithBrowser(),
        TestCase("openFileDialogFileListShowContextMenu").WithBrowser(),
        TestCase("openFileDialogSelectAllDisabled").WithBrowser(),
        TestCase("openMultiFileDialogSelectAllEnabled")
            .WithBrowser()
            .FeatureIds({"screenplay-3337ab4d-3c77-4908-a9ec-e43d2f52cd1f"}),
        TestCase("saveFileDialogGuestOs").WithBrowser(),
        TestCase("saveFileDialogGuestOs").WithBrowser().InIncognito(),
        TestCase("openFileDialogGuestOs").WithBrowser(),
        TestCase("openFileDialogGuestOs").WithBrowser().InIncognito()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    CopyBetweenWindows, /* copy_between_windows.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("copyBetweenWindowsLocalToDrive"),
                      TestCase("copyBetweenWindowsLocalToUsb"),
                      // TODO(b/189173190): Enable
                      // TestCase("copyBetweenWindowsUsbToDrive"),
                      TestCase("copyBetweenWindowsDriveToLocal"),
                      // TODO(b/189173190): Enable
                      // TestCase("copyBetweenWindowsDriveToUsb"),
                      TestCase("copyBetweenWindowsUsbToLocal")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    GridView, /* grid_view.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("showGridViewDownloads").InGuestMode(),
        TestCase("showGridViewDownloads"),
        TestCase("showGridViewButtonSwitches"),
        TestCase("showGridViewKeyboardSelectionA11y")
            .FeatureIds({"screenplay-af443ca0-6d9f-4cb3-af8f-0939c37833db"}),
        TestCase("showGridViewTitles"),
        TestCase("showGridViewMouseSelectionA11y")
            .FeatureIds({"screenplay-af443ca0-6d9f-4cb3-af8f-0939c37833db"}),
        TestCase("showGridViewDocumentsProvider")
            .EnableGenericDocumentsProvider(),
        TestCase("showGridViewEncryptedFile")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Providers, /* providers.js */
    ExtendedFilesAppBrowserTest,
    ::testing::Values(TestCase("requestMount"),
                      TestCase("requestMount").DisableNativeSmb(),
                      TestCase("requestMountMultipleMounts"),
                      TestCase("requestMountMultipleMounts").DisableNativeSmb(),
                      TestCase("requestMountSourceDevice"),
                      TestCase("requestMountSourceDevice").DisableNativeSmb(),
                      TestCase("requestMountSourceFile"),
                      TestCase("requestMountSourceFile").DisableNativeSmb(),
                      TestCase("providerEject"),
                      TestCase("providerEject").DisableNativeSmb()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    GearMenu, /* gear_menu.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("showHiddenFilesDownloads")
            .FeatureIds({"screenplay-616ee826-9b5f-4f5f-a516-f4a0d1123c8c"}),
        TestCase("showHiddenFilesDownloads")
            .InGuestMode()
            .FeatureIds({"screenplay-616ee826-9b5f-4f5f-a516-f4a0d1123c8c"}),
        TestCase("showHiddenFilesDrive")
            .FeatureIds({"screenplay-616ee826-9b5f-4f5f-a516-f4a0d1123c8c"}),
        TestCase("showPasteIntoCurrentFolder"),
        TestCase("showToggleHiddenAndroidFoldersGearMenuItemsInMyFiles"),
        TestCase("showSelectAllInCurrentFolder"),
        TestCase("enableToggleHiddenAndroidFoldersShowsHiddenFiles")
            .FeatureIds({"screenplay-09c32d6b-36d3-494b-bb83-e19655880471"}),
        TestCase("hideCurrentDirectoryByTogglingHiddenAndroidFolders"),
        TestCase("newFolderInDownloads"),
        TestCase("showFilesSettingsButton"),
        TestCase("showSendFeedbackAction")
            .FeatureIds({"screenplay-3bd7bbba-a25a-4386-93cf-933266df22a7"}),
        TestCase("showSendFeedbackAction")
            .EnableOsFeedback()
            .FeatureIds({"screenplay-3bd7bbba-a25a-4386-93cf-933266df22a7"}),
        TestCase("enableDisableStorageSettingsLink"),
        TestCase("showAvailableStorageMyFiles")
            .FeatureIds({"screenplay-56f7e10e-b7ba-4425-b397-14ce54d670dc"}),
        TestCase("showAvailableStorageDrive")
            .FeatureIds({"screenplay-56f7e10e-b7ba-4425-b397-14ce54d670dc"}),
        TestCase("showAvailableStorageSmbfs")
            .FeatureIds({"screenplay-56f7e10e-b7ba-4425-b397-14ce54d670dc"}),
        TestCase("showAvailableStorageDocProvider")
            .EnableGenericDocumentsProvider()
            .FeatureIds({"screenplay-56f7e10e-b7ba-4425-b397-14ce54d670dc"}),
        TestCase("openHelpPageFromDownloadsVolume"),
        TestCase("openHelpPageFromDriveVolume"),
        TestCase("showManageMirrorSyncShowsOnlyInLocalRoot"),
        TestCase("showManageMirrorSyncShowsOnlyInLocalRoot")
            .EnableMirrorSync()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FilesTooltip, /* files_tooltip.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("filesTooltipFocus"),
                      TestCase("filesTooltipLabelChange"),
                      TestCase("filesTooltipMouseOver"),
                      TestCase("filesTooltipMouseOverStaysOpen"),
                      TestCase("filesTooltipClickHides"),
                      TestCase("filesTooltipHidesOnWindowResize"),
                      TestCase("filesCardTooltipClickHides"),
                      TestCase("filesTooltipHidesOnDeleteDialogClosed")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileList, /* file_list.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("fileListAriaAttributes")
            .FeatureIds({"screenplay-af443ca0-6d9f-4cb3-af8f-0939c37833db"}),
        TestCase("fileListFocusFirstItem"),
        TestCase("fileListSelectLastFocusedItem"),
        TestCase("fileListSortWithKeyboard"),
        TestCase("fileListKeyboardSelectionA11y")
            .FeatureIds({"screenplay-af443ca0-6d9f-4cb3-af8f-0939c37833db"}),
        TestCase("fileListMouseSelectionA11y")
            .FeatureIds({"screenplay-af443ca0-6d9f-4cb3-af8f-0939c37833db"}),
        TestCase("fileListDeleteMultipleFiles"),
        TestCase("fileListRenameSelectedItem"),
        TestCase("fileListRenameFromSelectAll")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Crostini, /* crostini.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("mountCrostini"),
        TestCase("enableDisableCrostini"),
        TestCase("sharePathWithCrostini")
            .FeatureIds({"screenplay-122c00f8-9842-4666-8ca0-b6bf47454551"}),
        TestCase("pluginVmDirectoryNotSharedErrorDialog"),
        TestCase("pluginVmFileOnExternalDriveErrorDialog"),
        TestCase("pluginVmFileDropFailErrorDialog")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    MyFiles, /* my_files.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("directoryTreeRefresh")
            .FeatureIds({"screenplay-02521fe6-a9c5-4cd1-ac9b-cc46df33c1a0"}),
        TestCase("showMyFiles"),
        TestCase("myFilesDisplaysAndOpensEntries"),
        TestCase("myFilesFolderRename"),
        TestCase("myFilesUpdatesWhenAndroidVolumeMounts")
            .DontMountVolumes()
            .FeatureIds({"screenplay-e920978b-0184-4665-98a3-acc46dc48ce9"}),
        TestCase("myFilesUpdatesChildren"),
        TestCase("myFilesAutoExpandOnce"),
        TestCase("myFilesToolbarDelete")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Navigation, /* navigation.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("navigateToParent")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    InstallLinuxPackageDialog, /* install_linux_package_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("installLinuxPackageDialog")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Recents, /* recents.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("recentsA11yMessages")
            .FeatureIds({"screenplay-af443ca0-6d9f-4cb3-af8f-0939c37833db"}),
        TestCase("recentsAllowCutForDownloads"),
        TestCase("recentsAllowCutForDrive"),
        TestCase("recentsAllowCutForPlayFiles").EnableArc(),
        TestCase("recentsAllowDeletion").EnableArc(),
        TestCase("recentsAllowMultipleFilesDeletion").EnableArc(),
        TestCase("recentsAllowRename")
            .EnableArc()
            .FeatureIds({"screenplay-788b6d1f-0752-41e9-826e-bba324a19ef9"}),
        TestCase("recentsEmptyFolderMessage"),
        TestCase("recentsEmptyFolderMessageAfterDeletion"),
        TestCase("recentsDownloads"),
        TestCase("recentsDrive"),
        TestCase("recentsCrostiniNotMounted"),
        TestCase("recentsCrostiniMounted"),
        TestCase("recentsDownloadsAndDrive"),
        TestCase("recentsDownloadsAndDriveAndPlayFiles").EnableArc(),
        TestCase("recentsDownloadsAndDriveWithOverlap"),
        TestCase("recentsFilterResetToAll"),
        TestCase("recentsNested"),
        TestCase("recentsNoRenameForPlayFiles").EnableArc(),
        TestCase("recentsPlayFiles").EnableArc(),
        TestCase("recentsReadOnlyHidden"),
        TestCase("recentsRespectSearchWhenSwitchingFilter"),
        TestCase("recentsRespondToTimezoneChangeForGridView"),
        TestCase("recentsRespondToTimezoneChangeForListView"),
        TestCase("recentsTimePeriodHeadings"),
        TestCase("recentAudioDownloads"),
        TestCase("recentAudioDownloadsAndDrive"),
        TestCase("recentAudioDownloadsAndDriveAndPlayFiles").EnableArc(),
        TestCase("recentDocumentsDownloads"),
        TestCase("recentDocumentsDownloadsAndDrive"),
        TestCase("recentDocumentsDownloadsAndDriveAndPlayFiles").EnableArc(),
        TestCase("recentImagesDownloads"),
        TestCase("recentImagesDownloadsAndDrive"),
        TestCase("recentImagesDownloadsAndDriveAndPlayFiles").EnableArc(),
        TestCase("recentVideosDownloads"),
        TestCase("recentVideosDownloadsAndDrive"),
        TestCase("recentVideosDownloadsAndDriveAndPlayFiles").EnableArc()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Metadata, /* metadata.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("metadataDocumentsProvider").EnableGenericDocumentsProvider(),
        TestCase("metadataDownloads"),
        TestCase("metadataDrive"),
        TestCase("metadataTeamDrives"),
        TestCase("metadataLargeDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Search, /* search.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("searchDownloadsWithResults"),
        TestCase("searchDownloadsWithResults").EnableSearchV2(),
        TestCase("searchDownloadsWithNoResults"),
        TestCase("searchDownloadsWithNoResults").EnableSearchV2(),
        TestCase("searchDownloadsClearSearchKeyDown"),
        TestCase("searchDownloadsClearSearchKeyDown").EnableSearchV2(),
        TestCase("searchDownloadsClearSearch"),
        TestCase("searchDownloadsClearSearch").EnableSearchV2(),
        TestCase("searchHidingViaTab"),
        TestCase("searchHidingViaTab").EnableSearchV2(),
        TestCase("searchHidingTextEntryField"),
        TestCase("searchHidingTextEntryField").EnableSearchV2(),
        TestCase("searchButtonToggles"),
        TestCase("searchButtonToggles").EnableSearchV2(),
        TestCase("searchWithLocationOptions").EnableSearchV2(),
        TestCase("searchLocalWithTypeOptions").EnableSearchV2(),
        TestCase("searchDriveWithTypeOptions").EnableSearchV2(),
        TestCase("searchWithRecencyOptions").EnableSearchV2(),
        TestCase("searchDriveWithRecencyOptions").EnableSearchV2(),
        TestCase("searchRemovableDevice").EnableSearchV2(),
        TestCase("searchPartitionedRemovableDevice").EnableSearchV2(),
        TestCase("resetSearchOptionsOnFolderChange").EnableSearchV2(),
        TestCase("showSearchResultMessageWhenSearching").EnableSearchV2(),
        TestCase("showsEducationNudge").EnableSearchV2(),
        TestCase("searchFromMyFiles").EnableSearchV2(),
        TestCase("selectionPath").EnableSearchV2(),
        TestCase("searchHierarchy").EnableSearchV2(),
        TestCase("hideSearchInTrash").EnableSearchV2(),
// TODO(b/287169303): test is flaky on ChromiumOS MSan
#if !defined(MEMORY_SANITIZER)
        TestCase("searchTrashedFiles").EnableSearchV2(),
#endif
        TestCase("matchDriveFilesByName").EnableSearchV2(),
        TestCase("searchSharedWithMe").EnableSearchV2(),
        TestCase("searchDocumentsProvider")
            .EnableGenericDocumentsProvider()
            .EnableSearchV2(),
        TestCase("searchDocumentsProviderWithTypeOptions")
            .EnableGenericDocumentsProvider()
            .EnableSearchV2(),
        TestCase("searchDocumentsProviderWithRecencyOptions")
            .EnableGenericDocumentsProvider()
            .EnableSearchV2(),
        TestCase("searchFileSystemProvider").EnableSearchV2()
        // TODO(b/189173190): Enable
        // TestCase("searchQueryLaunchParam")
        ));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Metrics, /* metrics.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("metricsRecordEnum"),
// TODO(https://crbug.com/1303472): Fix flakes and re-enable.
#if !BUILDFLAG(IS_CHROMEOS)
                      TestCase("metricsRecordDirectoryListLoad"),
                      TestCase("metricsRecordUpdateAvailableApps"),
#endif
                      TestCase("metricsOpenSwa")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Breadcrumbs, /* breadcrumbs.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("breadcrumbsNavigate"),
                      TestCase("breadcrumbsDownloadsTranslation"),
                      TestCase("breadcrumbsRenderShortPath"),
                      TestCase("breadcrumbsEliderButtonNotExist"),
                      TestCase("breadcrumbsRenderLongPath"),
                      TestCase("breadcrumbsMainButtonClick"),
                      TestCase("breadcrumbsMainButtonEnterKey"),
                      TestCase("breadcrumbsEliderButtonClick"),
                      TestCase("breadcrumbsEliderButtonKeyboard"),
                      TestCase("breadcrumbsEliderMenuClickOutside"),
                      TestCase("breadcrumbsEliderMenuItemClick"),
                      TestCase("breadcrumbsEliderMenuItemTabLeft"),
                      TestCase("breadcrumbNavigateBackToSharedWithMe"),
                      TestCase("breadcrumbsEliderMenuItemTabRight")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FormatDialog, /* format_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("formatDialog"),
        TestCase("formatDialogIsModal"),
        TestCase("formatDialogEmpty"),
        TestCase("formatDialogCancel"),
        TestCase("formatDialogNameLength"),
        TestCase("formatDialogNameInvalid"),
        TestCase("formatDialogGearMenu"),
        TestCase("formatDialog").EnableSinglePartitionFormat(),
        TestCase("formatDialogIsModal").EnableSinglePartitionFormat(),
        TestCase("formatDialogEmpty").EnableSinglePartitionFormat(),
        TestCase("formatDialogCancel").EnableSinglePartitionFormat(),
        TestCase("formatDialogNameLength").EnableSinglePartitionFormat(),
        TestCase("formatDialogNameInvalid").EnableSinglePartitionFormat(),
        TestCase("formatDialogGearMenu").EnableSinglePartitionFormat()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Trash, /* trash.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("trashMoveToTrash")
            .FeatureIds({"screenplay-a06f961a-17f5-4fbd-8285-49abb000dee1"}),
        TestCase("trashPermanentlyDelete"),
        TestCase("trashRestoreFromToast"),
        TestCase("trashRestoreFromToast").EnableCrosComponents(),
        TestCase("trashRestoreFromTrash"),
        TestCase("trashRestoreFromTrashShortcut"),
        TestCase("trashEmptyTrash")
            .FeatureIds({"screenplay-38573550-c60a-4009-ba92-c0af1420fde6"}),
        TestCase("trashEmptyTrashShortcut")
            .FeatureIds({"screenplay-38573550-c60a-4009-ba92-c0af1420fde6"}),
        TestCase("trashDeleteFromTrash")
            .FeatureIds({"screenplay-38573550-c60a-4009-ba92-c0af1420fde6"}),
        TestCase("trashDeleteFromTrashOriginallyFromMyFiles")
            .FeatureIds({"screenplay-38573550-c60a-4009-ba92-c0af1420fde6"}),
        TestCase("trashDeleteFromTrashOriginallyFromDrive")
            .FeatureIds({"screenplay-38573550-c60a-4009-ba92-c0af1420fde6"})
            .EnableDriveTrash(),
        TestCase("trashNoTasksInTrashRoot"),
        TestCase("trashDoubleClickOnFileInTrashRootShowsDialog"),
        TestCase("trashDragDropRootAcceptsEntries"),
        TestCase("trashDragDropFromDisallowedRootsFails"),
        TestCase("trashDragDropNonModifiableEntriesCantBeTrashed"),
        TestCase("trashDragDropRootPerformsTrashAction"),
        TestCase("trashTraversingFolderShowsDisallowedDialog"),
        TestCase("trashDontShowTrashRootOnSelectFileDialog"),
        TestCase("trashDontShowTrashRootWhenOpeningAsAndroidFilePicker"),
        TestCase("trashEnsureOldEntriesArePeriodicallyRemoved"),
        TestCase("trashDragDropOutOfTrashPerformsRestoration"),
        TestCase("trashRestorationDialogInProgressDoesntShowUndo"),
        TestCase("trashRestorationDialogInProgressDoesntShowUndo")
            .EnableCrosComponents(),
        TestCase("trashTogglingTrashEnabledNavigatesAwayFromTrashRoot"),
        TestCase("trashTogglingTrashEnabledPrefUpdatesDirectoryTree"),
        TestCase("trashCantRestoreWhenParentDoesntExist"),
        TestCase(
            "trashPressingEnterOnFileInTrashRootShowsDialogWithRestoreButton"),
        TestCase("trashInfeasibleActionsForFileDisabledAndHiddenInTrashRoot"),
        TestCase("trashInfeasibleActionsForFolderDisabledAndHiddenInTrashRoot"),
        TestCase("trashExtractAllForZipHiddenAndDisabledInTrashRoot"),
        TestCase("trashAllActionsDisabledForBlankSpaceInTrashRoot"),
        TestCase("trashNudgeShownOnFirstTrashOperation"),
        TestCase("trashStaleTrashInfoFilesAreRemovedAfterOneHour")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    AndroidPhotos, /* android_photos.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("androidPhotosBanner").EnablePhotosDocumentsProvider(),
        TestCase("androidPhotosBanner")
            .EnablePhotosDocumentsProvider()
            .EnableCrosComponents()));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    Office, /* office.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("openOfficeWordFile").EnableUploadOfficeToCloud(),
        TestCase("openOfficeWordFromMyFiles").EnableUploadOfficeToCloud(),
        TestCase("uploadToDriveRequiresUploadOfficeToCloudEnabled"),
        TestCase("openMultipleOfficeWordFromDrive").EnableUploadOfficeToCloud(),
        TestCase("openOfficeWordFromDrive").EnableUploadOfficeToCloud(),
        TestCase("openOfficeExcelFromDrive").EnableUploadOfficeToCloud(),
        TestCase("openOfficePowerPointFromDrive").EnableUploadOfficeToCloud(),
        TestCase("openOfficeWordFromDriveNotSynced")
            .EnableUploadOfficeToCloud(),
        TestCase("openOfficeWordFromMyFilesOffline")
            .EnableUploadOfficeToCloud()
            .Offline(),
        TestCase("openOfficeWordFromDriveOffline")
            .EnableUploadOfficeToCloud()
            .Offline(),
        TestCase("officeShowNudgeGoogleDrive")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    GuestOs, /* guest_os.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("fakesListed"),
                      TestCase("listUpdatedWhenGuestsChanged"),
                      TestCase("mountGuestSuccess"),
                      TestCase("mountAndroidVolumeSuccess").EnableArcVm()));

}  // namespace file_manager
