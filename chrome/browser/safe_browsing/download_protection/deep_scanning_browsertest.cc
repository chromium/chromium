// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/base64.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_fcm_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_browsertest_base.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#include "chrome/browser/safe_browsing/download_protection/ppapi_download_request.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/db/test_database_manager.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "components/safe_browsing/core/proto/webprotect.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_utils.h"
#include "services/network/test/test_utils.h"

namespace safe_browsing {

namespace {

// Extract the metadata proto from the raw request string. Returns true on
// success.
bool GetUploadMetadata(
    const std::string& upload_request,
    enterprise_connectors::ContentAnalysisRequest* out_proto) {
  // The request is of the following format, see multipart_uploader.h for
  // details:
  // ---MultipartBoundary---
  // <Headers for metadata>
  //
  // <Base64-encoded metadata>
  // ---MultipartBoundary---
  // <Headers for uploaded data>
  //
  // <Uploaded data>
  // ---MultipartBoundary---
  size_t boundary_end = upload_request.find("\r\n");
  std::string multipart_boundary = upload_request.substr(0, boundary_end);

  size_t headers_end = upload_request.find("\r\n\r\n");
  size_t metadata_end =
      upload_request.find("\r\n" + multipart_boundary, headers_end);
  std::string encoded_metadata =
      upload_request.substr(headers_end + 4, metadata_end - headers_end - 4);

  std::string serialized_metadata;
  base::Base64Decode(encoded_metadata, &serialized_metadata);
  return out_proto->ParseFromString(serialized_metadata);
}

}  // namespace

class FakeBinaryFCMService : public BinaryFCMService {
 public:
  FakeBinaryFCMService() {}

  void GetInstanceID(GetInstanceIDCallback callback) override {
    std::move(callback).Run("test_instance_id");
  }

  void UnregisterInstanceID(const std::string& token,
                            UnregisterInstanceIDCallback callback) override {
    // Always successfully unregister.
    std::move(callback).Run(true);
  }
};

// Integration tests for download deep scanning behavior, only mocking network
// traffic and FCM dependencies.
class DownloadDeepScanningBrowserTest
    : public DeepScanningBrowserTestBase,
      public content::DownloadManager::Observer,
      public download::DownloadItem::Observer {
 public:
  DownloadDeepScanningBrowserTest() = default;

  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override {
    item->AddObserver(this);
    download_items_.insert(item);
  }

  void OnDownloadDestroyed(download::DownloadItem* item) override {
    download_items_.erase(item);
  }

  void SetUpReporting() {
    SetUnsafeEventsReportingPolicy(true);
    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
        browser()->profile())
        ->SetCloudPolicyClientForTesting(client_.get());
  }

  policy::MockCloudPolicyClient* client() { return client_.get(); }

 protected:
  void SetUp() override {
    test_sb_factory_ = std::make_unique<TestSafeBrowsingServiceFactory>();
    test_sb_factory_->UseV4LocalDatabaseManager();
    SafeBrowsingService::RegisterFactory(test_sb_factory_.get());

    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();

    SafeBrowsingService::RegisterFactory(nullptr);
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
    ASSERT_TRUE(embedded_test_server()->Start());
    AddUrlsToCheckComplianceOfDownloadsForConnectors(
        {embedded_test_server()->base_url().spec()});

    SetBinaryUploadServiceTestFactory();
    SetUrlLoaderInterceptor();
    ObserveDownloadManager();
    AuthorizeForDeepScanning();

    SetDMTokenForTesting(
        policy::DMToken::CreateValidTokenForTesting("dm_token"));
    SetDlpPolicyForConnectors(CheckContentComplianceValues::CHECK_DOWNLOADS);
    SetMalwarePolicyForConnectors(
        SendFilesForMalwareCheckValues::SEND_DOWNLOADS);
    SetAllowPasswordProtectedFilesPolicyForConnectors(
        AllowPasswordProtectedFilesValues::ALLOW_NONE);
  }

