// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <algorithm>
#include <memory>
#include <set>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_features.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_browsertest_base.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_sdk_manager.h"  // nogncheck
#endif

using extensions::SafeBrowsingPrivateEventRouter;
using safe_browsing::BinaryUploadService;
using safe_browsing::CloudBinaryUploadService;
using ::testing::_;
using ::testing::Mock;

namespace enterprise_connectors {

namespace {

constexpr char kUserName[] = "test@chromium.org";

constexpr char kScanId1[] = "scan id 1";
constexpr char kScanId2[] = "scan id 2";
constexpr char kScanId3[] = "scan id 3";

std::string text() {
  return std::string(100, 'a');
}

std::string image() {
  return std::string(50, 'a');
}

class FakeBinaryUploadService : public CloudBinaryUploadService {
 public:
  FakeBinaryUploadService()
      : CloudBinaryUploadService(nullptr, nullptr, nullptr) {}

  // Sets whether the user is authorized to upload data for Deep Scanning.
  void SetAuthorized(bool authorized) {
    authorization_result_ = authorized
                                ? BinaryUploadService::Result::SUCCESS
                                : BinaryUploadService::Result::UNAUTHORIZED;
  }

  // Finish the authentication request. Called after CreateForWebContents to
  // simulate an async callback.
  void ReturnAuthorizedResponse() {
    FinishRequest(authorization_request_.get(), authorization_result_,
                  ContentAnalysisResponse());
  }

  void SetResponseForText(BinaryUploadService::Result result,
                          const ContentAnalysisResponse& response) {
    prepared_text_result_ = result;
    prepared_text_response_ = response;
  }

  void SetResponseForImage(BinaryUploadService::Result result,
                           const ContentAnalysisResponse& response,
                           int image_size) {
    prepared_image_result_ = result;
    prepared_image_response_ = response;
    prepared_image_data_size_ = image_size;
  }

  void SetResponseForFile(const std::string& path,
                          BinaryUploadService::Result result,
                          const ContentAnalysisResponse& response) {
    prepared_file_results_[path] = result;
    prepared_file_responses_[path] = response;
  }

  void SetExpectedFinalAction(
      const std::string& request_token,
      ContentAnalysisAcknowledgement::FinalAction final_action) {
    request_tokens_to_final_actions_[request_token] = final_action;
  }

  void SetShouldAutomaticallyAuthorize(bool authorize) {
    should_automatically_authorize_ = authorize;
  }

  int requests_count() const { return requests_count_; }
  int ack_count() const { return ack_count_; }
  bool blocking() { return blocking_; }

 private:
  void MaybeAcknowledge(std::unique_ptr<Ack> ack) override {
    EXPECT_TRUE(ack);

    ++ack_count_;
    ASSERT_TRUE(base::Contains(request_tokens_to_final_actions_,
                               ack->ack().request_token()));
    ASSERT_EQ(ack->ack().final_action(),
              request_tokens_to_final_actions_.at(ack->ack().request_token()));
  }

  void UploadForDeepScanning(std::unique_ptr<Request> request) override {
    ++requests_count_;
    // A request without tags indicates that it's used for authentication
    if (request->content_analysis_request().tags().empty()) {
      authorization_request_.swap(request);
      if (should_automatically_authorize_) {
        ReturnAuthorizedResponse();
      }
    } else {
      blocking_ = request->blocking();
      Request* request_raw = request.get();
      std::string file = request->filename();
      switch (request->analysis_connector()) {
        case AnalysisConnector::FILE_ATTACHED:
          ASSERT_FALSE(file.empty());
          ASSERT_TRUE(prepared_file_results_.count(file));
          ASSERT_TRUE(prepared_file_responses_.count(file));
          request->FinishRequest(prepared_file_results_[file],
                                 prepared_file_responses_[file]);
          break;
        case AnalysisConnector::BULK_DATA_ENTRY:
          request_raw->GetRequestData(base::BindLambdaForTesting(
              [this, request = std::move(request)](
                  BinaryUploadService::Result result,
                  BinaryUploadService::Request::Data data) {
                if (data.size == prepared_image_data_size_) {
                  request->FinishRequest(prepared_image_result_,
                                         prepared_image_response_);
                } else {
                  request->FinishRequest(prepared_text_result_,
                                         prepared_text_response_);
                }
              }));
          break;
        case AnalysisConnector::PRINT:
          // Since this path is only used for prints that are too large, calling
          // GetRequestData should then call FinishRequest with FILE_TOO_LARGE.
          request_raw->GetRequestData(base::BindOnce(
              [](std::unique_ptr<BinaryUploadService::Request> request,
                 BinaryUploadService::Result result,
                 BinaryUploadService::Request::Data data) {
                ASSERT_EQ(result, BinaryUploadService::Result::FILE_TOO_LARGE);
                request->FinishRequest(result, ContentAnalysisResponse());
              },
              std::move(request)));
          break;
        case AnalysisConnector::ANALYSIS_CONNECTOR_UNSPECIFIED:
        case AnalysisConnector::FILE_DOWNLOADED:
        case AnalysisConnector::FILE_TRANSFER:
          NOTREACHED_IN_MIGRATION();
      }
    }
  }

  BinaryUploadService::Result authorization_result_;
  std::unique_ptr<Request> authorization_request_;

  BinaryUploadService::Result prepared_text_result_;
  ContentAnalysisResponse prepared_text_response_;

  uint64_t prepared_image_data_size_;
  BinaryUploadService::Result prepared_image_result_;
  ContentAnalysisResponse prepared_image_response_;

  std::map<std::string, BinaryUploadService::Result> prepared_file_results_;
  std::map<std::string, ContentAnalysisResponse> prepared_file_responses_;

  int requests_count_ = 0;
  int ack_count_ = 0;
  bool blocking_ = true;
  bool should_automatically_authorize_ = false;
  std::map<std::string, ContentAnalysisAcknowledgement::FinalAction>
      request_tokens_to_final_actions_;
};

FakeBinaryUploadService* FakeBinaryUploadServiceStorage() {
  static FakeBinaryUploadService service;
  return &service;
}

const std::set<std::string>* DocMimeTypes() {
  static std::set<std::string> set = {
      "application/msword", "text/plain",
      // The 50 MB file can result in no mimetype being found.
      ""};
  return &set;
}

const std::set<std::string>* ExeMimeTypes() {
  static std::set<std::string> set = {"application/x-msdownload",
                                      "application/x-ms-dos-executable",
                                      "application/octet-stream"};
  return &set;
}

const std::set<std::string>* ZipMimeTypes() {
  static std::set<std::string> set = {"application/zip",
                                      "application/x-zip-compressed"};
  return &set;
}

const std::set<std::string>* TextMimeTypes() {
  static std::set<std::string> set = {"text/plain"};
  return &set;
}

const std::set<std::string>* ImageMimeTypes() {
  // TODO(b/311679168): Update this set to be non-empty when image paste
  // scanning has better support.
  static std::set<std::string> set = {""};
  return &set;
}

// A fake delegate with minimal overrides to obtain behavior that's as close to
// the real one as possible.
class MinimalFakeContentAnalysisDelegate : public ContentAnalysisDelegate {
 public:
  MinimalFakeContentAnalysisDelegate(
      base::RepeatingClosure quit_closure,
      content::WebContents* web_contents,
      ContentAnalysisDelegate::Data data,
      ContentAnalysisDelegate::CompletionCallback callback)
      : ContentAnalysisDelegate(web_contents,
                                std::move(data),
                                std::move(callback),
                                safe_browsing::DeepScanAccessPoint::UPLOAD),
        quit_closure_(quit_closure) {}

  ~MinimalFakeContentAnalysisDelegate() override { quit_closure_.Run(); }

  static std::unique_ptr<ContentAnalysisDelegate> Create(
      base::RepeatingClosure quit_closure,
      content::WebContents* web_contents,
      ContentAnalysisDelegate::Data data,
      ContentAnalysisDelegate::CompletionCallback callback) {
    return std::make_unique<MinimalFakeContentAnalysisDelegate>(
        quit_closure, web_contents, std::move(data), std::move(callback));
  }

 private:
  BinaryUploadService* GetBinaryUploadService() override {
    return FakeBinaryUploadServiceStorage();
  }

