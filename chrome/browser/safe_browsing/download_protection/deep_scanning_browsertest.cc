// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_fcm_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_browsertest_base.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#include "chrome/browser/safe_browsing/download_protection/deep_scanning_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/ppapi_download_request.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_features.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/save_page_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/network/test/test_utils.h"

namespace safe_browsing {

namespace {

constexpr char kUserName[] = "test@chromium.org";

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

  bool Connected() override { return true; }
};

// Integration tests for download deep scanning behavior, only mocking network
// traffic and FCM dependencies.
class DownloadDeepScanningBrowserTestBase
    : public DeepScanningBrowserTestBase,
      public content::DownloadManager::Observer,
      public download::DownloadItem::Observer {
 public:
  // |connectors_machine_scope| indicates whether the Connector prefs such as
  // OnFileDownloadedEnterpriseConnector and OnSecurityEventEnterpriseConnector
  // should be set at the machine or user scope.
  explicit DownloadDeepScanningBrowserTestBase(bool connectors_machine_scope,
                                               bool is_consumer)
      : is_consumer_(is_consumer),
        connectors_machine_scope_(connectors_machine_scope) {
    scoped_feature_list_.InitWithFeatures(
        {}, {kSafeBrowsingEnterpriseCsd,
             kSafeBrowsingDisableConsumerCsdForEnterprise});
  }

  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override {
    item->AddObserver(this);
    download_items_.insert(item);
  }

  void OnDownloadDestroyed(download::DownloadItem* item) override {
    download_items_.erase(item);
  }

  void SetUpReporting() {
    SetOnSecurityEventReporting(browser()->profile()->GetPrefs(),
                                /*enabled*/ true, /*enabled_event_names*/ {},
                                /*enabled_opt_in_events*/ {},
                                connectors_machine_scope());
    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_->SetDMToken("dm_token");

#if BUILDFLAG(IS_CHROMEOS_ASH)
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        browser()->profile())
        ->SetBrowserCloudPolicyClientForTesting(client_.get());
#else
    if (connectors_machine_scope()) {
      enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
          browser()->profile())
          ->SetBrowserCloudPolicyClientForTesting(client_.get());
    } else {
      enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
          browser()->profile())
          ->SetProfileCloudPolicyClientForTesting(client_.get());
    }