  void WaitForDownloadToFinish() {
    content::DownloadManager* download_manager =
        content::BrowserContext::GetDownloadManager(browser()->profile());
    content::DownloadTestObserverTerminal observer(
        download_manager, 1,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_QUIT);
    observer.WaitForFinished();
  }

  void WaitForDeepScanRequest(bool is_advanced_protection) {
    if (is_advanced_protection)
      waiting_for_app_ = true;
    else
      waiting_for_enterprise_ = true;

    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    waiting_for_upload_closure_ = run_loop.QuitClosure();
    run_loop.Run();

    waiting_for_app_ = false;
    waiting_for_enterprise_ = false;
  }

  void WaitForMetadataCheck() {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    waiting_for_metadata_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void ExpectMetadataResponse(const ClientDownloadResponse& response) {
    test_sb_factory_->test_safe_browsing_service()
        ->GetTestUrlLoaderFactory()
        ->AddResponse(PPAPIDownloadRequest::GetDownloadRequestUrl().spec(),
                      response.SerializeAsString());
  }

  void ExpectDeepScanSynchronousResponse(
      bool is_advanced_protection,
      const DeepScanningClientResponse& response) {
    test_sb_factory_->test_safe_browsing_service()
        ->GetTestUrlLoaderFactory()
        ->AddResponse(
            BinaryUploadService::GetUploadUrl(is_advanced_protection).spec(),
            response.SerializeAsString());
  }

  void ExpectContentAnalysisSynchronousResponse(
      bool is_advanced_protection,
      const enterprise_connectors::ContentAnalysisResponse& response,
      const std::vector<std::string>& tags) {
    connector_url_ =
        "https://safebrowsing.google.com/safebrowsing/uploads/"
        "scan?device_token=dm_token&connector=OnFileDownloaded";
    for (const std::string& tag : tags)
      connector_url_ += ("&tag=" + tag);

    test_sb_factory_->test_safe_browsing_service()
        ->GetTestUrlLoaderFactory()
        ->AddResponse(connector_url_, response.SerializeAsString());
  }

  base::FilePath GetTestDataDirectory() {
    base::FilePath test_file_directory;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_file_directory);
    return test_file_directory;
  }

  FakeBinaryFCMService* binary_fcm_service() { return binary_fcm_service_; }

  TestSafeBrowsingServiceFactory* test_sb_factory() {
    return test_sb_factory_.get();
  }

  const enterprise_connectors::ContentAnalysisRequest&
  last_app_content_analysis_request() {
    return last_app_content_analysis_request_;
  }

  const DeepScanningClientRequest& last_app_request() {
    return last_app_request_;
  }

  const enterprise_connectors::ContentAnalysisRequest&
  last_enterprise_content_analysis_request() {
    return last_enterprise_content_analysis_request_;
  }

  const DeepScanningClientRequest& last_enterprise_request() {
    return last_enterprise_request_;
  }

  const base::flat_set<download::DownloadItem*>& download_items() {
    return download_items_;
  }