  base::RepeatingClosure quit_closure_;
};

constexpr char kBrowserDMToken[] = "browser_dm_token";
constexpr char kProfileDMToken[] = "profile_dm_token";

constexpr char kTestUrl[] = "https://google.com";

}  // namespace

// Tests the behavior of the dialog delegate with minimal overriding of methods.
// Only responses obtained via the BinaryUploadService are faked.
class ContentAnalysisDelegateBrowserTestBase
    : public test::DeepScanningBrowserTestBase,
      public ContentAnalysisDialog::TestObserver {
 public:
  explicit ContentAnalysisDelegateBrowserTestBase(bool machine_scope)
      : machine_scope_(machine_scope) {
    ContentAnalysisDialog::SetObserverForTesting(this);
  }

  void EnableUploadsScanningAndReporting() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    SetDMTokenForTesting(policy::DMToken::CreateValidToken(kBrowserDMToken));
#else
    if (machine_scope_) {
      SetDMTokenForTesting(policy::DMToken::CreateValidToken(kBrowserDMToken));
    } else {
      test::SetProfileDMToken(browser()->profile(), kProfileDMToken);
    }
#endif

    constexpr char kBlockingScansForDlpAndMalware[] = R"({
      "service_provider": "google",
      "enable": [
        {
          "url_list": ["*"],
          "tags": ["dlp", "malware"]
        }
      ],
      "block_until_verdict": 1
    })";
    enterprise_connectors::test::SetAnalysisConnector(
        browser()->profile()->GetPrefs(), FILE_ATTACHED,
        kBlockingScansForDlpAndMalware, machine_scope_);
    enterprise_connectors::test::SetAnalysisConnector(
        browser()->profile()->GetPrefs(), BULK_DATA_ENTRY,
        kBlockingScansForDlpAndMalware, machine_scope_);
    test::SetOnSecurityEventReporting(browser()->profile()->GetPrefs(),
                                      /*enabled*/ true,
                                      /*enabled_event_names*/ {},
                                      /*enabled_opt_in_events*/ {},
#if BUILDFLAG(IS_CHROMEOS_ASH)
                                      /*machine_scope*/ false);
#else
                                      machine_scope_);
#endif

    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_->SetDMToken(
#if BUILDFLAG(IS_CHROMEOS_ASH)
        kBrowserDMToken);
#else
        machine_scope() ? kBrowserDMToken : kProfileDMToken);
#endif
    if (machine_scope_) {
      RealtimeReportingClientFactory::GetForProfile(browser()->profile())
          ->SetBrowserCloudPolicyClientForTesting(client_.get());
    } else {
      RealtimeReportingClientFactory::GetForProfile(browser()->profile())
#if BUILDFLAG(IS_CHROMEOS_ASH)
          ->SetBrowserCloudPolicyClientForTesting(client_.get());
#else
          ->SetProfileCloudPolicyClientForTesting(client_.get());
#endif
    }
    identity_test_environment_ =
        std::make_unique<signin::IdentityTestEnvironment>();
    identity_test_environment_->MakePrimaryAccountAvailable(
        kUserName, signin::ConsentLevel::kSync);
    RealtimeReportingClientFactory::GetForProfile(browser()->profile())
        ->SetIdentityManagerForTesting(
            identity_test_environment_->identity_manager());
  }

  void DestructorCalled(ContentAnalysisDialog* dialog) override {
    // The test is over once the views are destroyed.
    CallQuitClosure();
  }

  bool machine_scope() const { return machine_scope_; }

  policy::MockCloudPolicyClient* client() { return client_.get(); }

  std::string GetProfileIdentifier() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    return browser()->profile()->GetPath().AsUTF8Unsafe();
#else
    if (machine_scope_) {
      return browser()->profile()->GetPath().AsUTF8Unsafe();
    }
    auto* profile_id_service =
        enterprise::ProfileIdServiceFactory::GetForProfile(
            browser()->profile());
    if (profile_id_service && profile_id_service->GetProfileId().has_value()) {
      return profile_id_service->GetProfileId().value();
    }
    return std::string();
#endif
  }

 private:
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  // This installs a fake SDK manager that creates fake SDK clients when
  // its GetClient() method is called. This is needed so that calls to
  // ContentAnalysisSdkManager::Get()->GetClient() do not fail.
  FakeContentAnalysisSdkManager sdk_manager_;
#endif
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_environment_;
  base::ScopedTempDir temp_dir_;
  bool machine_scope_;
};

class ContentAnalysisDelegateBrowserTest
    : public ContentAnalysisDelegateBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  ContentAnalysisDelegateBrowserTest()
      : ContentAnalysisDelegateBrowserTestBase(GetParam()) {}

  content::ClipboardEndpoint SourceEndpoint() {
    return content::ClipboardEndpoint(
        ui::DataTransferEndpoint(GURL("https://source.com")),
        base::BindLambdaForTesting([this]() {
          return static_cast<content::BrowserContext*>(browser()->profile());
        }),
        *browser()
             ->tab_strip_model()
             ->GetActiveWebContents()
             ->GetPrimaryMainFrame());
  }

  content::ClipboardEndpoint IncognitoSourceEndpoint() {
    return content::ClipboardEndpoint(
        ui::DataTransferEndpoint(GURL("https://source.com")),
        base::BindLambdaForTesting([this]() {
          return static_cast<content::BrowserContext*>(
              browser()->profile()->GetPrimaryOTRProfile(
                  /*create_if_needed*/ true));
        }),
        *browser()
             ->tab_strip_model()
             ->GetActiveWebContents()
             ->GetPrimaryMainFrame());
  }
};

