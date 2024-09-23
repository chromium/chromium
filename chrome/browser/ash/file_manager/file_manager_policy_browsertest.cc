// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/config/coverage/buildflags.h"
#include "chrome/browser/ash/file_manager/file_manager_browsertest_base.h"
#include "chrome/browser/ash/file_manager/file_manager_browsertest_utils.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_error_dialog.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_warn_dialog.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"
#include "chrome/browser/ash/policy/dlp/test/mock_dlp_files_controller_ash.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
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
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/file_access/test/mock_scoped_file_access_delegate.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/textarea/textarea.h"

using file_manager::test::TestCase;

namespace file_manager {
namespace {

// DLP source URLs
constexpr char kBlockedSourceUrl[] = "https://blocked.com";
constexpr char kWarnSourceUrl[] = "https://warned.com";
constexpr char kNotSetSourceUrl[] = "https://not-set.com";
constexpr char kNotBlockedSourceUrl[] = "https://allowed.com";

constexpr char16_t kUserJustification[] = u"User justification";

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

// Base class for DLP setup needed for browsertests.
class DlpFilesAppBrowserTestBase {
 public:
  DlpFilesAppBrowserTestBase(const DlpFilesAppBrowserTestBase&) = delete;
  DlpFilesAppBrowserTestBase& operator=(const DlpFilesAppBrowserTestBase&) =
      delete;

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager =
        std::make_unique<testing::NiceMock<policy::MockDlpRulesManager>>(
            Profile::FromBrowserContext(context));
    mock_rules_manager_ = dlp_rules_manager.get();
    ON_CALL(*mock_rules_manager_, IsFilesPolicyEnabled)
        .WillByDefault(testing::Return(true));

    files_controller_ = std::make_unique<policy::DlpFilesControllerAsh>(
        *mock_rules_manager_, Profile::FromBrowserContext(context));
    ON_CALL(*mock_rules_manager_, GetDlpFilesController)
        .WillByDefault(testing::Return(files_controller_.get()));

    return dlp_rules_manager;
  }

 protected:
  DlpFilesAppBrowserTestBase() = default;
  ~DlpFilesAppBrowserTestBase() = default;

  bool HandleDlpCommands(Profile* profile,
                         const std::string& name,
                         const base::Value::Dict& value,
                         std::string* output) {
    if (name == "setGetFilesSourcesMock") {
      base::FilePath result =
          file_manager::util::GetDownloadsFolderForProfile(profile);
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
          base::BindRepeating(&DlpFilesAppBrowserTestBase::GetFilesSourcesMock,
                              base::Unretained(this), response));
      return true;
    }
    if (name == "setBlockedFilesTransfer") {
      base::FilePath result =
          file_manager::util::GetDownloadsFolderForProfile(profile);
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
          file_manager::util::GetDownloadsFolderForProfile(profile);
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
    if (name == "setCheckFilesTransferMockToPause") {
      base::FilePath download_path =
          file_manager::util::GetDownloadsFolderForProfile(profile);
      std::optional<int> task_id = value.FindInt("taskId");
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
              profile);
      EXPECT_TRUE(fpnm);
      // Might be needed to time out the warning.
      fpnm->SetTaskRunnerForTesting(task_runner);

      auto cb = base::BindLambdaForTesting(
          [task_id, warning_files, action, profile](
              const dlp::CheckFilesTransferRequest,
              chromeos::DlpClient::CheckFilesTransferCallback daemon_callback) {
            auto warning_callback = base::BindOnce(
                [](chromeos::DlpClient::CheckFilesTransferCallback daemon_cb,
                   std::optional<std::u16string> justification,
                   bool should_proceed) {
                  if (should_proceed) {
                    std::move(daemon_cb).Run({});
                  }
                },
                std::move(daemon_callback));
            policy::FilesPolicyNotificationManager* fpnm =
                policy::FilesPolicyNotificationManagerFactory::
                    GetForBrowserContext(profile);
            ASSERT_TRUE(fpnm);
            ASSERT_TRUE(fpnm->HasIOTask(task_id.value()));
            // Call FPNM to show the warning, which pauses the task.
            fpnm->ShowDlpWarning(std::move(warning_callback), task_id,
                                 std::move(warning_files),
                                 policy::DlpFileDestination(), action);
          });
      chromeos::DlpClient::Get()->GetTestInterface()->SetIsAlive(true);
      chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferMock(
          cb);
      return true;
    }
    if (name == "timeoutWarning") {
      // Fast forward by 5 minutes to time out the DLP warning.
      task_runner->FastForwardBy(base::Minutes(5));
      return true;
    }
    return false;
  }