  void SetBinaryUploadServiceTestFactory() {
    BinaryUploadServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(
            &DownloadDeepScanningBrowserTest::CreateBinaryUploadService,
            base::Unretained(this)));
  }

  void ObserveDownloadManager() {
    content::DownloadManager* download_manager =
        content::BrowserContext::GetDownloadManager(browser()->profile());
    download_manager->AddObserver(this);
  }

  void SetUrlLoaderInterceptor() {
    test_sb_factory()->test_safe_browsing_service()->SetUseTestUrlLoaderFactory(
        true);
    test_sb_factory()
        ->test_safe_browsing_service()
        ->GetTestUrlLoaderFactory()
        ->SetInterceptor(base::BindRepeating(
            &DownloadDeepScanningBrowserTest::InterceptRequest,
            base::Unretained(this)));
  }

  template <typename T>
  void SendFcmMessage(const T& response) {
    std::string encoded_proto;
    base::Base64Encode(response.SerializeAsString(), &encoded_proto);
    gcm::IncomingMessage gcm_message;
    gcm_message.data["proto"] = encoded_proto;
    binary_fcm_service()->OnMessage("app_id", gcm_message);
  }

  void AuthorizeForDeepScanning() {
    BinaryUploadServiceFactory::GetForProfile(browser()->profile())
        ->SetAuthForTesting(/*authorized=*/true);
  }

 private:
  std::unique_ptr<KeyedService> CreateBinaryUploadService(
      content::BrowserContext* browser_context) {
    std::unique_ptr<FakeBinaryFCMService> binary_fcm_service =
        std::make_unique<FakeBinaryFCMService>();
    binary_fcm_service_ = binary_fcm_service.get();
    Profile* profile = Profile::FromBrowserContext(browser_context);
    return std::make_unique<BinaryUploadService>(
        g_browser_process->safe_browsing_service()->GetURLLoaderFactory(),
        profile, std::move(binary_fcm_service));
  }

  void InterceptRequest(const network::ResourceRequest& request) {
    if (request.url ==
        BinaryUploadService::GetUploadUrl(/*is_advanced_protection=*/true)) {
      ASSERT_TRUE(GetUploadMetadata(network::GetUploadData(request),
                                    &last_app_content_analysis_request_));
      if (waiting_for_app_)
        std::move(waiting_for_upload_closure_).Run();
    }

    if (request.url ==
        BinaryUploadService::GetUploadUrl(/*is_advanced_protection=*/false)) {
      ASSERT_TRUE(
          GetUploadMetadata(network::GetUploadData(request),
                            &last_enterprise_content_analysis_request_));
      if (waiting_for_enterprise_)
        std::move(waiting_for_upload_closure_).Run();
    }

    if (request.url == connector_url_) {
      ASSERT_TRUE(
          GetUploadMetadata(network::GetUploadData(request),
                            &last_enterprise_content_analysis_request_));
      if (waiting_for_enterprise_)
        std::move(waiting_for_upload_closure_).Run();
    }

    if (request.url == PPAPIDownloadRequest::GetDownloadRequestUrl()) {
      if (waiting_for_metadata_closure_)
        std::move(waiting_for_metadata_closure_).Run();
    }
  }

  std::unique_ptr<TestSafeBrowsingServiceFactory> test_sb_factory_;
  FakeBinaryFCMService* binary_fcm_service_;

  bool waiting_for_app_;
  enterprise_connectors::ContentAnalysisRequest
      last_app_content_analysis_request_;
  DeepScanningClientRequest last_app_request_;

  bool waiting_for_enterprise_;
  enterprise_connectors::ContentAnalysisRequest
      last_enterprise_content_analysis_request_;
  DeepScanningClientRequest last_enterprise_request_;

  std::string connector_url_;

  base::OnceClosure waiting_for_upload_closure_;
  base::OnceClosure waiting_for_metadata_closure_;

  base::flat_set<download::DownloadItem*> download_items_;

  std::unique_ptr<policy::MockCloudPolicyClient> client_;
};

IN_PROC_BROWSER_TEST_F(DownloadDeepScanningBrowserTest,
                       SafeDownloadHasCorrectDangerType) {
  // The file is SAFE according to the metadata check
  ClientDownloadResponse metadata_response;
  metadata_response.set_verdict(ClientDownloadResponse::SAFE);
  ExpectMetadataResponse(metadata_response);

  // The DLP scan runs synchronously, but doesn't find anything.
  enterprise_connectors::ContentAnalysisResponse sync_response;
  auto* dlp_result = sync_response.add_results();
  dlp_result->set_tag("dlp");
  dlp_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  ExpectContentAnalysisSynchronousResponse(/*is_advanced_protection=*/false,
                                           sync_response, {"dlp", "malware"});

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest(/*is_advanced_protection=*/false);

  // The malware scan finishes asynchronously, and doesn't find anything.
  enterprise_connectors::ContentAnalysisResponse async_response;
  async_response.set_request_token(
      last_enterprise_content_analysis_request().request_token());
  auto* malware_result = async_response.add_results();
  malware_result->set_tag("malware");
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  SendFcmMessage(async_response);

  WaitForDownloadToFinish();

  // The file should be deep scanned, and safe.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(
      item->GetDangerType(),
      download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE);
  EXPECT_EQ(item->GetState(), download::DownloadItem::COMPLETE);
}