INSTANTIATE_TEST_SUITE_P(, ContentAnalysisDelegateBrowserTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBrowserTest, Unauthorized) {
  // The reading of the browser DM token is blocking and happens in this test
  // when checking if the browser is enrolled.
  base::ScopedAllowBlockingForTesting allow_blocking;

  EnableUploadsScanningAndReporting();

  base::RunLoop content_analysis_run_loop;
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create,
                          content_analysis_run_loop.QuitClosure()));

  FakeBinaryUploadServiceStorage()->SetAuthorized(false);
  // This causes the DM Token to be rejected, and unauthorized for 24 hours.
  client()->SetStatus(policy::DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED);
  client()->NotifyClientError();

  bool called = false;
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  ContentAnalysisDelegate::Data data;
  data.text.emplace_back(text());
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.doc"));
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, FILE_ATTACHED));

  // Nothing should be reported for unauthorized users.
  test::EventReportValidator validator(client());
  validator.ExpectNoReport();

  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&quit_closure, &called](const ContentAnalysisDelegate::Data& data,
                                   ContentAnalysisDelegate::Result& result) {
            ASSERT_EQ(result.text_results.size(), 1u);
            ASSERT_EQ(result.paths_results.size(), 1u);
            ASSERT_TRUE(result.text_results[0]);
            ASSERT_TRUE(result.paths_results[0]);
            called = true;
            quit_closure.Run();
          }),
      safe_browsing::DeepScanAccessPoint::UPLOAD);

  FakeBinaryUploadServiceStorage()->ReturnAuthorizedResponse();

  run_loop.Run();
  EXPECT_TRUE(called);

  // 1 request to authenticate for upload.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 1);
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->ack_count(), 0);

  // Ensure the ContentAnalysisDelegate is destroyed before the end of the test.
  content_analysis_run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBrowserTest, Files) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();

  base::RunLoop content_analysis_run_loop;
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create,
                          content_analysis_run_loop.QuitClosure()));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  // Create the files to be opened and scanned.
  ContentAnalysisDelegate::Data data;
  CreateFilesForTest({"ok.doc", "bad.exe"},
                     {"ok file content", "bad file content"}, &data);
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, FILE_ATTACHED));

  // The malware verdict means an event should be reported.
  test::EventReportValidator validator(client());
  validator.ExpectDangerousDeepScanningResult(
      /*url*/ "about:blank",
      /*tab_url*/ "about:blank",
      /*source*/ "",
      /*destination*/ "",
      /*filename*/
      machine_scope() ? created_file_paths()[1].AsUTF8Unsafe() : "bad.exe",
      // printf "bad file content" | sha256sum |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "77AE96C38386429D28E53F5005C46C7B4D8D39BE73D757CE61E0AE65CC1A5A5D",
      /*threat_type*/ "DANGEROUS",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      /*mimetypes*/ ExeMimeTypes(),
      /*size*/ std::string("bad file content").size(),
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*scan_id*/ kScanId2);

  ContentAnalysisResponse ok_response;
  ok_response.set_request_token(kScanId1);
  auto* ok_result = ok_response.add_results();
  ok_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  ok_result->set_tag("malware");

  ContentAnalysisResponse bad_response;
  bad_response.set_request_token(kScanId2);
  auto* bad_result = bad_response.add_results();
  bad_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  bad_result->set_tag("malware");
  auto* bad_rule = bad_result->add_triggered_rules();
  bad_rule->set_action(TriggeredRule::BLOCK);
  bad_rule->set_rule_name("malware");

  FakeBinaryUploadServiceStorage()->SetResponseForFile(
      created_file_paths()[0].AsUTF8Unsafe(),
      BinaryUploadService::Result::SUCCESS, ok_response);
  FakeBinaryUploadServiceStorage()->SetExpectedFinalAction(
      kScanId1, ContentAnalysisAcknowledgement::ALLOW);
  FakeBinaryUploadServiceStorage()->SetResponseForFile(
      created_file_paths()[1].AsUTF8Unsafe(),
      BinaryUploadService::Result::SUCCESS, bad_response);
  FakeBinaryUploadServiceStorage()->SetExpectedFinalAction(
      kScanId2, ContentAnalysisAcknowledgement::BLOCK);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&called](const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.paths_results.size(), 2u);
            ASSERT_TRUE(result.paths_results[0]);
            ASSERT_FALSE(result.paths_results[1]);
            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::UPLOAD);

  run_loop.Run();

  EXPECT_TRUE(called);

  // There should have been 1 request per file (2 files) and 1 for
  // authentication.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 3);
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->ack_count(), 2);

  // Ensure the ContentAnalysisDelegate is destroyed before the end of the test.
  content_analysis_run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBrowserTest, ForFiles) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();

  base::RunLoop content_analysis_run_loop;
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create,
                          content_analysis_run_loop.QuitClosure()));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  // Create the files to be opened and scanned.
  ContentAnalysisDelegate::Data data;
  CreateFilesForTest({"ok.doc", "bad.exe"},
                     {"ok file content", "bad file content"}, &data);
  // Create a subdirectory with a file to be expanded.
  CreateFilesForTest({"fine.exe"}, {"fine file content"}, &data, "sub");

  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, FILE_ATTACHED));

  // The malware verdict means an event should be reported.
  test::EventReportValidator validator(client());
  validator.ExpectDangerousDeepScanningResult(
      /*url*/ "about:blank",
      /*tab_url*/ "about:blank",
      /*source*/ "",
      /*destination*/ "",
      /*filename*/
      machine_scope() ? created_file_paths()[1].AsUTF8Unsafe() : "bad.exe",
      // printf "bad file content" | sha256sum |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "77AE96C38386429D28E53F5005C46C7B4D8D39BE73D757CE61E0AE65CC1A5A5D",
      /*threat_type*/ "DANGEROUS",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      /*mimetypes*/ ExeMimeTypes(),
      /*size*/ std::string("bad file content").size(),
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*scan_id*/ kScanId2);

  {
    ContentAnalysisResponse ok_response;
    ok_response.set_request_token(kScanId1);
    auto* ok_result = ok_response.add_results();
    ok_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
    ok_result->set_tag("malware");

    FakeBinaryUploadServiceStorage()->SetResponseForFile(
        created_file_paths()[0].AsUTF8Unsafe(),
        BinaryUploadService::Result::SUCCESS, ok_response);
    FakeBinaryUploadServiceStorage()->SetExpectedFinalAction(
        kScanId1, ContentAnalysisAcknowledgement::ALLOW);
  }

  {
    ContentAnalysisResponse bad_response;
    bad_response.set_request_token(kScanId2);
    auto* bad_result = bad_response.add_results();
    bad_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
    bad_result->set_tag("malware");
    auto* bad_rule = bad_result->add_triggered_rules();
    bad_rule->set_action(TriggeredRule::BLOCK);
    bad_rule->set_rule_name("malware");

    FakeBinaryUploadServiceStorage()->SetResponseForFile(
        created_file_paths()[1].AsUTF8Unsafe(),
        BinaryUploadService::Result::SUCCESS, bad_response);
    FakeBinaryUploadServiceStorage()->SetExpectedFinalAction(
        kScanId2, ContentAnalysisAcknowledgement::BLOCK);
  }

  {
    ContentAnalysisResponse ok_response;
    ok_response.set_request_token(kScanId3);
    auto* ok_result = ok_response.add_results();
    ok_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
    ok_result->set_tag("malware");

    FakeBinaryUploadServiceStorage()->SetResponseForFile(
        created_file_paths()[2].AsUTF8Unsafe(),
        BinaryUploadService::Result::SUCCESS, ok_response);
    FakeBinaryUploadServiceStorage()->SetExpectedFinalAction(
        kScanId3, ContentAnalysisAcknowledgement::ALLOW);
  }

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  // Start test.
  ContentAnalysisDelegate::CreateForFilesInWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting([&called](std::vector<base::FilePath> paths,
                                           std::vector<bool> result) {
        ASSERT_EQ(paths.size(), 3u);
        ASSERT_EQ(paths[0].BaseName(),
                  base::FilePath(FILE_PATH_LITERAL("ok.doc")));
        ASSERT_EQ(paths[1].BaseName(),
                  base::FilePath(FILE_PATH_LITERAL("bad.exe")));
        ASSERT_EQ(paths[2].BaseName(),
                  base::FilePath(FILE_PATH_LITERAL("sub")));

        ASSERT_EQ(result.size(), 3u);
        ASSERT_TRUE(result[0]);
        ASSERT_FALSE(result[1]);
        ASSERT_TRUE(result[2]);

        called = true;
      }),
      safe_browsing::DeepScanAccessPoint::UPLOAD);

  run_loop.Run();
  EXPECT_TRUE(called);

  // There should have been 1 request per file (3 files) and 1 for
  // authentication.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 4);
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->ack_count(), 3);

  // Ensure the ContentAnalysisDelegate is destroyed before the end of the test.
  content_analysis_run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBrowserTest, Texts) {
  // The reading of the browser DM token is blocking and happens in this test
  // when checking if the browser is enrolled.
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();

  base::RunLoop content_analysis_run_loop;
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create,
                          content_analysis_run_loop.QuitClosure()));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);

  test::EventReportValidator validator(client());
  // Prepare a complex DLP response to test that the verdict is reported
  // correctly in the sensitive data event.
  ContentAnalysisResponse response;
  response.set_request_token(kScanId1);
  auto* result = response.add_results();
  result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  result->set_tag("dlp");

  auto* rule1 = result->add_triggered_rules();
  rule1->set_action(TriggeredRule::REPORT_ONLY);
  rule1->set_rule_id("1");
  rule1->set_rule_name("resource rule 1");

  auto* rule2 = result->add_triggered_rules();
  rule2->set_action(TriggeredRule::BLOCK);
  rule2->set_rule_id("3");
  rule2->set_rule_name("resource rule 2");

  FakeBinaryUploadServiceStorage()->SetResponseForText(
      BinaryUploadService::Result::SUCCESS, response);
  FakeBinaryUploadServiceStorage()->SetExpectedFinalAction(
      kScanId1, ContentAnalysisAcknowledgement::BLOCK);

  // The DLP verdict means an event should be reported. The content size is
  // equal to the length of the concatenated texts (2 * 100 * 'a').
  validator.ExpectSensitiveDataEvent(
      /*url*/ "about:blank",
      /*tab_url*/ "about:blank",
      /*source*/ "https://source.com/",
      /*destination*/ "about:blank",
      /*filename*/ "Text data",
      // The hash should not be included for string requests.
      /*sha*/ "",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      /*dlp_verdict*/ *result,
      /*mimetype*/ TextMimeTypes(),
      /*size*/ 200,
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*scan_id*/ kScanId1,
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  ContentAnalysisDelegate::Data data;
  data.text.emplace_back(text());
  data.text.emplace_back(text());
  data.reason = ContentAnalysisRequest::CLIPBOARD_PASTE;
  data.clipboard_source = "https://source.com/";
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, BULK_DATA_ENTRY));

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&called](const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.paths_results.empty());
            ASSERT_EQ(result.text_results.size(), 2u);
            ASSERT_FALSE(result.text_results[0]);
            ASSERT_FALSE(result.text_results[1]);
            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::PASTE);

  FakeBinaryUploadServiceStorage()->ReturnAuthorizedResponse();

  run_loop.Run();
  EXPECT_TRUE(called);

  // There should have been 1 request for all texts,
  // 1 for authentication of the scanning request.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 2);
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->ack_count(), 1);

  // Ensure the ContentAnalysisDelegate is destroyed before the end of the test.
  content_analysis_run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBrowserTest,
                       Texts_IncognitoSource) {
  // The reading of the browser DM token is blocking and happens in this test
  // when checking if the browser is enrolled.
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();

  base::RunLoop content_analysis_run_loop;
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create,
                          content_analysis_run_loop.QuitClosure()));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);

  test::EventReportValidator validator(client());
  // Prepare a complex DLP response to test that the verdict is reported
  // correctly in the sensitive data event.
  ContentAnalysisResponse response;
  response.set_request_token(kScanId1);
  auto* result = response.add_results();
  result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  result->set_tag("dlp");

  auto* rule1 = result->add_triggered_rules();
  rule1->set_action(TriggeredRule::REPORT_ONLY);
  rule1->set_rule_id("1");
  rule1->set_rule_name("resource rule 1");

  auto* rule2 = result->add_triggered_rules();
  rule2->set_action(TriggeredRule::BLOCK);
  rule2->set_rule_id("3");
  rule2->set_rule_name("resource rule 2");

  FakeBinaryUploadServiceStorage()->SetResponseForText(
      BinaryUploadService::Result::SUCCESS, response);
  FakeBinaryUploadServiceStorage()->SetExpectedFinalAction(
      kScanId1, ContentAnalysisAcknowledgement::BLOCK);

  // The DLP verdict means an event should be reported. The content size is
  // equal to the length of the concatenated texts (2 * 100 * 'a').
  validator.ExpectSensitiveDataEvent(
      /*url*/ "about:blank",
      /*tab_url*/ "about:blank",
      /*source*/ "",  // The source is omitted intentionally when it's
                      // incognito.
      /*destination*/ "about:blank",
      /*filename*/ "Text data",
      // The hash should not be included for string requests.
      /*sha*/ "",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      /*dlp_verdict*/ *result,
      /*mimetype*/ TextMimeTypes(),
      /*size*/ 200,
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*scan_id*/ kScanId1,
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  ContentAnalysisDelegate::Data data;
  data.text.emplace_back(text());
  data.text.emplace_back(text());
  data.reason = ContentAnalysisRequest::CLIPBOARD_PASTE;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, BULK_DATA_ENTRY));

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&called](const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.paths_results.empty());
            ASSERT_EQ(result.text_results.size(), 2u);
            ASSERT_FALSE(result.text_results[0]);
            ASSERT_FALSE(result.text_results[1]);
            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::PASTE);

  FakeBinaryUploadServiceStorage()->ReturnAuthorizedResponse();

  run_loop.Run();
  EXPECT_TRUE(called);

  // There should have been 1 request for all texts,
  // 1 for authentication of the scanning request.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 2);
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->ack_count(), 1);

  // Ensure the ContentAnalysisDelegate is destroyed before the end of the test.
  content_analysis_run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBrowserTest, AllowTextAndImage) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();

  base::RunLoop content_analysis_run_loop;
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create,
                          content_analysis_run_loop.QuitClosure()));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);

  ContentAnalysisResponse text_response;
  text_response.set_request_token(kScanId1);
  auto* text_result = text_response.add_results();
  text_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  text_result->set_tag("dlp");

  FakeBinaryUploadServiceStorage()->SetResponseForText(
      BinaryUploadService::Result::SUCCESS, text_response);
  FakeBinaryUploadServiceStorage()->SetExpectedFinalAction(
      kScanId1, ContentAnalysisAcknowledgement::ALLOW);

  ContentAnalysisResponse image_response;
  image_response.set_request_token(kScanId2);
  auto* image_result = image_response.add_results();
  image_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  image_result->set_tag("dlp");

  FakeBinaryUploadServiceStorage()->SetResponseForImage(
      BinaryUploadService::Result::SUCCESS, image_response, image().size());
  FakeBinaryUploadServiceStorage()->SetExpectedFinalAction(
      kScanId2, ContentAnalysisAcknowledgement::ALLOW);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  ContentAnalysisDelegate::Data data;
  data.image = image();
  data.text.emplace_back(text());
  data.reason = ContentAnalysisRequest::CLIPBOARD_PASTE;
  data.clipboard_source = "https://source.com/";
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, BULK_DATA_ENTRY));

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&called](const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.paths_results.empty());
            ASSERT_EQ(result.text_results.size(), 1u);
            ASSERT_TRUE(result.text_results[0]);
            ASSERT_TRUE(result.image_result);
            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::PASTE);

  FakeBinaryUploadServiceStorage()->ReturnAuthorizedResponse();

  run_loop.Run();
  EXPECT_TRUE(called);

  // There should have been 1 request for text, 1 request for image,
  // 1 for authentication of the scanning request.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 3);
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->ack_count(), 2);

  // Ensure the ContentAnalysisDelegate is destroyed before the end of the test.
  content_analysis_run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBrowserTest,
                       BlockTextAndAllowImage) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();

  base::RunLoop content_analysis_run_loop;
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create,
                          content_analysis_run_loop.QuitClosure()));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);

  ContentAnalysisResponse text_response;
  text_response.set_request_token(kScanId1);
  auto* text_result = text_response.add_results();
  text_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  text_result->set_tag("dlp");

  // Block text.
  auto* rule = text_result->add_triggered_rules();
  rule->set_action(TriggeredRule::BLOCK);
  rule->set_rule_id("1");
  rule->set_rule_name("resource rule 1");
  FakeBinaryUploadServiceStorage()->SetResponseForText(
      BinaryUploadService::Result::SUCCESS, text_response);
  FakeBinaryUploadServiceStorage()->SetExpectedFinalAction(
      kScanId1, ContentAnalysisAcknowledgement::BLOCK);

  ContentAnalysisResponse image_response;
  image_response.set_request_token(kScanId2);
  auto* image_result = image_response.add_results();
  image_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  image_result->set_tag("dlp");

  FakeBinaryUploadServiceStorage()->SetResponseForImage(
      BinaryUploadService::Result::SUCCESS, image_response, image().size());
  // Final action for image ack should be blocked, even though we are only
  // blocking text.
  FakeBinaryUploadServiceStorage()->SetExpectedFinalAction(
      kScanId2, ContentAnalysisAcknowledgement::BLOCK);

  test::EventReportValidator validator(client());
  validator.ExpectSensitiveDataEvent(
      /*url*/ "about:blank",
      /*tab_url*/ "about:blank",
      /*source*/ "https://source.com/",
      /*destination*/ "about:blank",
      /*filename*/ "Text data",
      // The hash should not be included for string requests.
      /*sha*/ "",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      /*dlp_verdict*/ *text_result,
      /*mimetype*/ TextMimeTypes(),
      /*size*/ 100,
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*scan_id*/ kScanId1,
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  ContentAnalysisDelegate::Data data;
  data.image = image();
  data.text.emplace_back(text());
  data.reason = ContentAnalysisRequest::CLIPBOARD_PASTE;
  data.clipboard_source = "https://source.com/";
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, BULK_DATA_ENTRY));

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&called](const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.paths_results.empty());
            ASSERT_EQ(result.text_results.size(), 1u);
            // Delegate does not handle result syncing across different types of
            // requests, so image_result should be true.
            ASSERT_TRUE(result.image_result);
            ASSERT_FALSE(result.text_results[0]);
            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::PASTE);

  FakeBinaryUploadServiceStorage()->ReturnAuthorizedResponse();

  run_loop.Run();
  EXPECT_TRUE(called);

  // There should have been 1 request for text, 1 request for image,
  // 1 for authentication of the scanning request.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 3);
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->ack_count(), 2);

  // Ensure the ContentAnalysisDelegate is destroyed before the end of the test.
  content_analysis_run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBrowserTest,
                       BlockTextAndAllowImage_IncognitoSource) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();

  base::RunLoop content_analysis_run_loop;
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create,
                          content_analysis_run_loop.QuitClosure()));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);

  ContentAnalysisResponse text_response;
  text_response.set_request_token(kScanId1);
  auto* text_result = text_response.add_results();
  text_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  text_result->set_tag("dlp");

  // Block text.
  auto* rule = text_result->add_triggered_rules();
  rule->set_action(TriggeredRule::BLOCK);
  rule->set_rule_id("1");
  rule->set_rule_name("resource rule 1");
  FakeBinaryUploadServiceStorage()->SetResponseForText(
      BinaryUploadService::Result::SUCCESS, text_response);
  FakeBinaryUploadServiceStorage()->SetExpectedFinalAction(
      kScanId1, ContentAnalysisAcknowledgement::BLOCK);

  ContentAnalysisResponse image_response;
  image_response.set_request_token(kScanId2);
  auto* image_result = image_response.add_results();
  image_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  image_result->set_tag("dlp");

  FakeBinaryUploadServiceStorage()->SetResponseForImage(
      BinaryUploadService::Result::SUCCESS, image_response, image().size());
  // Final action for image ack should be blocked, even though we are only
  // blocking text.
  FakeBinaryUploadServiceStorage()->SetExpectedFinalAction(
      kScanId2, ContentAnalysisAcknowledgement::BLOCK);

  test::EventReportValidator validator(client());
  validator.ExpectSensitiveDataEvent(
      /*url*/ "about:blank",
      /*tab_url*/ "about:blank",
      /*source*/ "",  // The source is omitted intentionally when it's
                      // incognito.
      /*destination*/ "about:blank",
      /*filename*/ "Text data",
      // The hash should not be included for string requests.
      /*sha*/ "",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      /*dlp_verdict*/ *text_result,
      /*mimetype*/ TextMimeTypes(),
      /*size*/ 100,
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*scan_id*/ kScanId1,
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  ContentAnalysisDelegate::Data data;
  data.image = image();
  data.text.emplace_back(text());
  data.reason = ContentAnalysisRequest::CLIPBOARD_PASTE;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, BULK_DATA_ENTRY));

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&called](const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.paths_results.empty());
            ASSERT_EQ(result.text_results.size(), 1u);
            // Delegate does not handle result syncing across different types of
            // requests, so image_result should be true.
            ASSERT_TRUE(result.image_result);
            ASSERT_FALSE(result.text_results[0]);
            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::PASTE);

  FakeBinaryUploadServiceStorage()->ReturnAuthorizedResponse();

  run_loop.Run();
  EXPECT_TRUE(called);

  // There should have been 1 request for text, 1 request for image,
  // 1 for authentication of the scanning request.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 3);
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->ack_count(), 2);

  // Ensure the ContentAnalysisDelegate is destroyed before the end of the test.
  content_analysis_run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBrowserTest,
                       AllowTextAndBlockImage) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();

  base::RunLoop content_analysis_run_loop;
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create,
                          content_analysis_run_loop.QuitClosure()));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);

  ContentAnalysisResponse text_response;
  text_response.set_request_token(kScanId1);
  auto* text_result = text_response.add_results();
  text_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  text_result->set_tag("dlp");

  FakeBinaryUploadServiceStorage()->SetResponseForText(
      BinaryUploadService::Result::SUCCESS, text_response);
  // Final action for text ack should be blocked, even though we are only
  // blocking image.
  FakeBinaryUploadServiceStorage()->SetExpectedFinalAction(
      kScanId1, ContentAnalysisAcknowledgement::BLOCK);

  ContentAnalysisResponse image_response;
  image_response.set_request_token(kScanId2);
  auto* image_result = image_response.add_results();
  image_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  image_result->set_tag("dlp");

  // Block image.
  auto* rule = image_result->add_triggered_rules();
  rule->set_action(TriggeredRule::BLOCK);
  rule->set_rule_id("1");
  rule->set_rule_name("resource rule 1");
  FakeBinaryUploadServiceStorage()->SetResponseForImage(
      BinaryUploadService::Result::SUCCESS, image_response, image().size());
  FakeBinaryUploadServiceStorage()->SetExpectedFinalAction(
      kScanId2, ContentAnalysisAcknowledgement::BLOCK);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  test::EventReportValidator validator(client());
  validator.ExpectSensitiveDataEvent(
      /*url*/ "about:blank",
      /*tab_url*/ "about:blank",
      /*source*/ "https://source.com/",
      /*destination*/ "about:blank",
      /*filename*/ "Image data",
      // The hash should not be included for image requests.
      /*sha*/ "",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      /*dlp_verdict*/ *image_result,
      /*mimetype*/ ImageMimeTypes(),
      /*size*/ 50,
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*scan_id*/ kScanId2,
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  ContentAnalysisDelegate::Data data;
  data.image = image();
  data.text.emplace_back(text());
  data.clipboard_source = "https://source.com/";
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, BULK_DATA_ENTRY));

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&called](const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.paths_results.empty());
            ASSERT_EQ(result.text_results.size(), 1u);
            ASSERT_FALSE(result.image_result);
            // Delegate does not handle result syncing across different types of
            // requests, so text_result should be true.
            ASSERT_TRUE(result.text_results[0]);
            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::PASTE);

  FakeBinaryUploadServiceStorage()->ReturnAuthorizedResponse();

  run_loop.Run();
  EXPECT_TRUE(called);

  // There should have been 1 request for text, 1 request for image,
  // 1 for authentication of the scanning request.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 3);
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->ack_count(), 2);

  // Ensure the ContentAnalysisDelegate is destroyed before the end of the test.
  content_analysis_run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBrowserTest,
                       AllowTextAndBlockImage_IncognitoSource) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();

  base::RunLoop content_analysis_run_loop;
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create,
                          content_analysis_run_loop.QuitClosure()));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);

  ContentAnalysisResponse text_response;
  text_response.set_request_token(kScanId1);
  auto* text_result = text_response.add_results();
  text_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  text_result->set_tag("dlp");

  FakeBinaryUploadServiceStorage()->SetResponseForText(
      BinaryUploadService::Result::SUCCESS, text_response);
  // Final action for text ack should be blocked, even though we are only
  // blocking image.
  FakeBinaryUploadServiceStorage()->SetExpectedFinalAction(
      kScanId1, ContentAnalysisAcknowledgement::BLOCK);

  ContentAnalysisResponse image_response;
  image_response.set_request_token(kScanId2);
  auto* image_result = image_response.add_results();
  image_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  image_result->set_tag("dlp");

  // Block image.
  auto* rule = image_result->add_triggered_rules();
  rule->set_action(TriggeredRule::BLOCK);
  rule->set_rule_id("1");
  rule->set_rule_name("resource rule 1");
  FakeBinaryUploadServiceStorage()->SetResponseForImage(
      BinaryUploadService::Result::SUCCESS, image_response, image().size());
  FakeBinaryUploadServiceStorage()->SetExpectedFinalAction(
      kScanId2, ContentAnalysisAcknowledgement::BLOCK);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  test::EventReportValidator validator(client());
  validator.ExpectSensitiveDataEvent(
      /*url*/ "about:blank",
      /*tab_url*/ "about:blank",
      /*source*/ "",  // The source is omitted intentionally when it's
                      // incognito.
      /*destination*/ "about:blank",
      /*filename*/ "Image data",
      // The hash should not be included for image requests.
      /*sha*/ "",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      /*dlp_verdict*/ *image_result,
      /*mimetype*/ ImageMimeTypes(),
      /*size*/ 50,
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*scan_id*/ kScanId2,
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  ContentAnalysisDelegate::Data data;
  data.image = image();
  data.text.emplace_back(text());
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, BULK_DATA_ENTRY));
  EXPECT_TRUE(data.settings.block_until_verdict != BlockUntilVerdict::kNoBlock);

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&called](const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.paths_results.empty());
            ASSERT_EQ(result.text_results.size(), 1u);
            ASSERT_FALSE(result.image_result);
            // Delegate does not handle result syncing across different types of
            // requests, so text_result should be true.
            ASSERT_TRUE(result.text_results[0]);
            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::PASTE);

  FakeBinaryUploadServiceStorage()->ReturnAuthorizedResponse();

  run_loop.Run();
  EXPECT_TRUE(called);

  // There should have been 1 request for text, 1 request for image,
  // 1 for authentication of the scanning request.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 3);
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->ack_count(), 2);
  EXPECT_TRUE(FakeBinaryUploadServiceStorage()->blocking());

  // Ensure the ContentAnalysisDelegate is destroyed before the end of the test.
  content_analysis_run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBrowserTest, Throttled) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();

  base::RunLoop content_analysis_run_loop;
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create,
                          content_analysis_run_loop.QuitClosure()));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  // Create the files to be opened and scanned.
  ContentAnalysisDelegate::Data data;
  data.reason = ContentAnalysisRequest::FILE_PICKER_DIALOG;
  CreateFilesForTest({"a.exe", "b.exe", "c.exe"},
                     {"a content", "b content", "c content"}, &data);
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, FILE_ATTACHED));

  // The malware verdict means an event should be reported.
  test::EventReportValidator validator(client());
  validator.ExpectUnscannedFileEvents(
      /*url*/ "about:blank",
      /*tab_url*/ "about:blank",
      /*source*/ "",
      /*destination*/ "",
      {
          machine_scope() ? created_file_paths()[0].AsUTF8Unsafe() : "a.exe",
          machine_scope() ? created_file_paths()[1].AsUTF8Unsafe() : "b.exe",
          machine_scope() ? created_file_paths()[2].AsUTF8Unsafe() : "c.exe",
      },
      {
          // printf "a content" | sha256sum | tr '[:lower:]' '[:upper:]'
          "D2D2ACF640179223BF9E1EB43C5FBF854C4E50FFB6733BC3A9279D3FF7DE9BE1",
          // printf "b content" | sha256sum | tr '[:lower:]' '[:upper:]'
          "93CB3641ADD6A9A6619D7E2F304EBCF5160B2DB016B27C6E3D641C5306897224",
          // printf "c content" | sha256sum | tr '[:lower:]' '[:upper:]'
          "2E6D1C4A1F39A02562BF1505AD775C0323D7A04C0C37C9B29D25F532B9972080",
      },
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      /*reason*/ "TOO_MANY_REQUESTS",
      /*mimetypes*/ ExeMimeTypes(),
      /*size*/ 9,
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::ALLOWED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*content_transfer_reason*/ "CONTENT_TRANSFER_METHOD_FILE_PICKER");

  // While only one file should reach the upload part and get a
  // TOO_MANY_REQUEST result, it can be any of them depending on how quickly
  // they are opened asynchronously. This means responses must be set up for
  // each of them.
  for (size_t i = 0; i < 3; ++i) {
    FakeBinaryUploadServiceStorage()->SetResponseForFile(
        created_file_paths()[i].AsUTF8Unsafe(),
        BinaryUploadService::Result::TOO_MANY_REQUESTS,
        ContentAnalysisResponse());
  }

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&called](const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.paths_results.size(), 3u);
            for (bool paths_result : result.paths_results) {
              ASSERT_TRUE(paths_result);
            }
            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::UPLOAD);

  run_loop.Run();

  EXPECT_TRUE(called);

  // There should have been 1 request for the first file and 1 for
  // authentication.  There were no successful requests so no acks.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 2);
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->ack_count(), 0);

  // Ensure the ContentAnalysisDelegate is destroyed before the end of the test.
  content_analysis_run_loop.Run();
}