  // MockDlpRulesManager is owned by KeyedService and is guaranteed to outlive
  // this class.
  raw_ptr<policy::MockDlpRulesManager, DanglingUntriaged> mock_rules_manager_ =
      nullptr;

  std::unique_ptr<policy::DlpFilesControllerAsh> files_controller_;

  std::unique_ptr<file_access::MockScopedFileAccessDelegate>
      scoped_file_access_delegate_;

  // The callback needs to survive the setup method.
  base::MockRepeatingCallback<void(
      const ::dlp::AddFilesRequest request,
      chromeos::DlpClient::AddFilesCallback callback)>
      add_files_cb;

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();

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
};

// Returns a file transfer connectors policy for DLP with the given settings.
std::string GetFileTransferConnectorsPolicyForDlp(
    const std::string& source,
    const std::string& destination,
    bool report_only,
    bool require_user_justification) {
  auto sources = base::Value::List().Append(
      base::Value::Dict().Set("file_system_type", source));

  auto destinations = base::Value::List().Append(
      base::Value::Dict().Set("file_system_type", destination));

  auto source_destination_list = base::Value::List().Append(
      base::Value::Dict()
          .Set("sources", std::move(sources))
          .Set("destinations", std::move(destinations)));

  auto enable = base::Value::List().Append(
      base::Value::Dict()
          .Set("source_destination_list", std::move(source_destination_list))
          .Set("tags", base::Value::List().Append("dlp")));

  auto settings = base::Value::Dict();
  settings.Set("service_provider", "google");
  settings.Set("enable", std::move(enable));
  settings.Set("block_until_verdict", report_only ? 0 : 1);

  if (require_user_justification) {
    settings.Set("require_justification_tags",
                 base::Value::List().Append("dlp"));
  }

  return settings.DebugString();
}

base::TimeDelta kResponseDelay = base::Seconds(0);

const std::set<std::string>* JpgMimeTypes() {
  static std::set<std::string> set = {"image/jpeg"};
  return &set;
}

// Base class for Enterprise connectrs setup needed for browsertests.
class FileTransferConnectorFilesAppBrowserTestBase {
 public:
  FileTransferConnectorFilesAppBrowserTestBase(
      const FileTransferConnectorFilesAppBrowserTestBase&) = delete;
  FileTransferConnectorFilesAppBrowserTestBase& operator=(
      const FileTransferConnectorFilesAppBrowserTestBase&) = delete;

 protected:
  FileTransferConnectorFilesAppBrowserTestBase() = default;
  ~FileTransferConnectorFilesAppBrowserTestBase() = default;