#endif
    identity_test_environment_ =
        std::make_unique<signin::IdentityTestEnvironment>();
    identity_test_environment_->MakePrimaryAccountAvailable(
        kUserName, signin::ConsentLevel::kSync);
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
        browser()->profile())
        ->SetIdentityManagerForTesting(
            identity_test_environment_->identity_manager());
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

    SetBinaryUploadServiceTestFactory();
    SetUrlLoaderInterceptor();
    ObserveDownloadManager();

    if (!is_consumer_) {
      AuthorizeForDeepScanning();

#if BUILDFLAG(IS_CHROMEOS_ASH)
      SetDMTokenForTesting(
          policy::DMToken::CreateValidTokenForTesting("dm_token"));
#else
      if (connectors_machine_scope()) {
        SetDMTokenForTesting(
            policy::DMToken::CreateValidTokenForTesting("dm_token"));
      } else {
        SetProfileDMToken(browser()->profile(), "dm_token");
      }
#endif
      SetAnalysisConnector(browser()->profile()->GetPrefs(),
                           enterprise_connectors::FILE_DOWNLOADED,
                           R"({
                              "service_provider": "google",
                              "enable": [
                                {
                                  "url_list": ["*"],
                                  "tags": ["dlp", "malware"]
                                }
                              ],
                              "block_until_verdict": 1,
                              "block_password_protected": true
                            })",
                           connectors_machine_scope());
    }
  }

  void WaitForDownloadToFinish() {
    content::DownloadManager* download_manager =
        browser()->profile()->GetDownloadManager();
    content::DownloadTestObserverTerminal observer(
        download_manager, 1,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_QUIT);
    observer.WaitForFinished();
  }

  void WaitForDeepScanRequest() {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    waiting_for_upload_closure_ = run_loop.QuitClosure();
    run_loop.Run();
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

  void ExpectContentAnalysisSynchronousResponse(
      const enterprise_connectors::ContentAnalysisResponse& response,
      const std::vector<std::string>& tags) {
    if (is_consumer_) {
      connector_url_ =
          "https://safebrowsing.google.com/safebrowsing/uploads/consumer";
    } else {
      connector_url_ =
          "https://safebrowsing.google.com/safebrowsing/uploads/"
          "scan?device_token=dm_token&connector=OnFileDownloaded";
      for (const std::string& tag : tags)
        connector_url_ += ("&tag=" + tag);
    }

    test_sb_factory_->test_safe_browsing_service()
        ->GetTestUrlLoaderFactory()
        ->AddResponse(connector_url_, response.SerializeAsString());
  }

  void ExpectContentAnalysisUploadFailure(
      net::HttpStatusCode status_code,
      const std::vector<std::string>& tags) {
    if (is_consumer_) {
      connector_url_ =
          "https://safebrowsing.google.com/safebrowsing/uploads/consumer";
    } else {
      connector_url_ =
          "https://safebrowsing.google.com/safebrowsing/uploads/"
          "scan?device_token=dm_token&connector=OnFileDownloaded";
      for (const std::string& tag : tags)
        connector_url_ += ("&tag=" + tag);
    }

    test_sb_factory_->test_safe_browsing_service()
        ->GetTestUrlLoaderFactory()
        ->AddResponse(connector_url_, "", status_code);
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

  const enterprise_connectors::ContentAnalysisRequest& last_request() const {
    return last_request_;
  }

  const base::flat_set<download::DownloadItem*>& download_items() {
    return download_items_;
  }

  void SetBinaryUploadServiceTestFactory() {
    CloudBinaryUploadServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(
            &DownloadDeepScanningBrowserTestBase::CreateBinaryUploadService,
            base::Unretained(this)));
  }

  void ObserveDownloadManager() {
    content::DownloadManager* download_manager =
        browser()->profile()->GetDownloadManager();
    download_manager->AddObserver(this);
  }

  void SetUrlLoaderInterceptor() {
    test_sb_factory()->test_safe_browsing_service()->SetUseTestUrlLoaderFactory(
        true);
    test_sb_factory()
        ->test_safe_browsing_service()
        ->GetTestUrlLoaderFactory()
        ->SetInterceptor(base::BindRepeating(
            &DownloadDeepScanningBrowserTestBase::InterceptRequest,
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
    static_cast<safe_browsing::CloudBinaryUploadService*>(
        CloudBinaryUploadServiceFactory::GetForProfile(browser()->profile()))
        ->SetAuthForTesting("dm_token", /*authorized=*/true);
  }

  bool connectors_machine_scope() const { return connectors_machine_scope_; }

 private:
  std::unique_ptr<KeyedService> CreateBinaryUploadService(
      content::BrowserContext* browser_context) {
    std::unique_ptr<FakeBinaryFCMService> binary_fcm_service =
        std::make_unique<FakeBinaryFCMService>();
    binary_fcm_service_ = binary_fcm_service.get();
    Profile* profile = Profile::FromBrowserContext(browser_context);
    return std::make_unique<safe_browsing::CloudBinaryUploadService>(
        g_browser_process->safe_browsing_service()->GetURLLoaderFactory(
            profile),
        profile, std::move(binary_fcm_service));
  }

  std::string GetDataPipeUploadData(const network::ResourceRequest& request) {
    EXPECT_TRUE(request.request_body);
    EXPECT_EQ(1u, request.request_body->elements()->size());
    network::DataElement& data_pipe_element =
        (*request.request_body->elements_mutable())[0];

    data_pipe_getter_.reset();
    data_pipe_getter_.Bind(data_pipe_element.As<network::DataElementDataPipe>()
                               .ReleaseDataPipeGetter());
    EXPECT_TRUE(data_pipe_getter_);

    mojo::ScopedDataPipeProducerHandle data_pipe_producer;
    mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
    base::RunLoop run_loop;
    EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(nullptr, data_pipe_producer,
                                                   data_pipe_consumer));
    data_pipe_getter_->Read(
        std::move(data_pipe_producer),
        base::BindLambdaForTesting([&run_loop](int32_t status, uint64_t size) {
          EXPECT_EQ(net::OK, status);
          run_loop.Quit();
        }));
    data_pipe_getter_.FlushForTesting();
    run_loop.Run();

    EXPECT_TRUE(data_pipe_consumer.is_valid());
    std::string body;
    while (true) {
      char buffer[1024];
      uint32_t read_size = sizeof(buffer);
      MojoResult result = data_pipe_consumer->ReadData(
          buffer, &read_size, MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        base::RunLoop().RunUntilIdle();
        continue;
      }
      if (result != MOJO_RESULT_OK) {
        break;
      }
      body.append(buffer, read_size);
    }

    return body;
  }

  void InterceptRequest(const network::ResourceRequest& request) {
    if (is_consumer_) {
      if (request.url == safe_browsing::CloudBinaryUploadService::GetUploadUrl(
                             /*is_consumer_scan_eligible=*/true)) {
        ASSERT_TRUE(
            GetUploadMetadata(GetDataPipeUploadData(request), &last_request_));
        if (waiting_for_upload_closure_)
          std::move(waiting_for_upload_closure_).Run();
      }
    } else {
      if (request.url == safe_browsing::CloudBinaryUploadService::GetUploadUrl(
                             /*is_consumer_scan_eligible=*/false)) {
        ASSERT_TRUE(
            GetUploadMetadata(GetDataPipeUploadData(request), &last_request_));
        if (waiting_for_upload_closure_)
          std::move(waiting_for_upload_closure_).Run();
      }

      if (request.url == connector_url_) {
        ASSERT_TRUE(
            GetUploadMetadata(GetDataPipeUploadData(request), &last_request_));
        if (waiting_for_upload_closure_)
          std::move(waiting_for_upload_closure_).Run();
      }
    }

    if (request.url == PPAPIDownloadRequest::GetDownloadRequestUrl()) {
      if (waiting_for_metadata_closure_)
        std::move(waiting_for_metadata_closure_).Run();
    }
  }

  bool is_consumer_;

  std::unique_ptr<TestSafeBrowsingServiceFactory> test_sb_factory_;
  raw_ptr<FakeBinaryFCMService, DanglingUntriaged> binary_fcm_service_;

  enterprise_connectors::ContentAnalysisRequest last_request_;

  std::string connector_url_;

  base::OnceClosure waiting_for_upload_closure_;
  base::OnceClosure waiting_for_metadata_closure_;

  base::flat_set<download::DownloadItem*> download_items_;

  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_environment_;

  bool connectors_machine_scope_;

  mojo::Remote<network::mojom::DataPipeGetter> data_pipe_getter_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

class ConsumerDeepScanningBrowserTest
    : public DownloadDeepScanningBrowserTestBase {
 public:
  ConsumerDeepScanningBrowserTest()
      : DownloadDeepScanningBrowserTestBase(/*connectors_machine_scope=*/true,
                                            /*is_consumer=*/true) {}
};

class DownloadDeepScanningBrowserTest
    : public DownloadDeepScanningBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  DownloadDeepScanningBrowserTest()
      : DownloadDeepScanningBrowserTestBase(GetParam(), /*is_consumer=*/false) {
  }
};

INSTANTIATE_TEST_SUITE_P(, DownloadDeepScanningBrowserTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(DownloadDeepScanningBrowserTest,
                       SafeDownloadHasCorrectDangerType) {
  // This allows the blocking DM token reads happening on profile-Connector
  // triggers.
  base::ScopedAllowBlockingForTesting allow_blocking;

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
  ExpectContentAnalysisSynchronousResponse(sync_response, {"dlp", "malware"});

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest();

  // The malware scan finishes asynchronously, and doesn't find anything.
  enterprise_connectors::ContentAnalysisResponse async_response;
  async_response.set_request_token(last_request().request_token());
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

IN_PROC_BROWSER_TEST_P(DownloadDeepScanningBrowserTest, FailedScanFailsOpen) {
  // This allows the blocking DM token reads happening on profile-Connector
  // triggers.
  base::ScopedAllowBlockingForTesting allow_blocking;

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
  ExpectContentAnalysisSynchronousResponse(sync_response, {"dlp", "malware"});

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest();

  // The malware scan finishes asynchronously, and fails
  enterprise_connectors::ContentAnalysisResponse async_response;
  async_response.set_request_token(last_request().request_token());
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

IN_PROC_BROWSER_TEST_P(DownloadDeepScanningBrowserTest,
                       PartialFailureShowsMalwareWarning) {
  // This allows the blocking DM token reads happening on profile-Connector
  // triggers.
  base::ScopedAllowBlockingForTesting allow_blocking;

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
  ExpectContentAnalysisSynchronousResponse(sync_response, {"dlp", "malware"});

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest();

  // The malware scan finishes asynchronously, and finds malware.
  enterprise_connectors::ContentAnalysisResponse async_response;
  async_response.set_request_token(last_request().request_token());
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

IN_PROC_BROWSER_TEST_P(DownloadDeepScanningBrowserTest,
                       PartialFailureShowsDlpWarning) {
  // This allows the blocking DM token reads happening on profile-Connector
  // triggers.
  base::ScopedAllowBlockingForTesting allow_blocking;

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
  ExpectContentAnalysisSynchronousResponse(sync_response, {"dlp", "malware"});

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest();

  // The malware scan finishes asynchronously, and fails.
  enterprise_connectors::ContentAnalysisResponse async_response;
  async_response.set_request_token(last_request().request_token());
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

IN_PROC_BROWSER_TEST_P(DownloadDeepScanningBrowserTest,
                       DangerousHostNotMalwareScanned) {
  base::HistogramTester histograms;

  // The file is DANGEROUS_HOST according to the metadata check
  ClientDownloadResponse metadata_response;
  metadata_response.set_verdict(ClientDownloadResponse::DANGEROUS_HOST);
  ExpectMetadataResponse(metadata_response);

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

  histograms.ExpectTotalCount("SafeBrowsingBinaryUploadRequest.Result", 0);
}

IN_PROC_BROWSER_TEST_P(DownloadDeepScanningBrowserTest,
                       PasswordProtectedTxtFilesAreBlocked) {
  // This allows the blocking DM token reads happening on profile-Connector
  // triggers.
  base::ScopedAllowBlockingForTesting allow_blocking;

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

IN_PROC_BROWSER_TEST_P(DownloadDeepScanningBrowserTest, MultipleFCMResponses) {
  // This allows the blocking DM token reads happening on profile-Connector
  // triggers.
  base::ScopedAllowBlockingForTesting allow_blocking;

  SetUpReporting();
  base::HistogramTester histograms;

  // The file is SAFE according to the metadata check
  ClientDownloadResponse metadata_response;
  metadata_response.set_verdict(ClientDownloadResponse::SAFE);
  ExpectMetadataResponse(metadata_response);

  // No scan runs synchronously.
  enterprise_connectors::ContentAnalysisResponse sync_response;
  ExpectContentAnalysisSynchronousResponse(sync_response, {"dlp", "malware"});

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest();

  // The malware scan finishes asynchronously, and finds malware.
  enterprise_connectors::ContentAnalysisResponse async_response_1;
  async_response_1.set_request_token(last_request().request_token());
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
      /*source*/ absl::nullopt,
      /*destination*/ absl::nullopt,
      /*filename*/ "zipfile_two_archives.zip",
      // sha256sum chrome/test/data/safe_browsing/download_protection/\
      // zipfile_two_archives.zip |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "339C8FFDAE735C4F1846D0E6FF07FBD85CAEE6D96045AAEF5B30F3220836643C",
      /*threat_type*/ "DANGEROUS",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
      /*mimetypes*/ &zip_types,
      /*size*/ 276,
      /*result*/ EventResultToString(EventResult::WARNED),
      /*username*/ kUserName,
      /*scan_id*/ last_request().request_token());

  // The DLP scan finishes asynchronously, and finds nothing. The malware result
  // is attached to the response again.
  enterprise_connectors::ContentAnalysisResponse async_response_2;
  async_response_2.set_request_token(last_request().request_token());
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

IN_PROC_BROWSER_TEST_P(DownloadDeepScanningBrowserTest,
                       DlpAndMalwareViolations) {
  // This allows the blocking DM token reads happening on profile-Connector
  // triggers.
  base::ScopedAllowBlockingForTesting allow_blocking;

  SetUpReporting();
  base::HistogramTester histograms;

  // The file is POTENTIALLY_UNWANTED according to the metadata check
  ClientDownloadResponse metadata_response;
  metadata_response.set_verdict(ClientDownloadResponse::POTENTIALLY_UNWANTED);
  ExpectMetadataResponse(metadata_response);

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // The DLP scan finishes synchronously and find a violation. Malware scanning
  // finds nothing.
  enterprise_connectors::ContentAnalysisResponse sync_response;
  auto* dlp_result = sync_response.add_results();
  dlp_result->set_tag("dlp");
  dlp_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* dlp_rule = dlp_result->add_triggered_rules();
  dlp_rule->set_action(enterprise_connectors::TriggeredRule::WARN);
  dlp_rule->set_rule_name("dlp_rule_name");
  auto* malware_result = sync_response.add_results();
  malware_result->set_tag("malware");
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  ExpectContentAnalysisSynchronousResponse(sync_response, {"dlp", "malware"});

  WaitForMetadataCheck();
  WaitForDeepScanRequest();

  // Both the DLP and malware violations generate an event.
  std::set<std::string> zip_types = {"application/zip",
                                     "application/x-zip-compressed"};
  EventReportValidator validator(client());
  validator.ExpectSensitiveDataEventAndDangerousDeepScanningResult(
      /*url*/ url.spec(),
      /*filename*/ "zipfile_two_archives.zip",
      // sha256sum chrome/test/data/safe_browsing/download_protection/\
      // zipfile_two_archives.zip |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "339C8FFDAE735C4F1846D0E6FF07FBD85CAEE6D96045AAEF5B30F3220836643C",
      /*threat_type*/ "POTENTIALLY_UNWANTED",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
      /*dlp_verdict*/ *dlp_result,
      /*mimetypes*/ &zip_types,
      /*size*/ 276,
      /*result*/ EventResultToString(EventResult::WARNED),
      /*username*/ kUserName,
      /*scan_id*/ last_request().request_token());
  WaitForDownloadToFinish();

  // The file should be blocked.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(
      item->GetDangerType(),
      download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED);
  EXPECT_EQ(item->GetState(), download::DownloadItem::IN_PROGRESS);

  // UMAs for this request should only be recorded once.
  histograms.ExpectUniqueSample("SafeBrowsingBinaryUploadRequest.Result",
                                BinaryUploadService::Result::SUCCESS, 1);
  histograms.ExpectUniqueSample("SafeBrowsingBinaryUploadRequest.DlpResult",
                                true, 1);
  histograms.ExpectUniqueSample("SafeBrowsingBinaryUploadRequest.MalwareResult",
                                true, 1);
}

class DownloadRestrictionsDeepScanningBrowserTest
    : public DownloadDeepScanningBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  DownloadRestrictionsDeepScanningBrowserTest()
      : DownloadDeepScanningBrowserTestBase(GetParam(), /*is_consumer=*/false) {
  }
  ~DownloadRestrictionsDeepScanningBrowserTest() override = default;

  void SetUpOnMainThread() override {
    DownloadDeepScanningBrowserTestBase::SetUpOnMainThread();

    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kDownloadRestrictions,
        static_cast<int>(DownloadPrefs::DownloadRestriction::DANGEROUS_FILES));
    SetAnalysisConnector(browser()->profile()->GetPrefs(),
                         enterprise_connectors::FILE_DOWNLOADED,
                         R"({
                              "service_provider": "google",
                              "enable": [
                                {
                                  "url_list": ["*"],
                                  "tags": ["malware"]
                                }
                              ],
                              "block_until_verdict": 1
                            })",
                         connectors_machine_scope());
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         DownloadRestrictionsDeepScanningBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(DownloadRestrictionsDeepScanningBrowserTest,
                       ReportsDownloadsBlockedByDownloadRestrictions) {
  // This allows the blocking DM token reads happening on profile-Connector
  // triggers.
  base::ScopedAllowBlockingForTesting allow_blocking;

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
      (*download_items().begin())
          ->GetTargetFilePath()
          .BaseName()
          .AsUTF8Unsafe(),
      // sha256sum chrome/test/data/safe_browsing/download_protection/\
      // zipfile_two_archives.zip |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "339C8FFDAE735C4F1846D0E6FF07FBD85CAEE6D96045AAEF5B30F3220836643C",
      /*threat_type*/ "DANGEROUS_FILE_TYPE",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
      /*mimetypes*/ &zip_types,
      /*size*/ 276,
      /*result*/ EventResultToString(EventResult::BLOCKED),
      /*username*/ kUserName, /*scan_id*/ absl::nullopt);

  WaitForDownloadToFinish();

  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  EXPECT_EQ(item->GetState(), download::DownloadItem::INTERRUPTED);
}

class AllowlistedUrlDeepScanningBrowserTest
    : public DownloadDeepScanningBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  AllowlistedUrlDeepScanningBrowserTest()
      : DownloadDeepScanningBrowserTestBase(GetParam(), /*is_consumer=*/false) {
  }
  ~AllowlistedUrlDeepScanningBrowserTest() override = default;

  void SetUpOnMainThread() override {
    DownloadDeepScanningBrowserTestBase::SetUpOnMainThread();

    base::Value::List domain_list;
    domain_list.Append(embedded_test_server()->base_url().host_piece());
    browser()->profile()->GetPrefs()->SetList(
        prefs::kSafeBrowsingAllowlistDomains, std::move(domain_list));
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         AllowlistedUrlDeepScanningBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(AllowlistedUrlDeepScanningBrowserTest,
                       AllowlistedUrlStillDoesDlp) {
  // This allows the blocking DM token reads happening on profile-Connector
  // triggers.
  base::ScopedAllowBlockingForTesting allow_blocking;

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
  ExpectContentAnalysisSynchronousResponse(sync_response, {"dlp"});

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest();

  WaitForDownloadToFinish();

  // The file should be blocked for sensitive content.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::
                DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK);
  EXPECT_EQ(item->GetState(), download::DownloadItem::INTERRUPTED);
}

enum class ScanningVerdict { MALWARE, UNWANTED, SAFE };

// This test validates that metadata check verdicts and deep scanning verdicts
// override each other correctly and only report up to 1 event.
class MetadataCheckAndDeepScanningBrowserTest
    : public DownloadDeepScanningBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<ClientDownloadResponse::Verdict, ScanningVerdict, bool>> {
 public:
  MetadataCheckAndDeepScanningBrowserTest()
      : DownloadDeepScanningBrowserTestBase(std::get<2>(GetParam()),
                                            /*is_consumer=*/false) {}

  ClientDownloadResponse::Verdict metadata_check_verdict() const {
    return std::get<0>(GetParam());
  }

  ScanningVerdict scanning_verdict() const { return std::get<1>(GetParam()); }

  enterprise_connectors::ContentAnalysisResponse scanning_response() const {
    enterprise_connectors::ContentAnalysisResponse response;
    response.set_request_token(last_request().request_token());
    auto* result = response.add_results();
    result->set_tag("malware");
    result->set_status(
        enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
    if (scanning_verdict() == ScanningVerdict::MALWARE) {
      auto* rule = result->add_triggered_rules();
      rule->set_action(enterprise_connectors::TriggeredRule::BLOCK);
      rule->set_rule_name("malware");
    } else if (scanning_verdict() == ScanningVerdict::UNWANTED) {
      auto* rule = result->add_triggered_rules();
      rule->set_action(enterprise_connectors::TriggeredRule::WARN);
      rule->set_rule_name("uws");
    }
    return response;
  }

  std::string metadata_check_threat_type() const {
    switch (metadata_check_verdict()) {
      case ClientDownloadResponse::UNKNOWN:
      case ClientDownloadResponse::SAFE:
        return "";
      case ClientDownloadResponse::DANGEROUS:
        return "DANGEROUS";
      case ClientDownloadResponse::UNCOMMON:
        return "UNCOMMON";
      case ClientDownloadResponse::POTENTIALLY_UNWANTED:
        return "POTENTIALLY_UNWANTED";
      case ClientDownloadResponse::DANGEROUS_HOST:
        return "DANGEROUS_HOST";
      case ClientDownloadResponse::DANGEROUS_ACCOUNT_COMPROMISE:
        return "DANGEROUS_ACCOUNT_COMPROMISE";
    }
  }

  std::string expected_threat_type() const {
    // These results exempt the file from being deep scanned.
    if (metadata_check_verdict() == ClientDownloadResponse::DANGEROUS ||
        metadata_check_verdict() == ClientDownloadResponse::DANGEROUS_HOST ||
        metadata_check_verdict() ==
            ClientDownloadResponse::DANGEROUS_ACCOUNT_COMPROMISE) {
      return metadata_check_threat_type();
    }
    switch (scanning_verdict()) {
      case ScanningVerdict::MALWARE:
        return "DANGEROUS";
      case ScanningVerdict::UNWANTED:
        return "POTENTIALLY_UNWANTED";
      case ScanningVerdict::SAFE:
        return metadata_check_threat_type();
    }
  }

  download::DownloadDangerType expected_danger_type() const {
    switch (metadata_check_verdict()) {
      case ClientDownloadResponse::DANGEROUS:
        return download::DownloadDangerType::
            DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT;
      case ClientDownloadResponse::DANGEROUS_HOST:
        return download::DownloadDangerType::
            DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST;
      case ClientDownloadResponse::DANGEROUS_ACCOUNT_COMPROMISE:
        return download::DownloadDangerType::
            DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE;
      case ClientDownloadResponse::UNCOMMON:
        if (scanning_verdict() != ScanningVerdict::MALWARE) {
          return download::DownloadDangerType::
              DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT;
        }
        break;

      case ClientDownloadResponse::POTENTIALLY_UNWANTED:
        if (scanning_verdict() != ScanningVerdict::MALWARE) {
          return download::DownloadDangerType::
              DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED;
        }
        break;
      case ClientDownloadResponse::UNKNOWN:
      case ClientDownloadResponse::SAFE:
        break;
    }

    switch (scanning_verdict()) {
      case ScanningVerdict::MALWARE:
        return download::DownloadDangerType::
            DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT;
      case ScanningVerdict::UNWANTED:
        return download::DownloadDangerType::
            DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED;
      case ScanningVerdict::SAFE:
        return download::DownloadDangerType::
            DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE;
    }
  }

  bool deep_scan_needed() const {
    return metadata_check_verdict() != ClientDownloadResponse::DANGEROUS &&
           metadata_check_verdict() != ClientDownloadResponse::DANGEROUS_HOST &&
           metadata_check_verdict() !=
               ClientDownloadResponse::DANGEROUS_ACCOUNT_COMPROMISE;
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    MetadataCheckAndDeepScanningBrowserTest,
    testing::Combine(
        testing::Values(ClientDownloadResponse::SAFE,
                        ClientDownloadResponse::DANGEROUS,
                        ClientDownloadResponse::UNCOMMON,
                        ClientDownloadResponse::POTENTIALLY_UNWANTED,
                        ClientDownloadResponse::DANGEROUS_HOST,
                        ClientDownloadResponse::UNKNOWN,
                        ClientDownloadResponse::DANGEROUS_ACCOUNT_COMPROMISE),
        testing::Values(ScanningVerdict::MALWARE,
                        ScanningVerdict::UNWANTED,
                        ScanningVerdict::SAFE),
        testing::Bool()));

IN_PROC_BROWSER_TEST_P(MetadataCheckAndDeepScanningBrowserTest, Test) {
  // This allows the blocking DM token reads happening on profile-Connector
  // triggers.
  base::ScopedAllowBlockingForTesting allow_blocking;

  SetUpReporting();
  SetAnalysisConnector(browser()->profile()->GetPrefs(),
                       enterprise_connectors::FILE_DOWNLOADED,
                       R"({
                            "service_provider": "google",
                            "enable": [
                              {
                                "url_list": ["*"],
                                "tags": ["malware"]
                              }
                            ],
                            "block_until_verdict": 1
                          })",
                       connectors_machine_scope());
  base::HistogramTester histograms;

  // Set up the metadata response.
  ClientDownloadResponse metadata_response;
  metadata_response.set_verdict(metadata_check_verdict());
  ExpectMetadataResponse(metadata_response);

  // Nothing is returned synchronously.
  if (deep_scan_needed()) {
    enterprise_connectors::ContentAnalysisResponse sync_response;
    sync_response.set_request_token(last_request().request_token());
    ExpectContentAnalysisSynchronousResponse(sync_response, {"malware"});
  }

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForMetadataCheck();
  if (deep_scan_needed())
    WaitForDeepScanRequest();

  // Both the DLP and malware violations generate an event.
  std::set<std::string> zip_types = {"application/zip",
                                     "application/x-zip-compressed"};
  EventReportValidator validator(client());
  std::string threat_type = expected_threat_type();
  if (threat_type.empty()) {
    validator.ExpectNoReport();
  } else {
    // A scan ID is only expected when a deep scan is performed and when its
    // result will be reported over the metadata check one.
    auto scan_id =
        deep_scan_needed() && scanning_verdict() != ScanningVerdict::SAFE
            ? absl::optional<std::string>(last_request().request_token())
            : absl::nullopt;

    validator.ExpectDangerousDeepScanningResult(
        /*url*/ url.spec(),
        /*source*/ absl::nullopt,
        /*destination*/ absl::nullopt,
        /*filename*/ "zipfile_two_archives.zip",
        // sha256sum chrome/test/data/safe_browsing/download_protection/\
        // zipfile_two_archives.zip |  tr '[:lower:]' '[:upper:]'
        /*sha*/
        "339C8FFDAE735C4F1846D0E6FF07FBD85CAEE6D96045AAEF5B30F3220836643C",
        /*threat_type*/ threat_type,
        /*trigger*/
        extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
        /*mimetypes*/ &zip_types,
        /*size*/ 276,
        /*result*/ EventResultToString(EventResult::WARNED),
        /*username*/ kUserName,
        /*scan_id*/ scan_id);
  }

  // The deep scanning malware verdict is returned asynchronously. It is not
  // done if the previous verdict is DANGEROUS or DANGEROUS_HOST.
  if (deep_scan_needed()) {
    SendFcmMessage(scanning_response());
  } else {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    validator.SetDoneClosure(run_loop.QuitClosure());
    run_loop.Run();
  }

  // The file should be blocked.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(item->GetDangerType(), expected_danger_type());
  EXPECT_EQ(item->GetState(), download::DownloadItem::IN_PROGRESS);

  if (metadata_check_verdict() == ClientDownloadResponse::UNCOMMON) {
    // UNCOMMON is not a verdict that's considered malicious, so the download
    // will not allow Chrome to close before being accepted or cancelled first
    // (see DownloadManagerImpl::NonMaliciousInProgressCount). This makes the
    // test crash after it runs as some callbacks are left unresolved, so a
    // "cancel" is simulated.
    item->SimulateErrorForTesting(
        download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);
  }

  if (threat_type.empty()) {
    // Safe verdicts on both SB and deep scanning tests need to wait for the
    // download to complete so they don't crash after being done.
    WaitForDownloadToFinish();
    EXPECT_EQ(item->GetDangerType(), expected_danger_type());
    EXPECT_EQ(item->GetState(), download::DownloadItem::COMPLETE);
  }

  // UMAs for this request should only be recorded once, and only if the malware
  // deep scan takes place.
  int samples = deep_scan_needed() ? 1 : 0;
  histograms.ExpectUniqueSample("SafeBrowsingBinaryUploadRequest.Result",
                                BinaryUploadService::Result::SUCCESS, samples);
  histograms.ExpectUniqueSample("SafeBrowsingBinaryUploadRequest.MalwareResult",
                                true, samples);
}

class WaitForModalObserver : public DeepScanningRequest::Observer {
 public:
  explicit WaitForModalObserver(DeepScanningRequest* request)
      : request_(request),
        run_loop_(base::RunLoop::Type::kNestableTasksAllowed) {
    request_->AddObserver(this);
  }

  ~WaitForModalObserver() override {
    if (request_)
      request_->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  void OnModalShown(DeepScanningRequest* request) override { run_loop_.Quit(); }
  void OnFinish(DeepScanningRequest* request) override {
    request_->RemoveObserver(this);
    request_ = nullptr;
  }

 private:
  raw_ptr<DeepScanningRequest> request_;
  base::RunLoop run_loop_;
};

class WaitForFinishObserver : public DeepScanningRequest::Observer {
 public:
  explicit WaitForFinishObserver(DeepScanningRequest* request)
      : request_(request),
        run_loop_(base::RunLoop::Type::kNestableTasksAllowed) {
    request_->AddObserver(this);
  }

  ~WaitForFinishObserver() override {
    if (request_)
      request_->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  void OnFinish(DeepScanningRequest* request) override {
    run_loop_.Quit();
    request_->RemoveObserver(this);
    request_ = nullptr;
  }

 private:
  raw_ptr<DeepScanningRequest> request_;
  base::RunLoop run_loop_;
};

IN_PROC_BROWSER_TEST_F(ConsumerDeepScanningBrowserTest,
                       RetriableErrorShowsModal) {
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  ClientDownloadResponse metadata_response;
  metadata_response.set_request_deep_scan(true);
  ExpectMetadataResponse(metadata_response);

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Wait for download to show the prompt
  {
    content::DownloadManager* download_manager =
        browser()->profile()->GetDownloadManager();
    content::DownloadTestObserverTerminal observer(
        download_manager, 1,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_QUIT);
    observer.WaitForFinished();
  }

  // Trigger upload, bypassing the prompt
  {
    ASSERT_EQ(download_items().size(), 1u);
    DownloadItemModel model(*download_items().begin());
    DownloadCommands(model.GetWeakPtr())
        .ExecuteCommand(DownloadCommands::DEEP_SCAN);
  }

  ExpectContentAnalysisUploadFailure(net::HTTP_INTERNAL_SERVER_ERROR, {});

  // Wait for the modal dialog
  {
    auto deep_scan_requests = test_sb_factory()
                                  ->test_safe_browsing_service()
                                  ->download_protection_service()
                                  ->GetDeepScanningRequests();
    ASSERT_EQ(deep_scan_requests.size(), 1u);
    WaitForModalObserver observer(*deep_scan_requests.begin());
    observer.Wait();
  }

  web_modal::WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  // If the dialog remains open, the download remains in progress. This blocks
  // shutdown, so close the modal.
  web_contents_modal_dialog_manager->CloseAllDialogs();

  WaitForDownloadToFinish();

  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* download = *download_items().begin();
  EXPECT_EQ(download->GetState(), download::DownloadItem::COMPLETE);
  EXPECT_EQ(download->GetDangerType(),
            download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
}

IN_PROC_BROWSER_TEST_F(ConsumerDeepScanningBrowserTest,
                       NonretriableErrorDoesNotShowModal) {
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  ClientDownloadResponse metadata_response;
  metadata_response.set_request_deep_scan(true);
  ExpectMetadataResponse(metadata_response);

  // An encrypted file will always be encrypted, and therefore not suitable for
  // deep scanning.
  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/encrypted.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Wait for download to show the prompt
  {
    content::DownloadManager* download_manager =
        browser()->profile()->GetDownloadManager();
    content::DownloadTestObserverTerminal observer(
        download_manager, 1,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_QUIT);
    observer.WaitForFinished();
  }

  // Trigger upload, bypassing the prompt
  {
    ASSERT_EQ(download_items().size(), 1u);
    DownloadItemModel model(*download_items().begin());
    DownloadCommands(model.GetWeakPtr())
        .ExecuteCommand(DownloadCommands::DEEP_SCAN);
  }

  // Wait for deep scanning to finish
  {
    auto deep_scan_requests = test_sb_factory()
                                  ->test_safe_browsing_service()
                                  ->download_protection_service()
                                  ->GetDeepScanningRequests();
    ASSERT_EQ(deep_scan_requests.size(), 1u);
    WaitForFinishObserver observer(*deep_scan_requests.begin());
    observer.Wait();
  }

  web_modal::WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());

  WaitForDownloadToFinish();

  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* download = *download_items().begin();
  EXPECT_EQ(download->GetState(), download::DownloadItem::COMPLETE);
  EXPECT_EQ(download->GetDangerType(),
            download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
}

class SavePackageDeepScanningBrowserTest
    : public DownloadDeepScanningBrowserTestBase {
 public:
  SavePackageDeepScanningBrowserTest()
      : DownloadDeepScanningBrowserTestBase(/*connectors_machine_scope=*/true,
                                            /*is_consumer=*/false) {}

  base::FilePath GetSaveDir() {
    return DownloadPrefs(browser()->profile()).DownloadPath();
  }

  base::FilePath GetTestFilePath() {
    return GetTestDataDirectory().AppendASCII("save_page/text.txt");
  }
};

IN_PROC_BROWSER_TEST_F(SavePackageDeepScanningBrowserTest, Allowed) {
  SetUpReporting();

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/save_page/text.txt")));

  // No scan runs synchronously.
  ExpectContentAnalysisSynchronousResponse(
      enterprise_connectors::ContentAnalysisResponse(), {"dlp"});

  base::RunLoop run_loop;
  content::SavePackageFinishedObserver observer(
      browser()->profile()->GetDownloadManager(), run_loop.QuitClosure());
  base::FilePath main_file = GetSaveDir().AppendASCII("text.htm");
  base::FilePath extra_files_dir = GetSaveDir().AppendASCII("text_files");
  ASSERT_TRUE(browser()->tab_strip_model()->GetActiveWebContents()->SavePage(
      main_file, extra_files_dir, content::SAVE_PAGE_TYPE_AS_ONLY_HTML));
  WaitForDeepScanRequest();

  // The async scanning response indicates the file has no sensitive data.
  enterprise_connectors::ContentAnalysisResponse response;
  auto* result = response.add_results();
  response.set_request_token(last_request().request_token());
  result->set_tag("dlp");
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);

  // That response should not trigger a security event.
  EventReportValidator validator(client());
  validator.ExpectNoReport();

  SendFcmMessage(response);
  run_loop.Run();

  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(
      item->GetDangerType(),
      download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_SAFE);
  EXPECT_EQ(item->GetState(), download::DownloadItem::COMPLETE);

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(main_file));
  EXPECT_TRUE(base::ContentsEqual(GetTestFilePath(), main_file));
  EXPECT_FALSE(base::PathExists(extra_files_dir));
}

IN_PROC_BROWSER_TEST_F(SavePackageDeepScanningBrowserTest, Blocked) {
  SetUpReporting();

  GURL url = embedded_test_server()->GetURL("/save_page/text.txt");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // No scan runs synchronously.
  ExpectContentAnalysisSynchronousResponse(
      enterprise_connectors::ContentAnalysisResponse(), {"dlp"});

  base::RunLoop run_loop;
  content::SavePackageFinishedObserver observer(
      browser()->profile()->GetDownloadManager(), run_loop.QuitClosure(),
      {download::DownloadItem::INTERRUPTED});
  base::FilePath main_file = GetSaveDir().AppendASCII("text.htm");
  base::FilePath extra_files_dir = GetSaveDir().AppendASCII("text_files");
  ASSERT_TRUE(browser()->tab_strip_model()->GetActiveWebContents()->SavePage(
      main_file, extra_files_dir, content::SAVE_PAGE_TYPE_AS_ONLY_HTML));
  WaitForDeepScanRequest();

  // The async scanning response indicates the file should be blocked.
  enterprise_connectors::ContentAnalysisResponse response;
  auto* result = response.add_results();
  response.set_request_token(last_request().request_token());
  result->set_tag("dlp");
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* dlp_verdict = result->add_triggered_rules();
  dlp_verdict->set_action(enterprise_connectors::TriggeredRule::BLOCK);

  // That blocking response should trigger a security event.
  EventReportValidator validator(client());
  std::set<std::string> mimetypes = {"text/plain"};
  validator.ExpectSensitiveDataEvent(
      /*url*/ url.spec(),
      /*source*/ absl::nullopt,
      /*destination*/ absl::nullopt,
      /*filename*/ "text.htm",
      // sha256sum chrome/test/data/save_page/text.txt | tr a-f A-F
      "9789A2E12D50EFA4B891D4EF95C5189FA4C98E34C84E1F8017CD8F574CA035DD",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
      /*dlp_verdict*/ *result,
      /*mimetypes*/ &mimetypes,
      /*size*/ 54,
      /*result*/ EventResultToString(EventResult::BLOCKED),
      /*username*/ kUserName,
      /*scan_id*/ last_request().request_token());

  SendFcmMessage(response);
  run_loop.Run();

  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::
                DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK);
  EXPECT_EQ(item->GetState(), download::DownloadItem::INTERRUPTED);

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(main_file));
  EXPECT_FALSE(base::PathExists(extra_files_dir));
}

IN_PROC_BROWSER_TEST_F(SavePackageDeepScanningBrowserTest, KeepAfterWarning) {
  SetUpReporting();

  GURL url = embedded_test_server()->GetURL("/save_page/text.txt");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // No scan runs synchronously.
  ExpectContentAnalysisSynchronousResponse(
      enterprise_connectors::ContentAnalysisResponse(), {"dlp"});

  base::RunLoop save_package_run_loop;
  content::SavePackageFinishedObserver observer(
      browser()->profile()->GetDownloadManager(),
      save_package_run_loop.QuitClosure(), {download::DownloadItem::COMPLETE});
  base::FilePath main_file = GetSaveDir().AppendASCII("text.htm");
  base::FilePath extra_files_dir = GetSaveDir().AppendASCII("text_files");
  ASSERT_TRUE(browser()->tab_strip_model()->GetActiveWebContents()->SavePage(
      main_file, extra_files_dir, content::SAVE_PAGE_TYPE_AS_ONLY_HTML));
  WaitForDeepScanRequest();

  // The async scanning response indicates the file should warn the user.
  enterprise_connectors::ContentAnalysisResponse response;
  auto* result = response.add_results();
  response.set_request_token(last_request().request_token());
  result->set_tag("dlp");
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* dlp_verdict = result->add_triggered_rules();
  dlp_verdict->set_action(enterprise_connectors::TriggeredRule::WARN);

  // That warning response should trigger a security event.
  base::RunLoop validator_run_loop;
  EventReportValidator validator(client());
  validator.SetDoneClosure(validator_run_loop.QuitClosure());
  std::set<std::string> mimetypes = {"text/plain"};
  validator.ExpectSensitiveDataEvent(
      /*url*/ url.spec(),
      /*source*/ absl::nullopt,
      /*destination*/ absl::nullopt,
      /*filename*/ "text.htm",
      // sha256sum chrome/test/data/save_page/text.txt | tr a-f A-F
      "9789A2E12D50EFA4B891D4EF95C5189FA4C98E34C84E1F8017CD8F574CA035DD",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
      /*dlp_verdict*/ *result,
      /*mimetypes*/ &mimetypes,
      /*size*/ 54,
      /*result*/ EventResultToString(EventResult::WARNED),
      /*username*/ kUserName,
      /*scan_id*/ last_request().request_token());

  SendFcmMessage(response);
  validator_run_loop.Run();

  // The warning has been received but neither "keep" or "discard" has been
  // chosen at this point, so the download isn't complete or interrupted and the
  // file on disk is still not it its final location.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::
                DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING);
  EXPECT_EQ(item->GetState(), download::DownloadItem::IN_PROGRESS);

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(main_file));
  EXPECT_FALSE(base::PathExists(extra_files_dir));

  // Keeping the save package will generate a second warning event, complete the
  // download and move the file to its final destination.
  validator.ExpectSensitiveDataEvent(
      /*url*/ url.spec(),
      /*source*/ absl::nullopt,
      /*destination*/ absl::nullopt,
      /*filename*/ "text.htm",
      // sha256sum chrome/test/data/save_page/text.txt | tr a-f A-F
      "9789A2E12D50EFA4B891D4EF95C5189FA4C98E34C84E1F8017CD8F574CA035DD",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
      /*dlp_verdict*/ *result,
      /*mimetypes*/ &mimetypes,
      /*size*/ 54,
      /*result*/ EventResultToString(EventResult::BYPASSED),
      /*username*/ kUserName,
      /*scan_id*/ last_request().request_token());

  DownloadItemModel model(item);
  DownloadCommands(model.GetWeakPtr()).ExecuteCommand(DownloadCommands::KEEP);
  save_package_run_loop.Run();

  ASSERT_EQ(download_items().size(), 1u);
  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_USER_VALIDATED);
  EXPECT_EQ(item->GetState(), download::DownloadItem::COMPLETE);

  EXPECT_TRUE(base::PathExists(main_file));
  EXPECT_TRUE(base::ContentsEqual(GetTestFilePath(), main_file));
  EXPECT_FALSE(base::PathExists(extra_files_dir));
}