// This class tests each of the blocking settings used in Connector policies:
// - block_until_verdict
// - block_password_protected
// - block_large_files
class ContentAnalysisDelegateBlockingSettingBrowserTest
    : public ContentAnalysisDelegateBrowserTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  ContentAnalysisDelegateBlockingSettingBrowserTest()
      : ContentAnalysisDelegateBrowserTestBase(machine_scope()) {
    if (is_resumable()) {
      scoped_feature_list_.InitAndEnableFeature(kResumableUploadEnabled);
    } else {
      scoped_feature_list_.InitAndDisableFeature(kResumableUploadEnabled);
    }
  }

  bool machine_scope() const { return std::get<0>(GetParam()); }

  bool setting_param() const { return std::get<1>(GetParam()); }

  bool is_resumable() const { return std::get<2>(GetParam()); }

  // Use a string since the setting value is inserted into a JSON policy.
  const char* bool_setting_value() const {
    return setting_param() ? "true" : "false";
  }
  const char* int_setting_value() const { return setting_param() ? "1" : "0"; }

  bool expected_result() const { return !setting_param(); }
};

INSTANTIATE_TEST_SUITE_P(,
                         ContentAnalysisDelegateBlockingSettingBrowserTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBlockingSettingBrowserTest,
                       BlockPasswordProtected) {
  // When the resumable protocol is in use and the `blocked_password_protected`
  // setting is off, the final verdict is determined by the server, not by the
  // policy value. So this specific scenario only applies to multi-part upload.
  //
  // TODO(b/341264970): Add test support when setting_param is on.
  if (is_resumable() && !setting_param()) {
    return;
  }

  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII("safe_browsing")
                 .AppendASCII("download_protection")
                 .AppendASCII("encrypted.zip");

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();
  constexpr char kPasswordProtectedPref[] = R"({
    "service_provider": "google",
    "enable": [
      {
        "url_list": ["*"],
        "tags": ["dlp"]
      }
    ],
    "block_until_verdict": 1,
    "block_password_protected": %s
  })";
  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), FILE_ATTACHED,
      base::StringPrintf(kPasswordProtectedPref, bool_setting_value()),
      machine_scope());

  base::RunLoop content_analysis_run_loop;
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create,
                          content_analysis_run_loop.QuitClosure()));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  ContentAnalysisDelegate::Data data;
  data.paths.emplace_back(test_zip);
  data.reason = ContentAnalysisRequest::DRAG_AND_DROP;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, FILE_ATTACHED));

  // The file should be reported as unscanned.
  test::EventReportValidator validator(client());
  validator.ExpectUnscannedFileEvent(
      /*url*/ "about:blank",
      /*tab_url*/ "about:blank",
      /*source*/ "",
      /*destination*/ "",
      /*filename*/ machine_scope() ? test_zip.AsUTF8Unsafe() : "encrypted.zip",
      // sha256sum < chrome/test/data/safe_browsing/download_protection/\
      // encrypted.zip |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "701FCEA8B2112FFAB257A8A8DFD3382ABCF047689AB028D42903E3B3AA488D9A",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      /*reason*/ "FILE_PASSWORD_PROTECTED",
      /*mimetypes*/ ZipMimeTypes(),
      // du chrome/test/data/safe_browsing/download_protection/encrypted.zip -b
      /*size*/ 20015,
      /*result*/
      expected_result() ? safe_browsing::EventResultToString(
                              safe_browsing::EventResult::ALLOWED)
                        : safe_browsing::EventResultToString(
                              safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*content_transfer_reason*/ "CONTENT_TRANSFER_METHOD_DRAG_AND_DROP");

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const ContentAnalysisDelegate::Data& data,
                          ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.paths_results.size(), 1u);
            ASSERT_EQ(result.paths_results[0], expected_result());
            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::DRAG_AND_DROP);

  run_loop.Run();
  EXPECT_TRUE(called);
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 0);
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->ack_count(), 0);

  // Ensure the ContentAnalysisDelegate is destroyed before the end of the test.
  content_analysis_run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBlockingSettingBrowserTest,
                       BlockLargeFiles) {
  // When the resumable protocol is in use and the `blocked_large_files` setting
  // is off, the final verdict is determined by the server, not by the policy
  // value. So this specific testcase only applies to multi-part upload.
  //
  // TODO(b/341264970): Add test support when setting_param is on.
  if (is_resumable() && !setting_param()) {
    return;
  }

  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();
  constexpr char kBlockLargeFilesPref[] = R"({
    "service_provider": "google",
    "enable": [
      {
        "url_list": ["*"],
        "tags": ["dlp", "malware"]
      }
    ],
    "block_until_verdict": 1,
    "block_large_files": %s
  })";
  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), FILE_ATTACHED,
      base::StringPrintf(kBlockLargeFilesPref, bool_setting_value()),
      machine_scope());

  base::RunLoop content_analysis_run_loop;
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create,
                          content_analysis_run_loop.QuitClosure()));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  // Create the large file.
  ContentAnalysisDelegate::Data data;
  data.reason = ContentAnalysisRequest::FILE_PICKER_DIALOG;

  CreateFilesForTest({"large.doc"}, {std::string()}, &data);

  constexpr int64_t kLargeSize = 51 * 1024 * 1024;
  std::string chunk = std::string(kLargeSize, 'a');
  base::File file(created_file_paths()[0],
                  base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  file.WriteAtCurrentPos(base::as_byte_span(chunk));

  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, FILE_ATTACHED));

  // The file should be reported as unscanned.
  test::EventReportValidator validator(client());
  validator.ExpectUnscannedFileEvent(
      /*url*/ "about:blank",
      /*tab_url*/ "about:blank",
      /*source*/ "",
      /*destination*/ "",
      /*filename*/
      machine_scope() ? created_file_paths()[0].AsUTF8Unsafe() : "large.doc",
      // python3 -c "print('a' * (51 * 1024 * 1024), end='')" |\
      // sha256sum |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "6F040FFDD67004CA3074BFB39936F553A49669427C477CC60DBE064C355EE1B1",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      /*reason*/ "FILE_TOO_LARGE",
      /*mimetypes*/ DocMimeTypes(),
      /*size*/ kLargeSize,
      /*result*/
      expected_result() ? safe_browsing::EventResultToString(
                              safe_browsing::EventResult::ALLOWED)
                        : safe_browsing::EventResultToString(
                              safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*content_transfer_method*/ "CONTENT_TRANSFER_METHOD_FILE_PICKER");

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const ContentAnalysisDelegate::Data& data,
                          ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.paths_results.size(), 1u);
            ASSERT_EQ(result.paths_results[0], expected_result());

            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::UPLOAD);

  run_loop.Run();
  EXPECT_TRUE(called);

  // Ensure the ContentAnalysisDelegate is destroyed before the end of the test.
  content_analysis_run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBlockingSettingBrowserTest,
                       BlockLargePages) {
  // When the resumable protocol is in use and the `blocked_large_files` setting
  // is off, the final verdict is determined by the server, not by the policy
  // value. So this specific testcase only applies to multi-part upload.
  //
  // TODO(b/341264970): Add test support when setting_param is on.
  if (is_resumable() && !setting_param()) {
    return;
  }

  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();
  constexpr char kBlockLargePagesPref[] = R"({
    "service_provider": "google",
    "enable": [
      {
        "url_list": ["*"],
        "tags": ["dlp"]
      }
    ],
    "block_until_verdict": 1,
    "block_large_files": %s
  })";
  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), PRINT,
      base::StringPrintf(kBlockLargePagesPref, bool_setting_value()),
      machine_scope());

  base::RunLoop content_analysis_run_loop;
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create,
                          content_analysis_run_loop.QuitClosure()));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);

  // Create the large page.
  ContentAnalysisDelegate::Data data;
  constexpr int64_t kLargeSize = 51 * 1024 * 1024;
  base::MappedReadOnlyRegion page =
      base::ReadOnlySharedMemoryRegion::Create(kLargeSize);
  std::ranges::fill(base::span(page.mapping), 'a');
  data.page = std::move(page.region);

  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(browser()->profile(),
                                                 GURL(kTestUrl), &data, PRINT));

  // The page should be reported as unscanned.
  test::EventReportValidator validator(client());
  validator.ExpectUnscannedFileEvent(
      /*url*/ "about:blank",
      /*tab_url*/ "about:blank",
      /*source*/ "",
      /*destination*/ "",
      /*filename*/ "about:blank",
      // python3 -c "print('a' * (51 * 1024 * 1024), end='')" |\
      // sha256sum |  tr '[:lower:]' '[:upper:]'
      /*sha*/ "",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerPagePrint,
      /*reason*/ "FILE_TOO_LARGE",
      /*mimetypes*/ DocMimeTypes(),
      /*size*/ std::nullopt,
      /*result*/
      expected_result() ? safe_browsing::EventResultToString(
                              safe_browsing::EventResult::ALLOWED)
                        : safe_browsing::EventResultToString(
                              safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*content_transfer_method*/ std::nullopt);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const ContentAnalysisDelegate::Data& data,
                          ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.paths_results.empty());
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.page_result, expected_result());

            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::PRINT);

  // If the block setting is on, the large page content won't be sent for deep
  // scanning, so no authorization is needed.
  if (!setting_param()) {
    FakeBinaryUploadServiceStorage()->ReturnAuthorizedResponse();
  }

  run_loop.Run();
  EXPECT_TRUE(called);

  // Ensure the ContentAnalysisDelegate is destroyed before the end of the
  // test.
  content_analysis_run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBlockingSettingBrowserTest,
                       BlockUntilVerdict) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();
  constexpr char kBlockUntilVerdictPref[] = R"({
    "service_provider": "google",
    "enable": [
      {
        "url_list": ["*"],
        "tags": ["dlp", "malware"]
      }
    ],
    "block_until_verdict": %s
  })";
  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), FILE_ATTACHED,
      base::StringPrintf(kBlockUntilVerdictPref, int_setting_value()),
      machine_scope());

  base::RunLoop content_analysis_run_loop;
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create,
                          content_analysis_run_loop.QuitClosure()));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  // Create a file.
  ContentAnalysisDelegate::Data data;
  data.reason = ContentAnalysisRequest::DRAG_AND_DROP;

  CreateFilesForTest({"foo.doc"}, {"foo content"}, &data);
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, FILE_ATTACHED));

  // The file should be reported as malware and sensitive content.
  test::EventReportValidator validator(client());
  ContentAnalysisResponse response;
  response.set_request_token(kScanId1);

  auto* malware_result = response.add_results();
  malware_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  malware_result->set_tag("malware");
  auto* malware_rule = malware_result->add_triggered_rules();
  malware_rule->set_action(TriggeredRule::BLOCK);
  malware_rule->set_rule_name("malware");

  auto* dlp_result = response.add_results();
  dlp_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  dlp_result->set_tag("dlp");
  auto* dlp_rule = dlp_result->add_triggered_rules();
  dlp_rule->set_action(TriggeredRule::BLOCK);
  dlp_rule->set_rule_id("0");
  dlp_rule->set_rule_name("some_dlp_rule");

  FakeBinaryUploadServiceStorage()->SetResponseForFile(
      created_file_paths()[0].AsUTF8Unsafe(),
      BinaryUploadService::Result::SUCCESS, response);
  FakeBinaryUploadServiceStorage()->SetExpectedFinalAction(
      kScanId1, ContentAnalysisAcknowledgement::BLOCK);
  validator.ExpectDangerousDeepScanningResultAndSensitiveDataEvent(
      /*url*/ "about:blank",
      /*tab_url*/ "about:blank",
      /*source*/ "",
      /*destination*/ "",
      /*filename*/
      machine_scope() ? created_file_paths()[0].AsUTF8Unsafe() : "foo.doc",
      // printf "foo content" | sha256sum  |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "B3A2E2EDBAA3C798B4FC267792B1641B94793DE02D870124E5CBE663750B4CFC",
      /*threat_type*/ "DANGEROUS",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      /*dlp_verdict*/ *dlp_result,
      /*mimetypes*/ DocMimeTypes(),
      /*size*/ std::string("foo content").size(),
      // If the policy allows immediate delivery of the file, then the result is
      // ALLOWED even if the verdict obtained afterwards is BLOCKED.
      /*result*/
      safe_browsing::EventResultToString(
          expected_result() ? safe_browsing::EventResult::ALLOWED
                            : safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*scan_id*/ kScanId1,
      /*content_transfer_method*/ "CONTENT_TRANSFER_METHOD_DRAG_AND_DROP");

  bool called = false;
  base::RunLoop run_loop;

  // If the delivery is not delayed, put the quit closure right after the events
  // are reported instead of when the dialog closes.
  if (expected_result()) {
    validator.SetDoneClosure(run_loop.QuitClosure());
  } else {
    SetQuitClosure(run_loop.QuitClosure());
  }

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const ContentAnalysisDelegate::Data& data,
                          ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.text_results.empty());
            ASSERT_EQ(result.paths_results.size(), 1u);
            ASSERT_EQ(result.paths_results[0], expected_result());

            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::DRAG_AND_DROP);

  run_loop.Run();
  EXPECT_TRUE(called);

  // Expect 1 request for initial authentication (unspecified type, to be
  // removed for crbug.com/1090088, then count should be 1), + 1 to scan the
  // file in all cases.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 2);
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->ack_count(), 1);

  // Ensure the ContentAnalysisDelegate is destroyed before the end of the test.
  content_analysis_run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateBlockingSettingBrowserTest,
                       BlockTextUntilVerdict) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();
  constexpr char kBlockUntilVerdictPref[] = R"({
    "service_provider": "google",
    "enable": [
      {
        "url_list": ["*"],
        "tags": ["dlp", "malware"]
      }
    ],
    "block_until_verdict": %s
  })";
  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), BULK_DATA_ENTRY,
      base::StringPrintf(kBlockUntilVerdictPref, int_setting_value()),
      machine_scope());

  base::RunLoop content_analysis_run_loop;
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create,
                          content_analysis_run_loop.QuitClosure()));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);

  // Create text.
  ContentAnalysisDelegate::Data data;
  data.text.emplace_back(text());
  data.reason = ContentAnalysisRequest::CLIPBOARD_PASTE;
  data.clipboard_source = "about:blank";
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, BULK_DATA_ENTRY));

  EXPECT_EQ(data.settings.block_until_verdict != BlockUntilVerdict::kNoBlock,
            setting_param());

  // The text should be reported as sensitive content.
  test::EventReportValidator validator(client());
  ContentAnalysisResponse response;
  response.set_request_token(kScanId1);

  auto* dlp_result = response.add_results();
  dlp_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  dlp_result->set_tag("dlp");
  auto* dlp_rule = dlp_result->add_triggered_rules();
  dlp_rule->set_action(TriggeredRule::BLOCK);
  dlp_rule->set_rule_id("1");
  dlp_rule->set_rule_name("resource rule 1");

  FakeBinaryUploadServiceStorage()->SetResponseForText(
      BinaryUploadService::Result::SUCCESS, response);
  FakeBinaryUploadServiceStorage()->SetExpectedFinalAction(
      kScanId1, ContentAnalysisAcknowledgement::BLOCK);
  validator.ExpectSensitiveDataEvent(
      /*url*/ "about:blank",
      /*tab_url*/ "about:blank",
      /*source*/ "about:blank",
      /*destination*/ "about:blank",
      /*filename*/ "Text data",
      // The hash should not be included for string requests.
      /*sha*/ "",
      /*trigger*/ SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      /*dlp_verdict*/ *dlp_result,
      /*mimetypes*/ TextMimeTypes(),
      /*size*/ 100,
      // If the policy allows immediate delivery of the file, then the result is
      // ALLOWED even if the verdict obtained afterwards is BLOCKED.
      /*result*/
      safe_browsing::EventResultToString(
          expected_result() ? safe_browsing::EventResult::ALLOWED
                            : safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*scan_id*/ kScanId1,
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  bool called = false;
  base::RunLoop run_loop;

  // If the delivery is not delayed, put the quit closure right after the events
  // are reported instead of when the dialog closes.
  if (expected_result()) {
    validator.SetDoneClosure(run_loop.QuitClosure());
  } else {
    SetQuitClosure(run_loop.QuitClosure());
  }

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const ContentAnalysisDelegate::Data& data,
                          ContentAnalysisDelegate::Result& result) {
            ASSERT_TRUE(result.paths_results.empty());
            ASSERT_EQ(result.text_results.size(), 1u);
            ASSERT_EQ(result.text_results[0], expected_result());
            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::PASTE);

  FakeBinaryUploadServiceStorage()->ReturnAuthorizedResponse();
  run_loop.Run();
  EXPECT_TRUE(called);

  // Expect 1 request for text and 1 for authentication of the scanning request.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 2);
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->ack_count(), 1);
  EXPECT_EQ(FakeBinaryUploadServiceStorage()->blocking(),
            data.settings.block_until_verdict != BlockUntilVerdict::kNoBlock);

  // Ensure the ContentAnalysisDelegate is destroyed before the end of the test.
  content_analysis_run_loop.Run();
}