IN_PROC_BROWSER_TEST_F(DownloadDeepScanningBrowserTest, FailedScanFailsOpen) {
  // The file is SAFE according to the metadata check
  ClientDownloadResponse metadata_response;
  metadata_response.set_verdict(ClientDownloadResponse::SAFE);
  ExpectMetadataResponse(metadata_response);

  // The DLP scan runs synchronously, but doesn't find anything.
  enterprise_connectors::ContentAnalysisResponse sync_response;
  auto* dlp_result = sync_response.add_results();
  dlp_result->set_tag("dlp");
  dlp_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  ExpectContentAnalysisSynchronousResponse(/*is_advanced_protection=*/false,
                                           sync_response, {"dlp", "malware"});

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest(/*is_advanced_protection=*/false);

  // The malware scan finishes asynchronously, and fails
  enterprise_connectors::ContentAnalysisResponse async_response;
  async_response.set_request_token(
      last_enterprise_content_analysis_request().request_token());
  auto* malware_result = async_response.add_results();
  malware_result->set_tag("malware");
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::FAILURE);
  SendFcmMessage(async_response);

  WaitForDownloadToFinish();

  // The file should be safe, but not deep scanned.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  EXPECT_EQ(item->GetState(), download::DownloadItem::COMPLETE);
}

IN_PROC_BROWSER_TEST_F(DownloadDeepScanningBrowserTest,
                       PartialFailureShowsMalwareWarning) {
  // The file is SAFE according to the metadata check
  ClientDownloadResponse metadata_response;
  metadata_response.set_verdict(ClientDownloadResponse::SAFE);
  ExpectMetadataResponse(metadata_response);

  // The DLP scan runs synchronously, and fails.
  enterprise_connectors::ContentAnalysisResponse sync_response;
  auto* dlp_result = sync_response.add_results();
  dlp_result->set_tag("dlp");
  dlp_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::FAILURE);
  ExpectContentAnalysisSynchronousResponse(/*is_advanced_protection=*/false,
                                           sync_response, {"dlp", "malware"});

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest(/*is_advanced_protection=*/false);

  // The malware scan finishes asynchronously, and finds malware.
  enterprise_connectors::ContentAnalysisResponse async_response;
  async_response.set_request_token(
      last_enterprise_content_analysis_request().request_token());
  auto* malware_result = async_response.add_results();
  malware_result->set_tag("malware");
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* malware_rule = malware_result->add_triggered_rules();
  malware_rule->set_action(enterprise_connectors::TriggeredRule::BLOCK);
  malware_rule->set_rule_name("malware");
  SendFcmMessage(async_response);

  WaitForDownloadToFinish();

  // The file should be dangerous.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(
      item->GetDangerType(),
      download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT);
  EXPECT_EQ(item->GetState(), download::DownloadItem::IN_PROGRESS);
}

IN_PROC_BROWSER_TEST_F(DownloadDeepScanningBrowserTest,
                       PartialFailureShowsDlpWarning) {
  // The file is SAFE according to the metadata check
  ClientDownloadResponse metadata_response;
  metadata_response.set_verdict(ClientDownloadResponse::SAFE);
  ExpectMetadataResponse(metadata_response);

  // The DLP scan runs synchronously, and finds a violation.
  enterprise_connectors::ContentAnalysisResponse sync_response;
  auto* dlp_result = sync_response.add_results();
  dlp_result->set_tag("dlp");
  dlp_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* dlp_rule = dlp_result->add_triggered_rules();
  dlp_rule->set_action(enterprise_connectors::TriggeredRule::BLOCK);
  ExpectContentAnalysisSynchronousResponse(/*is_advanced_protection=*/false,
                                           sync_response, {"dlp", "malware"});

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest(/*is_advanced_protection=*/false);

  // The malware scan finishes asynchronously, and fails.
  enterprise_connectors::ContentAnalysisResponse async_response;
  async_response.set_request_token(
      last_enterprise_content_analysis_request().request_token());
  auto* malware_result = async_response.add_results();
  malware_result->set_tag("malware");
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::FAILURE);
  SendFcmMessage(async_response);

  WaitForDownloadToFinish();

  // The file should be blocked for sensitive content.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::
                DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK);
  EXPECT_EQ(item->GetState(), download::DownloadItem::INTERRUPTED);
}