  void SetUpOnMainThread(Profile* profile) {
    // Set a device management token. It is required to enable scanning.
    // Without it, FileTransferAnalysisDelegate::IsEnabled() always
    // returns std::nullopt.
    SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));

    // Enable reporting.
    enterprise_connectors::test::SetOnSecurityEventReporting(
        profile->GetPrefs(),
        /*enabled*/ true,
        /*enabled_event_names*/ {},
        /*enabled_opt_in_events*/ {},
        /*machine_scope*/ false);
    // Add mock to check reports.
    cloud_policy_client_ = std::make_unique<policy::MockCloudPolicyClient>();
    cloud_policy_client_->SetDMToken("dm_token");
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        profile)
        ->SetBrowserCloudPolicyClientForTesting(cloud_policy_client_.get());
    // Add IdentityTestEnvironment to verify user name.
    identity_test_environment_ =
        std::make_unique<signin::IdentityTestEnvironment>();
    identity_test_environment_->MakePrimaryAccountAvailable(
        kUserName, signin::ConsentLevel::kSync);
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        profile)
        ->SetIdentityManagerForTesting(
            identity_test_environment_->identity_manager());
  }

  std::string GetScanIDForFileName(std::string file_name) {
    return std::string(kScanId) + file_name;
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
        .WillOnce([this, delegate](base::OnceClosure callback) {
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
              base::BindOnce(&FileTransferConnectorFilesAppBrowserTestBase::
                                 ScanningHasCompletedCallback,
                             base::Unretained(this))));
        });

    // Call GetWarnedFiles from the base class.
    EXPECT_CALL(*delegate, GetWarnedFiles()).WillRepeatedly([delegate]() {
      return delegate->FileTransferAnalysisDelegate::GetWarnedFiles();
    });

    // Call GetAnalysisResultAfterScan from the base class.
    EXPECT_CALL(*delegate, GetAnalysisResultAfterScan(::testing::_))
        .WillRepeatedly([delegate](storage::FileSystemURL url) {
          return delegate
              ->FileTransferAnalysisDelegate::GetAnalysisResultAfterScan(url);
        });
  }

  bool HandleEnterpriseConnectorCommands(
      Profile* profile,
      const FileManagerBrowserTestBase::Options& options,
      const std::string& name,
      const base::Value::Dict& value,
      std::string* output) {
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
          profile->GetPrefs(), enterprise_connectors::FILE_TRANSFER,
          GetFileTransferConnectorsPolicyForDlp(
              *source, *destination,
              options.file_transfer_connector_report_only,
              options.bypass_requires_justification));

      // Create a FakeFilesRequestHandler that intercepts uploads and fakes
      // responses.
      enterprise_connectors::FilesRequestHandler::SetFactoryForTesting(
          base::BindRepeating(
              &enterprise_connectors::test::FakeFilesRequestHandler::Create,
              base::BindRepeating(
                  &FileTransferConnectorFilesAppBrowserTestBase::
                      FakeFileUploadCallback,
                  base::Unretained(this), *source, *destination)));

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
                  &FileTransferConnectorFilesAppBrowserTestBase::SetupMock,
                  base::Unretained(this))));

      return true;
    }
    if (name == "issueFileTransferResponses") {
      // Issue all saved responses and issue all future responses directly.
      IssueResponses();
      return true;
    }
    if (name == "isReportOnlyFileTransferConnector") {
      *output = options.file_transfer_connector_report_only ? "true" : "false";
      return true;
    }
    if (name == "usesNewFileTransferConnectorUI") {
      *output =
          options.enable_file_transfer_connector_new_ux ? "true" : "false";
      return true;
    }
    if (name == "getExpectedNumberOfBlockedFilesByConnectors") {
      *output = base::NumberToString(expected_blocked_files_.size());
      return true;
    }
    if (name == "getExpectedNumberOfWarnedFilesByConnectors") {
      *output = base::NumberToString(expected_warned_files_.size());
      return true;
    }
    if (name == "doesBypassRequireJustification") {
      *output = options.bypass_requires_justification ? "true" : "false";
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
      std::optional<bool> expect_proceed_warning_reports_optional =
          value.FindBool("expect_proceed_warning_reports");
      bool expect_proceed_warning_reports =
          expect_proceed_warning_reports_optional.value_or(false);

      std::vector<std::string> file_names;
      std::vector<std::string> shas;
      std::vector<enterprise_connectors::ContentAnalysisResponse::Result>
          expected_dlp_verdicts;
      std::vector<std::string> expected_results;
      std::vector<std::string> expected_scan_ids;

      for (const auto& path_value : *entry_paths) {
        const std::string* path_str = path_value.GetIfString();
        CHECK(path_str);
        base::FilePath path(*path_str);

        auto file_name = path.BaseName().AsUTF8Unsafe();

        bool should_block = base::Contains(file_name, "blocked");
        bool should_warn = base::Contains(file_name, "warned");
        CHECK(!(should_block && should_warn))
            << "A file shouldn't be both blocked and warned.";
        if (!should_block && !should_warn) {
          // If a file name contains neither blocked nor warned, expect no
          // report.
          continue;
        }

        if (expect_proceed_warning_reports && !should_warn) {
          // If we are expecting proceed warning reports, then we can ignore
          // blocked files.
          continue;
        }

        file_names.push_back(file_name);
        // sha256sum chrome/test/data/chromeos/file_manager/small.jpg |  tr
        // '[:lower:]' '[:upper:]'
        shas.push_back(
            "28F5754447BBA26238B93B820DFFCB6743876F8A82077BA1ABB0F4B2529AE5BE");

        // Get the expected verdict from the ConnectorStatusCallback.
        expected_dlp_verdicts.push_back(
            ConnectorStatusCallback(path).results()[0]);

        if (!expect_proceed_warning_reports) {
          if (should_block) {
            expected_blocked_files_.push_back(file_name);
          } else if (should_warn) {
            expected_warned_files_.push_back(file_name);
          }
        }

        // For report-only mode, the transfer is always allowed. It's blocked,
        // otherwise.
        expected_results.push_back(safe_browsing::EventResultToString(
            options.file_transfer_connector_report_only
                ? safe_browsing::EventResult::ALLOWED
                : (should_warn ? (expect_proceed_warning_reports
                                      ? safe_browsing::EventResult::BYPASSED
                                      : safe_browsing::EventResult::WARNED)
                               : safe_browsing::EventResult::BLOCKED)));
        expected_scan_ids.push_back(GetScanIDForFileName(file_name));
      }

      validator_ =
          std::make_unique<enterprise_connectors::test::EventReportValidator>(
              cloud_policy_client());
      validator_->ExpectSensitiveDataEvents(
          /*url*/ "",
          /*tab_url*/ "",
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
          /*profile_identifier*/ profile->GetPath().AsUTF8Unsafe(),
          /*scan_ids*/ expected_scan_ids,
          /*content_transfer_method*/ std::nullopt,
          /*user_justification*/
          expect_proceed_warning_reports &&
                  options.bypass_requires_justification
              ? std::make_optional(kUserJustification)
              : std::nullopt);

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
    } else if (base::Contains(path.BaseName().value(), "warned")) {
      response = enterprise_connectors::test::FakeContentAnalysisDelegate::
          FakeContentAnalysisDelegate::DlpResponse(
              enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS,
              "rule", enterprise_connectors::TriggeredRule::WARN);
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

  std::vector<std::string> expected_blocked_files_;
  std::vector<std::string> expected_warned_files_;

  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

// A version of FilesAppBrowserTest that supports DLP files restrictions.
class DlpFilesAppBrowserTest
    : public FileManagerBrowserTestBase,
      public ::testing::WithParamInterface<file_manager::test::TestCase>,
      public DlpFilesAppBrowserTestBase {
 public:
  DlpFilesAppBrowserTest(const DlpFilesAppBrowserTest&) = delete;
  DlpFilesAppBrowserTest& operator=(const DlpFilesAppBrowserTest&) = delete;

 protected:
  DlpFilesAppBrowserTest() = default;
  ~DlpFilesAppBrowserTest() override = default;

  void SetUpOnMainThread() override {
    FileManagerBrowserTestBase::SetUpOnMainThread();
    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&DlpFilesAppBrowserTestBase::SetDlpRulesManager,
                            base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    // Make sure the rules manager does not return a freed files controller.
    ON_CALL(*mock_rules_manager_, GetDlpFilesController)
        .WillByDefault(testing::Return(nullptr));

    // The files controller must be destroyed before the profile since it's
    // holding a pointer to it.
    files_controller_.reset();

    FileManagerBrowserTestBase::TearDownOnMainThread();
  }

  bool HandleDlpCommands(const std::string& name,
                         const base::Value::Dict& value,
                         std::string* output) override {
    return DlpFilesAppBrowserTestBase::HandleDlpCommands(profile(), name, value,
                                                         output);
  }

  const char* GetTestCaseName() const override { return GetParam().name; }

  std::string GetFullTestCaseName() const override {
    return GetParam().GetFullName();
  }

  const char* GetTestExtensionManifestName() const override {
    return "file_manager_test_manifest.json";
  }

  FileManagerBrowserTestBase::Options GetOptions() const override {
    return GetParam().options;
  }
};

IN_PROC_BROWSER_TEST_P(DlpFilesAppBrowserTest, Test) {
  ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
  ON_CALL(*mock_rules_manager_, IsRestricted)
      .WillByDefault(::testing::Return(policy::DlpRulesManager::Level::kAllow));
  ON_CALL(*mock_rules_manager_, GetReportingManager)
      .WillByDefault(::testing::Return(nullptr));

  StartTest();
}

// A version of FilesAppBrowserTest that supports the file transfer enterprise
// connector.
class FileTransferConnectorFilesAppBrowserTest
    : public FileManagerBrowserTestBase,
      public ::testing::WithParamInterface<file_manager::test::TestCase>,
      public FileTransferConnectorFilesAppBrowserTestBase {
 public:
  FileTransferConnectorFilesAppBrowserTest(
      const FileTransferConnectorFilesAppBrowserTest&) = delete;
  FileTransferConnectorFilesAppBrowserTest& operator=(
      const FileTransferConnectorFilesAppBrowserTest&) = delete;

 protected:
  FileTransferConnectorFilesAppBrowserTest() = default;
  ~FileTransferConnectorFilesAppBrowserTest() override = default;

  void SetUpOnMainThread() override {
    FileManagerBrowserTestBase::SetUpOnMainThread();
    FileTransferConnectorFilesAppBrowserTestBase::SetUpOnMainThread(profile());
  }

  bool HandleEnterpriseConnectorCommands(const std::string& name,
                                         const base::Value::Dict& value,
                                         std::string* output) override {
    if (name == "verifyFileTransferErrorDialogAndDismiss") {
      const std::string* app_id = value.FindString("app_id");
      CHECK_NE(app_id, nullptr);
      VerifyFileTransferErrorDialogAndDismiss(*app_id);
      return true;
    }
    if (name == "verifyFileTransferWarningDialogAndProceed") {
      const std::string* app_id = value.FindString("app_id");
      CHECK_NE(app_id, nullptr);
      VerifyFileTransferWarningDialogAndProceed(
          *app_id, GetOptions().bypass_requires_justification);
      return true;
    } else {
      return FileTransferConnectorFilesAppBrowserTestBase::
          HandleEnterpriseConnectorCommands(profile(), GetOptions(), name,
                                            value, output);
    }
  }

  const char* GetTestCaseName() const override { return GetParam().name; }

  std::string GetFullTestCaseName() const override {
    return GetParam().GetFullName();
  }

  const char* GetTestExtensionManifestName() const override {
    return "file_manager_test_manifest.json";
  }

  FileManagerBrowserTestBase::Options GetOptions() const override {
    return GetParam().options;
  }

  void VerifyFileTransferErrorDialogAndDismiss(const std::string& app_id) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::WebContents* web_contents = GetWebContentsForId(app_id);
    CHECK_NE(web_contents, nullptr);
    gfx::NativeWindow native_window = web_contents->GetTopLevelNativeWindow();

    std::set<raw_ptr<views::Widget, SetExperimental>> owned_widgets;
    views::Widget::GetAllOwnedWidgets(native_window, &owned_widgets);

    // Verify that the FilesPolicyErrorDialog widget is displayed.
    ASSERT_EQ(owned_widgets.size(), 1ul);
    auto* widget = (*owned_widgets.begin()).get();
    ASSERT_EQ(widget->GetName(), "FilesPolicyErrorDialog");

    auto* view = widget->GetRootView()->GetViewByID(
        policy::PolicyDialogBase::kScrollViewId);
    ASSERT_TRUE(view);

    // Verify the displayed blocked files shown in the dialog.
    std::vector<std::string> displayed_files;
    for (const views::View* row_view : view->children()) {
      const views::Label* label =
          static_cast<const views::Label*>(row_view->GetViewByID(
              policy::PolicyDialogBase::kConfidentialRowTitleViewId));
      if (label) {
        displayed_files.push_back(base::UTF16ToUTF8(label->GetText()));
      }
    }
    EXPECT_THAT(displayed_files,
                ::testing::UnorderedElementsAreArray(expected_blocked_files_));

    // Close the dialog.
    auto* dialog = static_cast<policy::FilesPolicyErrorDialog*>(
        widget->widget_delegate()->AsDialogDelegate());
    dialog->AcceptDialog();

    // Verify that the dialog is closed.
    EXPECT_TRUE(widget->IsClosed());
  }

  void VerifyFileTransferWarningDialogAndProceed(
      const std::string& app_id,
      bool bypass_requires_justification) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::WebContents* web_contents = GetWebContentsForId(app_id);
    CHECK_NE(web_contents, nullptr);
    gfx::NativeWindow native_window = web_contents->GetTopLevelNativeWindow();

    std::set<raw_ptr<views::Widget, SetExperimental>> owned_widgets;
    views::Widget::GetAllOwnedWidgets(native_window, &owned_widgets);

    // Verify that the FilesPolicyWarnDialog widget is displayed.
    ASSERT_EQ(owned_widgets.size(), 1ul);
    auto* widget = (*owned_widgets.begin()).get();
    ASSERT_EQ(widget->GetName(), "FilesPolicyWarnDialog");

    auto* view = widget->GetRootView()->GetViewByID(
        policy::PolicyDialogBase::kScrollViewId);
    ASSERT_TRUE(view);

    // Verify the displayed blocked files shown in the dialog.
    std::vector<std::string> displayed_files;
    for (const views::View* row_view : view->children()) {
      const views::Label* label =
          static_cast<const views::Label*>(row_view->GetViewByID(
              policy::PolicyDialogBase::kConfidentialRowTitleViewId));
      if (label) {
        displayed_files.push_back(base::UTF16ToUTF8(label->GetText()));
      }
    }
    EXPECT_THAT(displayed_files,
                ::testing::UnorderedElementsAreArray(expected_warned_files_));

    policy::FilesPolicyWarnDialog* dialog =
        static_cast<policy::FilesPolicyWarnDialog*>(
            widget->widget_delegate()->AsDialogDelegate());
    ASSERT_TRUE(dialog);

    // Verify that the dialog has a text area where the user can enter a
    // justification if required.
    views::Textarea* justification_area =
        static_cast<views::Textarea*>(widget->GetRootView()->GetViewByID(
            policy::PolicyDialogBase::
                kEnterpriseConnectorsJustificationTextareaId));
    if (bypass_requires_justification) {
      EXPECT_NE(justification_area, nullptr);
      EXPECT_FALSE(dialog->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

      justification_area->InsertText(
          kUserJustification,
          ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
      EXPECT_TRUE(dialog->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

    } else {
      EXPECT_EQ(justification_area, nullptr);
      EXPECT_TRUE(dialog->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
    }

    // Close the dialog.
    dialog->AcceptDialog();

    // Verify that the dialog is closed.
    EXPECT_TRUE(widget->IsClosed());
  }
};

IN_PROC_BROWSER_TEST_P(FileTransferConnectorFilesAppBrowserTest, Test) {
  StartTest();
}

// A version of FilesAppBrowserTest that supports DLP and Enterprise Connectors
// files restrictions.
class DlpAndEnterpriseConnectorsFilesAppBrowserTest
    : public FileManagerBrowserTestBase,
      public ::testing::WithParamInterface<file_manager::test::TestCase>,
      public DlpFilesAppBrowserTestBase,
      public FileTransferConnectorFilesAppBrowserTestBase {
 public:
  DlpAndEnterpriseConnectorsFilesAppBrowserTest(
      const DlpAndEnterpriseConnectorsFilesAppBrowserTest&) = delete;
  DlpAndEnterpriseConnectorsFilesAppBrowserTest& operator=(
      const DlpAndEnterpriseConnectorsFilesAppBrowserTest&) = delete;

 protected:
  DlpAndEnterpriseConnectorsFilesAppBrowserTest() = default;
  ~DlpAndEnterpriseConnectorsFilesAppBrowserTest() override = default;

  void SetUpOnMainThread() override {
    FileManagerBrowserTestBase::SetUpOnMainThread();
    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&DlpFilesAppBrowserTestBase::SetDlpRulesManager,
                            base::Unretained(this)));
    FileTransferConnectorFilesAppBrowserTestBase::SetUpOnMainThread(profile());
  }

  void TearDownOnMainThread() override {
    // The files controller must be destroyed before the profile since it's
    // holding a pointer to it.
    files_controller_.reset();
    FileManagerBrowserTestBase::TearDownOnMainThread();
  }

  bool HandleDlpCommands(const std::string& name,
                         const base::Value::Dict& value,
                         std::string* output) override {
    return DlpFilesAppBrowserTestBase::HandleDlpCommands(profile(), name, value,
                                                         output);
  }

  bool HandleEnterpriseConnectorCommands(const std::string& name,
                                         const base::Value::Dict& value,
                                         std::string* output) override {
    return FileTransferConnectorFilesAppBrowserTestBase::
        HandleEnterpriseConnectorCommands(profile(), GetOptions(), name, value,
                                          output);
  }

  const char* GetTestCaseName() const override { return GetParam().name; }

  std::string GetFullTestCaseName() const override {
    return GetParam().GetFullName();
  }

  const char* GetTestExtensionManifestName() const override {
    return "file_manager_test_manifest.json";
  }

  FileManagerBrowserTestBase::Options GetOptions() const override {
    return GetParam().options;
  }
};

IN_PROC_BROWSER_TEST_P(DlpAndEnterpriseConnectorsFilesAppBrowserTest, Test) {
  ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
  ON_CALL(*mock_rules_manager_, IsRestricted)
      .WillByDefault(::testing::Return(policy::DlpRulesManager::Level::kAllow));
  ON_CALL(*mock_rules_manager_, GetReportingManager)
      .WillByDefault(::testing::Return(nullptr));

  StartTest();
}

// A version of FilesAppBrowserTest with SkyVault restrictions.
class SkyVaultFilesAppBrowserTest
    : public FileManagerBrowserTestBase,
      public ::testing::WithParamInterface<file_manager::test::TestCase> {
 public:
  SkyVaultFilesAppBrowserTest(const SkyVaultFilesAppBrowserTest&) = delete;
  SkyVaultFilesAppBrowserTest& operator=(const SkyVaultFilesAppBrowserTest&) =
      delete;

 protected:
  SkyVaultFilesAppBrowserTest() = default;
  ~SkyVaultFilesAppBrowserTest() override = default;

  void TearDown() override {
    FileManagerBrowserTestBase::TearDown();
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
  }

  bool HandleSkyVaultCommands(const std::string& name,
                              const base::Value::Dict& value,
                              std::string* output) override {
    if (name == "skyvault:mountMyFiles") {
      my_files_dir_ = profile()->GetPath().Append("MyFiles");
      {
        base::ScopedAllowBlockingForTesting allow_blocking;
        CHECK(base::CreateDirectory(my_files_dir_));
      }
      std::string mount_point_name =
          file_manager::util::GetDownloadsMountPointName(profile());
      storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
          mount_point_name);
      CHECK(
          storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
              mount_point_name, storage::kFileSystemTypeLocal,
              storage::FileSystemMountOption(), my_files_dir_));
      file_manager::VolumeManager::Get(profile())
          ->RegisterDownloadsDirectoryForTesting(my_files_dir_);
      return true;
    }

    if (name == "skyvault:addLocalFiles") {
      const base::FilePath my_files = profile()->GetPath().Append("MyFiles");

      base::FilePath source_dir;
      CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_dir));
      const base::FilePath test_file_path = source_dir.AppendASCII("chrome")
                                                .AppendASCII("test")
                                                .AppendASCII("data")
                                                .AppendASCII("chromeos")
                                                .AppendASCII("file_manager")
                                                .AppendASCII("text.txt");

      CHECK(base::CopyFile(test_file_path, my_files.AppendASCII("hello.txt")));
      return true;
    }

    return false;
  }

  const char* GetTestCaseName() const override { return GetParam().name; }

  std::string GetFullTestCaseName() const override {
    return GetParam().GetFullName();
  }

  const char* GetTestExtensionManifestName() const override {
    return "file_manager_test_manifest.json";
  }

  FileManagerBrowserTestBase::Options GetOptions() const override {
    return GetParam().options;
  }

 private:
  base::FilePath my_files_dir_;
};