// This class tests if ContentAnalysisDelegate handles the settings
// `default_action` from Connector policies correctly, specifically for cloud
// analysis.
class ContentAnalysisDelegateDefaultActionSettingBrowserTest
    : public ContentAnalysisDelegateBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<safe_browsing::BinaryUploadService::Result, bool>> {
 public:
  ContentAnalysisDelegateDefaultActionSettingBrowserTest()
      : ContentAnalysisDelegateBrowserTestBase(/*machine_scope=*/true) {}

  safe_browsing::BinaryUploadService::Result upload_result() const {
    return std::get<0>(GetParam());
  }

  bool setting_param() const { return std::get<1>(GetParam()); }

  // Use a string since the setting value is inserted into a JSON policy.
  const char* default_action_setting_value() const {
    return setting_param() ? "block" : "allow";
  }

  bool expected_result() const { return !setting_param(); }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    ContentAnalysisDelegateDefaultActionSettingBrowserTest,
    testing::Combine(
        testing::Values(
            safe_browsing::BinaryUploadService::Result::UPLOAD_FAILURE,
            safe_browsing::BinaryUploadService::Result::TIMEOUT,
            safe_browsing::BinaryUploadService::Result::FAILED_TO_GET_TOKEN,
            safe_browsing::BinaryUploadService::Result::TOO_MANY_REQUESTS,
            safe_browsing::BinaryUploadService::Result::UNKNOWN,
            safe_browsing::BinaryUploadService::Result::INCOMPLETE_RESPONSE),
        testing::Bool()));

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateDefaultActionSettingBrowserTest,
                       DefaultAction) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Set up delegate and upload service.
  EnableUploadsScanningAndReporting();
  constexpr char kDefaultActionPref[] = R"({
    "service_provider": "google",
    "enable": [
      {
        "url_list": ["*"],
        "tags": ["dlp"]
      }
    ],
    "block_until_verdict": 1,
    "default_action": "%s"
  })";
  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), BULK_DATA_ENTRY,
      base::StringPrintf(kDefaultActionPref, default_action_setting_value()),
      /*machine_scope=*/true);

  base::RunLoop content_analysis_run_loop;
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create,
                          content_analysis_run_loop.QuitClosure()));

  FakeBinaryUploadServiceStorage()->SetAuthorized(true);

  // Create test data.
  ContentAnalysisDelegate::Data data;
  data.text.emplace_back(text());
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, BULK_DATA_ENTRY));
  ContentAnalysisResponse text_response;
  FakeBinaryUploadServiceStorage()->SetResponseForText(upload_result(),
                                                       text_response);

  bool called = false;
  base::RunLoop run_loop;
  SetQuitClosure(run_loop.QuitClosure());

  // Start test.
  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const ContentAnalysisDelegate::Data& data,
                          ContentAnalysisDelegate::Result& result) {
            ASSERT_EQ(result.text_results[0], expected_result());
            called = true;
          }),
      safe_browsing::DeepScanAccessPoint::PASTE);

  FakeBinaryUploadServiceStorage()->ReturnAuthorizedResponse();

  run_loop.Run();
  EXPECT_TRUE(called);

  // Ensure the ContentAnalysisDelegate is destroyed before the end of the test.
  content_analysis_run_loop.Run();
}