IN_PROC_BROWSER_TEST_F(DownloadDeepScanningBrowserTest,
                       DangerousHostNotMalwareScanned) {
  // The file is DANGEROUS_HOST according to the metadata check
  ClientDownloadResponse metadata_response;
  metadata_response.set_verdict(ClientDownloadResponse::DANGEROUS_HOST);
  ExpectMetadataResponse(metadata_response);

  // The DLP scan still runs, but finds nothing
  enterprise_connectors::ContentAnalysisResponse sync_response;
  auto* result = sync_response.add_results();
  result->set_tag("dlp");
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  ExpectContentAnalysisSynchronousResponse(/*is_advanced_protection=*/false,
                                           sync_response, {"dlp", "malware"});

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/signed.exe");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDownloadToFinish();

  // The file should be blocked.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST);
  EXPECT_EQ(item->GetState(), download::DownloadItem::IN_PROGRESS);
}

IN_PROC_BROWSER_TEST_F(DownloadDeepScanningBrowserTest,
                       PasswordProtectedTxtFilesAreBlocked) {
  // The file is SAFE according to the metadata check
  ClientDownloadResponse metadata_response;
  metadata_response.set_verdict(ClientDownloadResponse::SAFE);
  ExpectMetadataResponse(metadata_response);

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/encrypted_txt.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDownloadToFinish();

  // The file should be blocked for containing a password protected file.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::
                DOWNLOAD_DANGER_TYPE_BLOCKED_PASSWORD_PROTECTED);
  EXPECT_EQ(item->GetState(), download::DownloadItem::INTERRUPTED);
}

IN_PROC_BROWSER_TEST_F(DownloadDeepScanningBrowserTest, MultipleFCMResponses) {
  SetUpReporting();
  base::HistogramTester histograms;

  // The file is SAFE according to the metadata check
  ClientDownloadResponse metadata_response;
  metadata_response.set_verdict(ClientDownloadResponse::SAFE);
  ExpectMetadataResponse(metadata_response);

  // No scan runs synchronously.
  enterprise_connectors::ContentAnalysisResponse sync_response;
  ExpectContentAnalysisSynchronousResponse(/*is_advanced_protection=*/false,
                                           sync_response, {"dlp", "malware"});

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest(/*is_advanced_protection=*/false);

  // The malware scan finishes asynchronously, and finds malware.
  enterprise_connectors::ContentAnalysisResponse async_response_1;
  async_response_1.set_request_token(
      last_enterprise_content_analysis_request().request_token());
  auto* result = async_response_1.add_results();
  result->set_tag("malware");
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* malware_rule_1 = result->add_triggered_rules();
  malware_rule_1->set_action(enterprise_connectors::TriggeredRule::BLOCK);
  malware_rule_1->set_rule_name("malware");
  SendFcmMessage(async_response_1);

  // A single unsafe event should be recorded for this request.
  std::set<std::string> zip_types = {"application/zip",
                                     "application/x-zip-compressed"};
  EventReportValidator validator(client());
  validator.ExpectDangerousDeepScanningResult(
      /*url*/ url.spec(),
      /*filename*/
      (*download_items().begin())->GetTargetFilePath().AsUTF8Unsafe(),
      // sha256sum chrome/test/data/safe_browsing/download_protection/\
      // zipfile_two_archives.zip |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "339C8FFDAE735C4F1846D0E6FF07FBD85CAEE6D96045AAEF5B30F3220836643C",
      /*threat_type*/ "DANGEROUS",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
      /*mimetypes*/ &zip_types,
      /*size*/ 276,
      /*result*/ EventResultToString(EventResult::WARNED));

  // The DLP scan finishes asynchronously, and finds nothing. The malware result
  // is attached to the response again.
  enterprise_connectors::ContentAnalysisResponse async_response_2;
  async_response_2.set_request_token(
      last_enterprise_content_analysis_request().request_token());
  auto* malware_result = async_response_2.add_results();
  malware_result->set_tag("malware");
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* malware_rule_2 = malware_result->add_triggered_rules();
  malware_rule_2->set_action(enterprise_connectors::TriggeredRule::BLOCK);
  malware_rule_2->set_rule_name("malware");
  auto* dlp_result = async_response_2.add_results();
  dlp_result->set_tag("dlp");
  dlp_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  SendFcmMessage(async_response_2);

  // The file should be blocked.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(
      item->GetDangerType(),
      download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT);
  EXPECT_EQ(item->GetState(), download::DownloadItem::IN_PROGRESS);

  // UMAs for this request should only be recorded once.
  histograms.ExpectUniqueSample("SafeBrowsingBinaryUploadRequest.Result",
                                BinaryUploadService::Result::SUCCESS, 1);
  histograms.ExpectUniqueSample("SafeBrowsingBinaryUploadRequest.DlpResult",
                                true, 1);
  histograms.ExpectUniqueSample("SafeBrowsingBinaryUploadRequest.MalwareResult",
                                true, 1);
}