IN_PROC_BROWSER_TEST_P(SkyVaultFilesAppBrowserTest, Test) {
  StartTest();
}

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DLP, /* dlp.ts */
    DlpFilesAppBrowserTest,
    ::testing::Values(
        file_manager::test::TestCase("transferShowDlpToast").EnableDlp(),
        file_manager::test::TestCase("dlpShowManagedIcon").EnableDlp(),
        file_manager::test::TestCase("dlpContextMenuRestrictionDetails")
            .EnableDlp(),
        file_manager::test::TestCase("saveAsDlpRestrictedAndroid")
            .EnableArcVm()
            .EnableDlp(),
        file_manager::test::TestCase("saveAsDlpRestrictedCrostini").EnableDlp(),
        file_manager::test::TestCase("saveAsDlpRestrictedVm").EnableDlp(),
        file_manager::test::TestCase("saveAsDlpRestrictedUsb").EnableDlp(),
        file_manager::test::TestCase("saveAsDlpRestrictedDrive").EnableDlp(),
        file_manager::test::TestCase("saveAsNonDlpRestricted").EnableDlp(),
        file_manager::test::TestCase("saveAsDlpRestrictedRedirectsToMyFiles")
            .EnableDlp(),
        file_manager::test::TestCase("openDlpRestrictedFile").EnableDlp(),
// TODO(b/290329625): Enable this once we identify a way to collect coverage
// when windows are closed before the test finishes.
#if !BUILDFLAG(USE_JAVASCRIPT_COVERAGE)
        file_manager::test::TestCase("openFolderDlpRestricted").EnableDlp(),
#endif
        file_manager::test::TestCase("fileTasksDlpRestricted").EnableDlp(),
        file_manager::test::TestCase("zipExtractRestrictedArchiveCheckContent")
            .EnableDlp(),
        file_manager::test::TestCase("blockShowsPanelItem")
            .EnableDlp()
            .EnableFilesPolicyNewUX(),
        file_manager::test::TestCase("warnShowsPanelItem")
            .EnableDlp()
            .EnableFilesPolicyNewUX(),
        file_manager::test::TestCase("warnTimeoutShowsPanelItem")
            .EnableDlp()
            .EnableFilesPolicyNewUX(),
        file_manager::test::TestCase("mixedSummaryDisplayPanel")
            .EnableDlp()
            .EnableFilesPolicyNewUX()));