IN_PROC_BROWSER_TEST_F(SavePackageDeepScanningBrowserTest,
                       DiscardAfterWarning) {
  SetUpReporting();

  GURL url = embedded_test_server()->GetURL("/save_page/text.txt");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // No scan runs synchronously.
  ExpectContentAnalysisSynchronousResponse(
      enterprise_connectors::ContentAnalysisResponse(), {"dlp"});

  base::RunLoop save_package_run_loop;
  content::SavePackageFinishedObserver observer(
      browser()->profile()->GetDownloadManager(),
      save_package_run_loop.QuitClosure(), {download::DownloadItem::CANCELLED});
  base::FilePath main_file = GetSaveDir().AppendASCII("text.htm");
  base::FilePath extra_files_dir = GetSaveDir().AppendASCII("text_files");
  ASSERT_TRUE(browser()->tab_strip_model()->GetActiveWebContents()->SavePage(
      main_file, extra_files_dir, content::SAVE_PAGE_TYPE_AS_ONLY_HTML));
  WaitForDeepScanRequest();

  // The async scanning response indicates the file should warn the user.
  enterprise_connectors::ContentAnalysisResponse response;
  auto* result = response.add_results();
  response.set_request_token(last_request().request_token());
  result->set_tag("dlp");
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* dlp_verdict = result->add_triggered_rules();
  dlp_verdict->set_action(enterprise_connectors::TriggeredRule::WARN);

  // That warning response should trigger a security event.
  base::RunLoop validator_run_loop;
  EventReportValidator validator(client());
  validator.SetDoneClosure(validator_run_loop.QuitClosure());
  std::set<std::string> mimetypes = {"text/plain"};
  validator.ExpectSensitiveDataEvent(
      /*url*/ url.spec(),
      /*source*/ absl::nullopt,
      /*destination*/ absl::nullopt,
      /*filename*/ "text.htm",
      // sha256sum chrome/test/data/save_page/text.txt | tr a-f A-F
      "9789A2E12D50EFA4B891D4EF95C5189FA4C98E34C84E1F8017CD8F574CA035DD",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
      /*dlp_verdict*/ *result,
      /*mimetypes*/ &mimetypes,
      /*size*/ 54,
      /*result*/ EventResultToString(EventResult::WARNED),
      /*username*/ kUserName,
      /*scan_id*/ last_request().request_token());

  SendFcmMessage(response);
  validator_run_loop.Run();

  // The warning has been received but neither "keep" or "discard" has been
  // chosen at this point, so the download isn't complete or interrupted and the
  // file on disk is still not it its final location.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::
                DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING);
  EXPECT_EQ(item->GetState(), download::DownloadItem::IN_PROGRESS);

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(main_file));
  EXPECT_FALSE(base::PathExists(extra_files_dir));

  // Discarding the download will remove it from disk and from download_items().
  DownloadItemModel model(item);
  DownloadCommands(model.GetWeakPtr())
      .ExecuteCommand(DownloadCommands::DISCARD);
  save_package_run_loop.Run();

  ASSERT_TRUE(download_items().empty());
  EXPECT_FALSE(base::PathExists(main_file));
  EXPECT_FALSE(base::PathExists(extra_files_dir));
}