IN_PROC_BROWSER_TEST_F(DownloadDeepScanningBrowserTest,
                       DlpAndMalwareViolations) {
  SetUpReporting();
  base::HistogramTester histograms;

  // The file is DANGEROUS_HOST according to the metadata check
  ClientDownloadResponse metadata_response;
  metadata_response.set_verdict(ClientDownloadResponse::DANGEROUS_HOST);
  ExpectMetadataResponse(metadata_response);

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // The DLP scan finishes synchronously and find a violation.
  enterprise_connectors::ContentAnalysisResponse sync_response;
  auto* result = sync_response.add_results();
  result->set_tag("dlp");
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* dlp_rule = result->add_triggered_rules();
  dlp_rule->set_action(enterprise_connectors::TriggeredRule::WARN);
  dlp_rule->set_rule_name("dlp_rule_name");
  ExpectContentAnalysisSynchronousResponse(/*is_advanced_protection=*/false,
                                           sync_response, {"dlp"});

  WaitForMetadataCheck();
  WaitForDeepScanRequest(/*is_advanced_protection=*/false);

  // Both the DLP and malware violations generate an event.
  std::set<std::string> zip_types = {"application/zip",
                                     "application/x-zip-compressed"};
  EventReportValidator validator(client());
  validator.ExpectSensitiveDataEventAndDangerousDeepScanningResult(
      /*url*/ url.spec(),
      /*filename*/
      (*download_items().begin())->GetTargetFilePath().AsUTF8Unsafe(),
      // sha256sum chrome/test/data/safe_browsing/download_protection/\
      // zipfile_two_archives.zip |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "339C8FFDAE735C4F1846D0E6FF07FBD85CAEE6D96045AAEF5B30F3220836643C",
      /*threat_type*/ "DANGEROUS_HOST",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
      /*dlp_verdict*/ *result,
      /*mimetypes*/ &zip_types,
      /*size*/ 276,
      /*result*/ EventResultToString(EventResult::WARNED));
  WaitForDownloadToFinish();

  // The file should be blocked.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST);
  EXPECT_EQ(item->GetState(), download::DownloadItem::IN_PROGRESS);

  // UMAs for this request should only be recorded once. The malware metric
  // should not be recorded since no deep malware scan occurred.
  histograms.ExpectUniqueSample("SafeBrowsingBinaryUploadRequest.Result",
                                BinaryUploadService::Result::SUCCESS, 1);
  histograms.ExpectUniqueSample("SafeBrowsingBinaryUploadRequest.DlpResult",
                                true, 1);
  histograms.ExpectUniqueSample("SafeBrowsingBinaryUploadRequest.MalwareResult",
                                true, 0);
}