#define FILE_TRANSFER_TEST_CASE(name) \
  file_manager::test::TestCase(name).EnableFileTransferConnector()

// Enable both new policy UX and file transfer connector new UX, as the latter
// requires the former.
#define FILE_TRANSFER_TEST_CASE_NEW_UX(name) \
  file_manager::test::TestCase(name)         \
      .EnableFileTransferConnector()         \
      .EnableFilesPolicyNewUX()              \
      .EnableFileTransferConnectorNewUX()

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    FileTransferConnector, /* file_transfer_connector.ts */
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
        FILE_TRANSFER_TEST_CASE("transferConnectorFromDriveToDownloadsFlatDesti"
                                "nationNoSpaceForReportOnly")
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
        FILE_TRANSFER_TEST_CASE("transferConnectorFromUsbToDownloadsFlat"),
        FILE_TRANSFER_TEST_CASE_NEW_UX(
            "transferConnectorFromUsbToDownloadsDeepNewUX"),
        FILE_TRANSFER_TEST_CASE_NEW_UX(
            "transferConnectorFromUsbToDownloadsFlatNewUX"),
        FILE_TRANSFER_TEST_CASE_NEW_UX(
            "transferConnectorFromUsbToDownloadsDeepMoveNewUX"),
        FILE_TRANSFER_TEST_CASE_NEW_UX(
            "transferConnectorFromUsbToDownloadsFlatMoveNewUX"),
        FILE_TRANSFER_TEST_CASE_NEW_UX(
            "transferConnectorFromUsbToDownloadsFlatWarnProceedNewUX"),
        FILE_TRANSFER_TEST_CASE_NEW_UX("transferConnectorFromUsbToDownloadsFlat"
                                       "WarnProceedWithJustificationNewUX")
            .BypassRequiresJustification(),
        FILE_TRANSFER_TEST_CASE_NEW_UX(
            "transferConnectorFromUsbToDownloadsDeepWarnProceedNewUX"),
        FILE_TRANSFER_TEST_CASE_NEW_UX("transferConnectorFromUsbToDownloadsDeep"
                                       "WarnProceedWithJustificationNewUX")
            .BypassRequiresJustification(),
        FILE_TRANSFER_TEST_CASE_NEW_UX(
            "transferConnectorFromUsbToDownloadsFlatWarnCancelNewUX"),
        FILE_TRANSFER_TEST_CASE_NEW_UX(
            "transferConnectorFromUsbToDownloadsDeepWarnCancelNewUX")));

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    DlpEntrepriseConnectors, /* dlp_enterprise_connectors.ts */
    DlpAndEnterpriseConnectorsFilesAppBrowserTest,
    ::testing::Values(
        FILE_TRANSFER_TEST_CASE_NEW_UX("twoWarningsProceeded"),
        FILE_TRANSFER_TEST_CASE_NEW_UX("differentBlockPolicies")));

#undef FILE_TRANSFER_TEST_CASE
#undef FILE_TRANSFER_TEST_CASE_NEW_UX

WRAPPED_INSTANTIATE_TEST_SUITE_P(
    SkyVault, /* skyvault.ts */
    SkyVaultFilesAppBrowserTest,
    ::testing::Values(TestCase("fileDisplayLocalFilesDisabledUnmountRemovable")
                          .DontMountVolumes()
                          .EnableSkyVault(),
                      // TODO(b/347643334): Enable.
                      // TestCase("fileDisplayLocalFilesDisableInMyFiles")
                      //     .DontMountVolumes()
                      //     .EnableSkyVault(),
                      // TestCase("fileDisplayOneDrivePlaceholder")
                      //     .DontMountVolumes()
                      //     .EnableSkyVault(),
                      TestCase("fileDisplayFileSystemDisabled")
                          .DontMountVolumes()
                          .EnableSkyVault(),
                      TestCase("fileDisplaySkyVaultMigrationToGoogleDrive")
                          .DontMountVolumes()
                          .EnableSkyVault(),
                      TestCase("fileDisplaySkyVaultMigrationToOneDrive")
                          .DontMountVolumes()
                          .EnableSkyVault()));

}  // namespace file_manager