// This class tests that ContentAnalysisDelegate is handled correctly when the
// requests are already unauthorized. The test parameter represents if the scan
// is set to be blocking through policy.
class ContentAnalysisDelegateUnauthorizedBrowserTest
    : public ContentAnalysisDelegateBrowserTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  ContentAnalysisDelegateUnauthorizedBrowserTest()
      : ContentAnalysisDelegateBrowserTestBase(machine_scope()) {}

  bool machine_scope() const { return std::get<0>(GetParam()); }
  bool blocking_scan() const { return std::get<1>(GetParam()); }

  const char* dm_token() const {
    return machine_scope() ? kBrowserDMToken : kProfileDMToken;
  }

  void SetUpScanning(bool file_scan) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    SetDMTokenForTesting(policy::DMToken::CreateValidToken(dm_token()));
#else
    if (machine_scope()) {
      SetDMTokenForTesting(policy::DMToken::CreateValidToken(dm_token()));
    } else {
      test::SetProfileDMToken(browser()->profile(), dm_token());
    }
#endif

    std::string pref = base::StringPrintf(
        R"({
          "service_provider": "google",
          "enable": [
            {
              "url_list": ["*"],
              "tags": ["dlp", "malware"]
            }
          ],
          "block_until_verdict": %d
        })",
        blocking_scan() ? 1 : 0);

    enterprise_connectors::test::SetAnalysisConnector(
        browser()->profile()->GetPrefs(),
        file_scan ? FILE_ATTACHED : BULK_DATA_ENTRY, pref, machine_scope());
    file_scan_ = file_scan;
  }

  // The dialog should appear on blocking scans for both paste and files upload,
  // because CBUS retries authorizarion check first and then update the scan
  // result.
  void ConstructorCalled(ContentAnalysisDialog* dialog,
                         base::TimeTicks timestamp) override {
    ASSERT_TRUE(blocking_scan());
  }

  void ViewsFirstShown(ContentAnalysisDialog* dialog,
                       base::TimeTicks timestamp) override {
    ASSERT_TRUE(blocking_scan());
  }

  void DialogUpdated(ContentAnalysisDialog* dialog,
                     FinalContentAnalysisResult result) override {
    ASSERT_TRUE(blocking_scan());
  }

  void DestructorCalled(ContentAnalysisDialog* dialog) override {
    ASSERT_TRUE(blocking_scan());
    CallQuitClosure();
  }

 protected:
  bool file_scan_ = false;
};