IN_PROC_BROWSER_TEST_F(SavePackageDeepScanningBrowserTest, OpenNow) {
  SetUpReporting();

  GURL url = embedded_test_server()->GetURL("/save_page/text.txt");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // No scan runs synchronously.
  ExpectContentAnalysisSynchronousResponse(
      enterprise_connectors::ContentAnalysisResponse(), {"dlp"});

  base::RunLoop save_package_run_loop;
  content::SavePackageFinishedObserver observer(
      browser()->profile()->GetDownloadManager(),
      save_package_run_loop.QuitClosure(), {download::DownloadItem::COMPLETE});
  base::FilePath main_file = GetSaveDir().AppendASCII("text.htm");
  base::FilePath extra_files_dir = GetSaveDir().AppendASCII("text_files");
  ASSERT_TRUE(browser()->tab_strip_model()->GetActiveWebContents()->SavePage(
      main_file, extra_files_dir, content::SAVE_PAGE_TYPE_AS_ONLY_HTML));
  WaitForDeepScanRequest();

  // Opening the save package before the async response is obtained will
  // generate a warning event once it does come back, complete the
  // download and move the file to its final destination.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  DownloadItemModel model(item);
  model.CompleteSafeBrowsingScan();
  save_package_run_loop.Run();

  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_USER_VALIDATED);
  EXPECT_EQ(item->GetState(), download::DownloadItem::COMPLETE);

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(main_file));
  EXPECT_TRUE(base::ContentsEqual(GetTestFilePath(), main_file));
  EXPECT_FALSE(base::PathExists(extra_files_dir));

  // After the verdict is obtained, send the FCM message to confirm the
  // appropriate event is reported.
  enterprise_connectors::ContentAnalysisResponse response;
  auto* result = response.add_results();
  response.set_request_token(last_request().request_token());
  result->set_tag("dlp");
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* dlp_verdict = result->add_triggered_rules();
  dlp_verdict->set_action(enterprise_connectors::TriggeredRule::BLOCK);

  base::RunLoop validator_run_loop;
  EventReportValidator validator(client());
  validator.SetDoneClosure(validator_run_loop.QuitClosure());
  std::set<std::string> mimetypes = {"text/plain"};
  validator.ExpectSensitiveDataEvent(
      /*url*/ url.spec(),
      /*source*/ absl::nullopt,
      /*destination*/ absl::nullopt,
      /*filename*/ "text.htm",
      // sha256sum chrome/test/data/save_page/text.txt | tr a-f A-F
      "9789A2E12D50EFA4B891D4EF95C5189FA4C98E34C84E1F8017CD8F574CA035DD",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
      /*dlp_verdict*/ *result,
      /*mimetypes*/ &mimetypes,
      /*size*/ 54,
      /*result*/ EventResultToString(EventResult::BLOCKED),
      /*username*/ kUserName,
      /*scan_id*/ last_request().request_token());

  SendFcmMessage(response);
  validator_run_loop.Run();
}

}  // namespace safe_browsing