class DownloadRestrictionsDeepScanningBrowserTest
    : public DownloadDeepScanningBrowserTest {
 public:
  DownloadRestrictionsDeepScanningBrowserTest() = default;
  ~DownloadRestrictionsDeepScanningBrowserTest() override = default;

  void SetUpOnMainThread() override {
    DownloadDeepScanningBrowserTest::SetUpOnMainThread();

    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kDownloadRestrictions,
        static_cast<int>(DownloadPrefs::DownloadRestriction::DANGEROUS_FILES));
    SetDlpPolicyForConnectors(CheckContentComplianceValues::CHECK_NONE);
  }
};

IN_PROC_BROWSER_TEST_F(DownloadRestrictionsDeepScanningBrowserTest,
                       ReportsDownloadsBlockedByDownloadRestrictions) {
  SetUpReporting();

  // The file is DANGEROUS according to the metadata check
  ClientDownloadResponse metadata_response;
  metadata_response.set_verdict(ClientDownloadResponse::DANGEROUS);
  ExpectMetadataResponse(metadata_response);

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForMetadataCheck();

  EventReportValidator validator(client());
  std::set<std::string> zip_types = {"application/zip",
                                     "application/x-zip-compressed"};
  validator.ExpectDangerousDownloadEvent(
      /*url*/ url.spec(),
      (*download_items().begin())->GetTargetFilePath().AsUTF8Unsafe(),
      // sha256sum chrome/test/data/safe_browsing/download_protection/\
      // zipfile_two_archives.zip |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "339C8FFDAE735C4F1846D0E6FF07FBD85CAEE6D96045AAEF5B30F3220836643C",
      /*threat_type*/ "DANGEROUS_FILE_TYPE",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
      /*mimetypes*/ &zip_types,
      /*size*/ 276,
      /*result*/ EventResultToString(EventResult::BLOCKED));

  WaitForDownloadToFinish();

  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  EXPECT_EQ(item->GetState(), download::DownloadItem::INTERRUPTED);
}

class WhitelistedUrlDeepScanningBrowserTest
    : public DownloadDeepScanningBrowserTest {
 public:
  WhitelistedUrlDeepScanningBrowserTest() = default;
  ~WhitelistedUrlDeepScanningBrowserTest() override = default;

  void SetUpOnMainThread() override {
    DownloadDeepScanningBrowserTest::SetUpOnMainThread();

    base::ListValue domain_list;
    domain_list.AppendString(embedded_test_server()->base_url().host_piece());
    browser()->profile()->GetPrefs()->Set(prefs::kSafeBrowsingWhitelistDomains,
                                          domain_list);
  }
};

IN_PROC_BROWSER_TEST_F(WhitelistedUrlDeepScanningBrowserTest,
                       WhitelistedUrlStillDoesDlp) {
  // The file is SAFE according to the metadata check
  ClientDownloadResponse metadata_response;
  metadata_response.set_verdict(ClientDownloadResponse::SAFE);
  ExpectMetadataResponse(metadata_response);

  // The DLP scan runs synchronously, and finds a violation.
  enterprise_connectors::ContentAnalysisResponse sync_response;
  auto* result = sync_response.add_results();
  result->set_tag("dlp");
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* dlp_rule = result->add_triggered_rules();
  dlp_rule->set_action(enterprise_connectors::TriggeredRule::BLOCK);
  ExpectContentAnalysisSynchronousResponse(/*is_advanced_protection=*/false,
                                           sync_response, {"dlp"});

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest(/*is_advanced_protection=*/false);

  WaitForDownloadToFinish();

  // The file should be blocked for sensitive content.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::
                DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK);
  EXPECT_EQ(item->GetState(), download::DownloadItem::INTERRUPTED);
}

}  // namespace safe_browsing