INSTANTIATE_TEST_SUITE_P(,
                         ContentAnalysisDelegateUnauthorizedBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateUnauthorizedBrowserTest, Paste) {
  // The reading of the browser DM token is blocking and happens in this test
  // when checking if the browser is enrolled.
  base::ScopedAllowBlockingForTesting allow_blocking;

  SetUpScanning(/*file_scan*/ false);

  base::RunLoop content_analysis_run_loop;
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create,
                          content_analysis_run_loop.QuitClosure()));

  FakeBinaryUploadServiceStorage()->SetAuthForTesting(
      dm_token(), BinaryUploadService::Result::UNAUTHORIZED);
  FakeBinaryUploadServiceStorage()->SetAuthorized(false);

  bool called = false;
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  ContentAnalysisDelegate::Data data;
  data.text.emplace_back(text());
  data.reason = ContentAnalysisRequest::CLIPBOARD_PASTE;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, BULK_DATA_ENTRY));

  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&quit_closure, &called](const ContentAnalysisDelegate::Data& data,
                                   ContentAnalysisDelegate::Result& result) {
            ASSERT_EQ(result.text_results.size(), 1u);
            ASSERT_EQ(result.paths_results.size(), 0u);
            ASSERT_TRUE(result.text_results[0]);
            called = true;
            quit_closure.Run();
          }),
      safe_browsing::DeepScanAccessPoint::PASTE);

  // Make sure auth retry fails.
  FakeBinaryUploadServiceStorage()->ReturnAuthorizedResponse();

  run_loop.Run();
  EXPECT_TRUE(called);

  // 1 request to retry authentication.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 1);
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->ack_count(), 0);

  // Ensure the ContentAnalysisDelegate is destroyed before the end of the test.
  content_analysis_run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(ContentAnalysisDelegateUnauthorizedBrowserTest, Files) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  SetUpScanning(/*file_scan*/ true);

  base::RunLoop content_analysis_run_loop;
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&MinimalFakeContentAnalysisDelegate::Create,
                          content_analysis_run_loop.QuitClosure()));

  FakeBinaryUploadServiceStorage()->SetAuthForTesting(
      dm_token(), BinaryUploadService::Result::UNAUTHORIZED);
  // Make sure all auth retries fail.
  FakeBinaryUploadServiceStorage()->SetAuthorized(false);
  FakeBinaryUploadServiceStorage()->SetShouldAutomaticallyAuthorize(true);

  bool called = false;
  base::RunLoop run_loop;
  std::optional<base::RepeatingClosure> quit_closure = std::nullopt;

  // If the scan is blocking, we can call the quit closure when the dialog
  // closes. If it's not, call it at the end of the result callback.
  if (blocking_scan()) {
    SetQuitClosure(run_loop.QuitClosure());
  } else {
    quit_closure = run_loop.QuitClosure();
  }

  ContentAnalysisDelegate::Data data;
  CreateFilesForTest({"file1.doc", "file2.doc"}, {"content1", "content2"},
                     &data);
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(
      browser()->profile(), GURL(kTestUrl), &data, FILE_ATTACHED));

  ContentAnalysisDelegate::CreateForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(), std::move(data),
      base::BindLambdaForTesting(
          [&quit_closure, &called](const ContentAnalysisDelegate::Data& data,
                                   ContentAnalysisDelegate::Result& result) {
            ASSERT_EQ(result.text_results.size(), 0u);
            ASSERT_EQ(result.paths_results.size(), 2u);
            ASSERT_TRUE(result.paths_results[0]);
            ASSERT_TRUE(result.paths_results[1]);
            called = true;
            if (quit_closure.has_value()) {
              quit_closure.value().Run();
            }
          }),
      safe_browsing::DeepScanAccessPoint::UPLOAD);

  run_loop.Run();
  EXPECT_TRUE(called);

  // Ensure the ContentAnalysisDelegate is destroyed before the end of the test.
  content_analysis_run_loop.Run();

  // Check result after both blocking and non-blocking scan finishes.
  // 2 request to retry authentication for each file.
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->requests_count(), 2);
  ASSERT_EQ(FakeBinaryUploadServiceStorage()->ack_count(), 0);
}

}  // namespace enterprise_connectors
