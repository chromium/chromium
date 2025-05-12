// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_browsertest_base.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service_factory.h"
#include "chrome/browser/safe_browsing/download_protection/deep_scanning_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/ppapi_download_request.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_test_utils.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_features.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/obfuscation/core/utils.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/common/file_type_policies_test_util.h"
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
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/network/test/test_utils.h"

namespace safe_browsing {

namespace {

constexpr char kUserName[] = "test@chromium.org";

constexpr char kResumableUploadUrl[] =
    "http://uploads.google.com?upload_id=ABC&upload_protocol=resumable";

constexpr int64_t kSingleChunkObfuscationOverhead =
    enterprise_obfuscation::kKeySize + enterprise_obfuscation::kNonceSize +
    enterprise_obfuscation::kAuthTagSize;

constexpr char kTestContent[] = "test content";

// Extract the metadata proto from the raw request string based on multipart
// upload protocol. Returns true on success.
bool GetMultipartUploadMetadata(
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

// Extract the metadata proto from the raw request string based on resumable
// upload protocol. Returns true on success.
bool GetResumableUploadMetadata(
    const std::string& upload_request,
    enterprise_connectors::ContentAnalysisRequest* out_proto) {
  // The request content is of the following format, see resumable_uploader.h
  // for details: "metadata\r\n"
  size_t boundary_end = upload_request.find("\r\n");
  std::string encoded_metadata = upload_request.substr(0, boundary_end);

  std::string serialized_metadata;
  base::Base64Decode(encoded_metadata, &serialized_metadata);

  return out_proto->ParseFromString(serialized_metadata);
}
}  // namespace

// Integration tests for download deep scanning behavior, only mocking network
// traffic.
class DownloadDeepScanningBrowserTestBase
    : public enterprise_connectors::test::DeepScanningBrowserTestBase,
      public content::DownloadManager::Observer,
      public download::DownloadItem::Observer {
 public:
  // |connectors_machine_scope| indicates whether the Connector prefs such as
  // OnFileDownloadedEnterpriseConnector and OnSecurityEventEnterpriseConnector
  // should be set at the machine or user scope.
  // |is_consumer| indicates whether the content scan is a consumer or an
  // enterprise scan.
  // |is_obfuscated| indicates whether the downloaded file has been obfuscated
  // to prevent user access. Currently, this is done while waiting for an
  // enterprise deep scan verdict.
  explicit DownloadDeepScanningBrowserTestBase(bool connectors_machine_scope,
                                               bool is_consumer,
                                               bool is_obfuscated)
      : is_consumer_(is_consumer),
        is_obfuscated_(is_obfuscated),
        connectors_machine_scope_(connectors_machine_scope) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    is_obfuscated_ ? enabled_features.push_back(
                         enterprise_obfuscation::kEnterpriseFileObfuscation)
                   : disabled_features.push_back(
                         enterprise_obfuscation::kEnterpriseFileObfuscation);

    scoped_feature_list_.InitWithFeatures(std::move(enabled_features),
                                          std::move(disabled_features));
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
    enterprise_connectors::test::SetOnSecurityEventReporting(
        browser()->profile()->GetPrefs(),
        /*enabled*/ true, /*enabled_event_names*/ {},
        /*enabled_opt_in_events*/ {}, connectors_machine_scope());
    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_->SetDMToken("dm_token");

#if BUILDFLAG(IS_CHROMEOS)
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
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
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

#if BUILDFLAG(IS_CHROMEOS)
      SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));
#else
      if (connectors_machine_scope()) {
        SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));
      } else {
        enterprise_connectors::test::SetProfileDMToken(browser()->profile(),
                                                       "dm_token");
      }
#endif
      enterprise_connectors::test::SetAnalysisConnector(
          browser()->profile()->GetPrefs(),
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
        ->AddResponse(test_sb_factory_->test_safe_browsing_service()
                          ->download_protection_service()
                          ->GetDownloadRequestUrl()
                          .spec(),
                      response.SerializeAsString());
  }

  void PrepareConnectorUrl(const std::vector<std::string>& tags) {
    if (is_consumer_) {
      connector_url_ =
          "https://safebrowsing.google.com/safebrowsing/uploads/consumer";
    } else {
      connector_url_ =
          "https://safebrowsing.google.com/safebrowsing/uploads/"
          "scan?device_token=dm_token&connector=OnFileDownloaded";
      for (const std::string& tag : tags) {
        connector_url_ += ("&tag=" + tag);
      }
    }
  }

  void ExpectContentAnalysisResumableMetadataResponse(
      const std::vector<std::string>& tags) {
    PrepareConnectorUrl(tags);

    auto metadata_response_head = network::CreateURLResponseHead(net::HTTP_OK);
    metadata_response_head->headers->AddHeader("X-Goog-Upload-Status",
                                               "active");
    metadata_response_head->headers->AddHeader("X-Goog-Upload-URL",
                                               kResumableUploadUrl);
    test_sb_factory_->test_safe_browsing_service()
        ->GetTestUrlLoaderFactory()
        ->AddResponse(GURL(connector_url_), std::move(metadata_response_head),
                      "metadata_response",
                      network::URLLoaderCompletionStatus(net::OK));
  }

  void ExpectContentAnalysisResumableContentResponse(
      const enterprise_connectors::ContentAnalysisResponse& response) {
    auto content_response_head = network::CreateURLResponseHead(net::HTTP_OK);
    content_response_head->headers->AddHeader("X-Goog-Upload-Status", "final");

    test_sb_factory_->test_safe_browsing_service()
        ->GetTestUrlLoaderFactory()
        ->AddResponse(GURL(kResumableUploadUrl),
                      std::move(content_response_head),
                      response.SerializeAsString(),
                      network::URLLoaderCompletionStatus(net::OK));
  }

  void ExpectContentAnalysisMultipartResponse(
      const enterprise_connectors::ContentAnalysisResponse& response,
      const std::vector<std::string>& tags) {
    PrepareConnectorUrl(tags);

    test_sb_factory_->test_safe_browsing_service()
        ->GetTestUrlLoaderFactory()
        ->AddResponse(connector_url_, response.SerializeAsString());
  }

  void ExpectContentAnalysisUploadFailure(
      net::HttpStatusCode status_code,
      const std::vector<std::string>& tags) {
    PrepareConnectorUrl(tags);

    test_sb_factory_->test_safe_browsing_service()
        ->GetTestUrlLoaderFactory()
        ->AddResponse(connector_url_, "", status_code);
  }

  base::FilePath GetTestDataDirectory() {
    base::FilePath test_file_directory;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_file_directory);
    return test_file_directory;
  }

  TestSafeBrowsingServiceFactory* test_sb_factory() {
    return test_sb_factory_.get();
  }

  const enterprise_connectors::ContentAnalysisRequest& last_request() const {
    return last_request_;
  }

  const base::flat_set<raw_ptr<download::DownloadItem, CtnExperimental>>&
  download_items() {
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

  void AuthorizeForDeepScanning() {
    static_cast<safe_browsing::CloudBinaryUploadService*>(
        CloudBinaryUploadServiceFactory::GetForProfile(browser()->profile()))
        ->SetAuthForTesting(
            "dm_token",
            /*auth_check_result=*/BinaryUploadService::Result::SUCCESS);
  }

  bool connectors_machine_scope() const { return connectors_machine_scope_; }

  bool is_obfuscated() const { return is_obfuscated_; }

  std::string GetProfileIdentifier() const {
#if BUILDFLAG(IS_CHROMEOS)
    return browser()->profile()->GetPath().AsUTF8Unsafe();
#else
    if (connectors_machine_scope_) {
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
  std::unique_ptr<KeyedService> CreateBinaryUploadService(
      content::BrowserContext* browser_context) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    return std::make_unique<safe_browsing::CloudBinaryUploadService>(
        g_browser_process->safe_browsing_service()->GetURLLoaderFactory(
            profile),
        profile);
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
      std::string buffer(1024, '\0');
      size_t actually_read_bytes = 0;
      MojoResult result = data_pipe_consumer->ReadData(
          MOJO_READ_DATA_FLAG_NONE, base::as_writable_byte_span(buffer),
          actually_read_bytes);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        base::RunLoop().RunUntilIdle();
        continue;
      }
      if (result != MOJO_RESULT_OK) {
        break;
      }
      body.append(std::string_view(buffer).substr(0, actually_read_bytes));
    }

    return body;
  }

  bool MaybeHandleDownloadRequest(const network::ResourceRequest& request) {
    if (request.url != test_sb_factory_->test_safe_browsing_service()
                           ->download_protection_service()
                           ->GetDownloadRequestUrl()) {
      return false;
    }
    if (waiting_for_metadata_closure_) {
      std::move(waiting_for_metadata_closure_).Run();
    }
    return true;
  }

  void HandleConsumerRequest(const network::ResourceRequest& request) {
    if (request.url == safe_browsing::CloudBinaryUploadService::GetUploadUrl(
                           /*is_consumer_scan_eligible=*/true)) {
      ASSERT_TRUE(GetMultipartUploadMetadata(GetDataPipeUploadData(request),
                                             &last_request_));
      if (waiting_for_upload_closure_) {
        std::move(waiting_for_upload_closure_).Run();
      }
    }
  }

  void HandleEnterpriseRequest(const network::ResourceRequest& request) {
    if (request.url == GURL(kResumableUploadUrl)) {
      GetDataPipeUploadData(request);
      if (waiting_for_upload_closure_) {
        std::move(waiting_for_upload_closure_).Run();
      }
      return;
    }

    if (request.url == safe_browsing::CloudBinaryUploadService::GetUploadUrl(
                           /*is_consumer_scan_eligible=*/false)) {
      ASSERT_TRUE(GetMultipartUploadMetadata(GetDataPipeUploadData(request),
                                             &last_request_));
      if (waiting_for_upload_closure_) {
        std::move(waiting_for_upload_closure_).Run();
      }
      return;
    }

    if (request.url == connector_url_) {
      ASSERT_TRUE(GetResumableUploadMetadata(network::GetUploadData(request),
                                             &last_request_));
    }
  }

  void InterceptRequest(const network::ResourceRequest& request) {
    if (MaybeHandleDownloadRequest(request)) {
      return;
    }
    if (is_consumer_) {
      HandleConsumerRequest(request);
      return;
    }
    HandleEnterpriseRequest(request);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  bool is_consumer_;
  bool is_obfuscated_;

  std::unique_ptr<TestSafeBrowsingServiceFactory> test_sb_factory_;

  enterprise_connectors::ContentAnalysisRequest last_request_;

  std::string connector_url_;

  base::OnceClosure waiting_for_upload_closure_;
  base::OnceClosure waiting_for_metadata_closure_;

  base::flat_set<raw_ptr<download::DownloadItem, CtnExperimental>>
      download_items_;

  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_environment_;

  bool connectors_machine_scope_;

  mojo::Remote<network::mojom::DataPipeGetter> data_pipe_getter_;
};

class ConsumerDeepScanningBrowserTest
    : public DownloadDeepScanningBrowserTestBase {
 public:
  ConsumerDeepScanningBrowserTest()
      : DownloadDeepScanningBrowserTestBase(/*connectors_machine_scope=*/true,
                                            /*is_consumer=*/true,
                                            /*is_obfuscated=*/false) {}
};

IN_PROC_BROWSER_TEST_F(ConsumerDeepScanningBrowserTest, ErrorIndicatesFailure) {
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

  ExpectContentAnalysisUploadFailure(net::HTTP_INTERNAL_SERVER_ERROR, {});

  WaitForDownloadToFinish();

  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* download = *download_items().begin();
  EXPECT_EQ(download->GetState(), download::DownloadItem::COMPLETE);
  EXPECT_EQ(
      download->GetDangerType(),
      download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED);
}

class DownloadDeepScanningBrowserTest
    : public DownloadDeepScanningBrowserTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  DownloadDeepScanningBrowserTest()
      : DownloadDeepScanningBrowserTestBase(
            /*connectors_machine_scope=*/std::get<0>(GetParam()),
            /*is_consumer=*/false,
            /*is_obfuscated=*/std::get<1>(GetParam())) {}
};

INSTANTIATE_TEST_SUITE_P(,
                         DownloadDeepScanningBrowserTest,
                         testing::Combine(
                             /*connectors_machine_scope=*/testing::Bool(),
                             /*is_obfuscated=*/testing::Bool()));

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

  // The malware scan finishes synchronously, and doesn't find anything.
  auto* malware_result = sync_response.add_results();
  malware_result->set_tag("malware");
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);

  ExpectContentAnalysisResumableMetadataResponse({"dlp", "malware"});
  ExpectContentAnalysisResumableContentResponse(sync_response);

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest();

  EXPECT_EQ(last_request().reason(),
            enterprise_connectors::ContentAnalysisRequest::NORMAL_DOWNLOAD);

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

  // The malware scan finishes synchronously, and fails.
  auto* malware_result = sync_response.add_results();
  malware_result->set_tag("malware");
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::FAILURE);

  ExpectContentAnalysisResumableMetadataResponse({"dlp", "malware"});
  ExpectContentAnalysisResumableContentResponse(sync_response);

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest();

  EXPECT_EQ(last_request().reason(),
            enterprise_connectors::ContentAnalysisRequest::NORMAL_DOWNLOAD);

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

  // The malware scan finishes synchronously, and finds malware.
  auto* malware_result = sync_response.add_results();
  malware_result->set_tag("malware");
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* malware_rule = malware_result->add_triggered_rules();
  malware_rule->set_action(enterprise_connectors::TriggeredRule::BLOCK);
  malware_rule->set_rule_name("malware");

  ExpectContentAnalysisResumableMetadataResponse({"dlp", "malware"});
  ExpectContentAnalysisResumableContentResponse(sync_response);

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest();

  EXPECT_EQ(last_request().reason(),
            enterprise_connectors::ContentAnalysisRequest::NORMAL_DOWNLOAD);

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

  // The malware scan finishes synchronously, and fails.
  auto* malware_result = sync_response.add_results();
  malware_result->set_tag("malware");
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::FAILURE);

  ExpectContentAnalysisResumableMetadataResponse({"dlp", "malware"});
  ExpectContentAnalysisResumableContentResponse(sync_response);

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  WaitForDeepScanRequest();

  EXPECT_EQ(last_request().reason(),
            enterprise_connectors::ContentAnalysisRequest::NORMAL_DOWNLOAD);

  WaitForDownloadToFinish();

  // The file should be blocked for sensitive content.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::
                DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK);
  EXPECT_EQ(item->GetState(), download::DownloadItem::INTERRUPTED);
}

// Regardless of resumable or multipart upload protocol, when a file is password
// protected and the `block_password_protected` setting is on, the file should
// be blocked.
IN_PROC_BROWSER_TEST_P(DownloadDeepScanningBrowserTest,
                       PasswordProtectedTxtFilesAreBlocked) {
  // TODO(crbug.com/378490429): Add support for obfuscated password protected
  // files.
  if (is_obfuscated()) {
    GTEST_SKIP() << "Encryption status cannot be detected in obfuscated files.";
  }
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

// TODO(crbug.com/414822762): Fix flaky test.
IN_PROC_BROWSER_TEST_P(DownloadDeepScanningBrowserTest,
                       DISABLED_DlpAndMalwareViolations) {
  // This allows the blocking DM token reads happening on profile-Connector
  // triggers.
  base::ScopedAllowBlockingForTesting allow_blocking;

  SetUpReporting();
  base::HistogramTester histograms;

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // The DLP scan finishes synchronously and find a violation. Malware scanning
  // finds violation.
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
  auto* malware_rule = malware_result->add_triggered_rules();
  malware_rule->set_action(enterprise_connectors::TriggeredRule::WARN);
  malware_rule->set_rule_name("uws");

  ExpectContentAnalysisResumableMetadataResponse({"dlp", "malware"});
  ExpectContentAnalysisResumableContentResponse(sync_response);

  WaitForDeepScanRequest();

  EXPECT_EQ(last_request().reason(),
            enterprise_connectors::ContentAnalysisRequest::NORMAL_DOWNLOAD);

  // Both the DLP and malware violations generate an event.
  base::RunLoop run_loop;
  std::set<std::string> zip_types = {"application/zip",
                                     "application/x-zip-compressed"};
  enterprise_connectors::test::EventReportValidator validator(client());
  validator.SetDoneClosure(run_loop.QuitClosure());
  validator.ExpectSensitiveDataEventAndDangerousDeepScanningResult(
      /*url*/ url.spec(),
      /*tab_url*/ url.spec(),
      /*source*/ "",
      /*destination*/ "",
      /*filename*/
      connectors_machine_scope()
          ? (*download_items().begin())->GetTargetFilePath().AsUTF8Unsafe()
          : "zipfile_two_archives.zip",
      // sha256sum chrome/test/data/safe_browsing/download_protection/\
      // zipfile_two_archives.zip |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "339C8FFDAE735C4F1846D0E6FF07FBD85CAEE6D96045AAEF5B30F3220836643C",
      /*threat_type*/ "POTENTIALLY_UNWANTED",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
      /*dlp_verdict*/ *dlp_result,
      /*mimetypes*/ &zip_types,
      /*size*/ is_obfuscated() ? 276 + kSingleChunkObfuscationOverhead : 276,
      /*result*/
      enterprise_connectors::EventResultToString(
          enterprise_connectors::EventResult::WARNED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*scan_id*/ last_request().request_token());
  WaitForDownloadToFinish();

  run_loop.Run();

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
      : DownloadDeepScanningBrowserTestBase(
            /*connectors_machine_scope=*/GetParam(),
            /*is_consumer=*/false,
            /*is_obfuscated=*/false) {}
  ~DownloadRestrictionsDeepScanningBrowserTest() override = default;

  void SetUpOnMainThread() override {
    DownloadDeepScanningBrowserTestBase::SetUpOnMainThread();

    browser()->profile()->GetPrefs()->SetInteger(
        policy::policy_prefs::kDownloadRestrictions,
        static_cast<int>(policy::DownloadRestriction::DANGEROUS_FILES));
    enterprise_connectors::test::SetAnalysisConnector(
        browser()->profile()->GetPrefs(),
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
  safe_browsing::FileTypePoliciesTestOverlay scoped_all_file_types_dangerous =
      safe_browsing::ScopedMarkAllFilesDangerousForTesting();

  // This allows the blocking DM token reads happening on profile-Connector
  // triggers.
  base::ScopedAllowBlockingForTesting allow_blocking;

  SetUpReporting();

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  base::FilePath main_file = DownloadPrefs(browser()->profile())
                                 .DownloadPath()
                                 .AppendASCII("zipfile_two_archives.zip");
  base::RunLoop run_loop;
  enterprise_connectors::test::EventReportValidator validator(client());
  validator.SetDoneClosure(run_loop.QuitClosure());
  std::set<std::string> zip_types = {"application/zip",
                                     "application/x-zip-compressed"};
  validator.ExpectDangerousDownloadEvent(
      /*url*/ url.spec(),
      /*tab_url*/ url.spec(),
      /*filename*/
      connectors_machine_scope() ? main_file.AsUTF8Unsafe()
                                 : "zipfile_two_archives.zip",
      // sha256sum chrome/test/data/safe_browsing/download_protection/\
      // zipfile_two_archives.zip |  tr '[:lower:]' '[:upper:]'
      /*sha*/ "",
      /*threat_type*/ "DANGEROUS_FILE_TYPE",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
      /*mimetypes*/ &zip_types,
      /*size*/ 276,
      /*result*/
      enterprise_connectors::EventResultToString(
          enterprise_connectors::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier());

  WaitForDownloadToFinish();

  run_loop.Run();

  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  EXPECT_EQ(item->GetState(), download::DownloadItem::INTERRUPTED);
}

class AllowlistedUrlDeepScanningBrowserTest
    : public DownloadDeepScanningBrowserTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  AllowlistedUrlDeepScanningBrowserTest()
      : DownloadDeepScanningBrowserTestBase(
            /*connectors_machine_scope=*/std::get<0>(GetParam()),
            /*is_consumer=*/false,
            /*is_obfuscated=*/std::get<1>(GetParam())) {}
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
                         testing::Combine(
                             /*connectors_machine_scope=*/testing::Bool(),
                             /*is_obfuscated=*/testing::Bool()));

IN_PROC_BROWSER_TEST_P(AllowlistedUrlDeepScanningBrowserTest,
                       AllowlistedUrlStillDoesDlpAndMalware) {
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

  // The malware scan also runs synchronously, but finds no violation
  result = sync_response.add_results();
  result->set_tag("malware");
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);

  ExpectContentAnalysisResumableMetadataResponse({"dlp", "malware"});
  ExpectContentAnalysisResumableContentResponse(sync_response);

  GURL url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/zipfile_two_archives.zip");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  WaitForDeepScanRequest();

  EXPECT_EQ(last_request().reason(),
            enterprise_connectors::ContentAnalysisRequest::NORMAL_DOWNLOAD);

  WaitForDownloadToFinish();

  // The file should be blocked for sensitive content.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::
                DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK);
  EXPECT_EQ(item->GetState(), download::DownloadItem::INTERRUPTED);
}

class WaitForModalObserver : public DeepScanningRequest::Observer {
 public:
  explicit WaitForModalObserver(DeepScanningRequest* request)
      : request_(request),
        run_loop_(base::RunLoop::Type::kNestableTasksAllowed) {
    request_->AddObserver(this);
  }

  ~WaitForModalObserver() override {
    if (request_) {
      request_->RemoveObserver(this);
    }
  }

  void Wait() { run_loop_.Run(); }

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
    if (request_) {
      request_->RemoveObserver(this);
    }
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

class SavePackageDeepScanningBrowserTest
    : public DownloadDeepScanningBrowserTestBase {
 public:
  SavePackageDeepScanningBrowserTest()
      : DownloadDeepScanningBrowserTestBase(/*connectors_machine_scope=*/true,
                                            /*is_consumer=*/false,
                                            /*is_obfuscated=*/false) {}

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

  enterprise_connectors::ContentAnalysisResponse response;
  auto* result = response.add_results();
  result->set_tag("dlp");
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);

  ExpectContentAnalysisResumableMetadataResponse({"dlp"});
  ExpectContentAnalysisResumableContentResponse(response);

  base::RunLoop run_loop;
  content::SavePackageFinishedObserver observer(
      browser()->profile()->GetDownloadManager(), run_loop.QuitClosure());
  base::FilePath main_file = GetSaveDir().AppendASCII("text.htm");
  base::FilePath extra_files_dir = GetSaveDir().AppendASCII("text_files");
  ASSERT_TRUE(browser()->tab_strip_model()->GetActiveWebContents()->SavePage(
      main_file, extra_files_dir, content::SAVE_PAGE_TYPE_AS_ONLY_HTML));
  WaitForDeepScanRequest();

  EXPECT_EQ(last_request().reason(),
            enterprise_connectors::ContentAnalysisRequest::SAVE_AS_DOWNLOAD);

  // The response should not trigger a security event.
  enterprise_connectors::test::EventReportValidator validator(client());
  validator.ExpectNoReport();

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

  // Prepares a scanning response that indicates the file should be blocked.
  enterprise_connectors::ContentAnalysisResponse response;
  auto* result = response.add_results();
  result->set_tag("dlp");
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* dlp_verdict = result->add_triggered_rules();
  dlp_verdict->set_action(enterprise_connectors::TriggeredRule::BLOCK);

  ExpectContentAnalysisResumableMetadataResponse({"dlp"});
  ExpectContentAnalysisResumableContentResponse(response);

  base::RunLoop run_loop;
  content::SavePackageFinishedObserver observer(
      browser()->profile()->GetDownloadManager(), run_loop.QuitClosure(),
      {download::DownloadItem::INTERRUPTED});
  base::FilePath main_file = GetSaveDir().AppendASCII("text.htm");
  base::FilePath extra_files_dir = GetSaveDir().AppendASCII("text_files");
  ASSERT_TRUE(browser()->tab_strip_model()->GetActiveWebContents()->SavePage(
      main_file, extra_files_dir, content::SAVE_PAGE_TYPE_AS_ONLY_HTML));
  WaitForDeepScanRequest();

  EXPECT_EQ(last_request().reason(),
            enterprise_connectors::ContentAnalysisRequest::SAVE_AS_DOWNLOAD);

  // The blocking response should trigger a security event.
  base::RunLoop validator_run_loop;
  enterprise_connectors::test::EventReportValidator validator(client());
  validator.SetDoneClosure(validator_run_loop.QuitClosure());
  std::set<std::string> mimetypes = {"text/plain"};
  validator.ExpectSensitiveDataEvent(
      /*url*/ url.spec(),
      /*tab_url*/ url.spec(),
      /*source*/ "",
      /*destination*/ "",
      /*filename*/ main_file.AsUTF8Unsafe(),
      // sha256sum chrome/test/data/save_page/text.txt | tr a-f A-F
      "9789A2E12D50EFA4B891D4EF95C5189FA4C98E34C84E1F8017CD8F574CA035DD",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
      /*dlp_verdict*/ *result,
      /*mimetypes*/ &mimetypes,
      /*size*/ 54,
      /*result*/
      enterprise_connectors::EventResultToString(
          enterprise_connectors::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*scan_id*/ last_request().request_token(),
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

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
  validator_run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SavePackageDeepScanningBrowserTest, KeepAfterWarning) {
  SetUpReporting();

  GURL url = embedded_test_server()->GetURL("/save_page/text.txt");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Prepares a scanning response that indicates the file should warn the user.
  enterprise_connectors::ContentAnalysisResponse response;
  auto* result = response.add_results();
  result->set_tag("dlp");
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* dlp_verdict = result->add_triggered_rules();
  dlp_verdict->set_action(enterprise_connectors::TriggeredRule::WARN);

  ExpectContentAnalysisResumableMetadataResponse({"dlp"});
  ExpectContentAnalysisResumableContentResponse(response);

  base::RunLoop save_package_run_loop;
  content::SavePackageFinishedObserver observer(
      browser()->profile()->GetDownloadManager(),
      save_package_run_loop.QuitClosure(), {download::DownloadItem::COMPLETE});
  base::FilePath main_file = GetSaveDir().AppendASCII("text.htm");
  base::FilePath extra_files_dir = GetSaveDir().AppendASCII("text_files");
  ASSERT_TRUE(browser()->tab_strip_model()->GetActiveWebContents()->SavePage(
      main_file, extra_files_dir, content::SAVE_PAGE_TYPE_AS_ONLY_HTML));
  WaitForDeepScanRequest();

  EXPECT_EQ(last_request().reason(),
            enterprise_connectors::ContentAnalysisRequest::SAVE_AS_DOWNLOAD);

  // The warning response should trigger a security event.
  base::RunLoop validator_run_loop;
  enterprise_connectors::test::EventReportValidator validator(client());
  validator.SetDoneClosure(validator_run_loop.QuitClosure());
  std::set<std::string> mimetypes = {"text/plain"};
  validator.ExpectSensitiveDataEvent(
      /*url*/ url.spec(),
      /*tab_url*/ url.spec(),
      /*source*/ "",
      /*destination*/ "",
      /*filename*/ main_file.AsUTF8Unsafe(),
      // sha256sum chrome/test/data/save_page/text.txt | tr a-f A-F
      "9789A2E12D50EFA4B891D4EF95C5189FA4C98E34C84E1F8017CD8F574CA035DD",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
      /*dlp_verdict*/ *result,
      /*mimetypes*/ &mimetypes,
      /*size*/ 54,
      /*result*/
      enterprise_connectors::EventResultToString(
          enterprise_connectors::EventResult::WARNED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*scan_id*/ last_request().request_token(),
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

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

  base::RunLoop validator_warn_run_loop;
  enterprise_connectors::test::EventReportValidator validator_warn(client());
  validator_warn.SetDoneClosure(validator_warn_run_loop.QuitClosure());
  validator_warn.ExpectSensitiveDataEvent(
      /*url*/ url.spec(),
      /*tab_url*/ url.spec(),
      /*source*/ "",
      /*destination*/ "",
      /*filename*/ main_file.AsUTF8Unsafe(),
      // sha256sum chrome/test/data/save_page/text.txt | tr a-f A-F
      "9789A2E12D50EFA4B891D4EF95C5189FA4C98E34C84E1F8017CD8F574CA035DD",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
      /*dlp_verdict*/ *result,
      /*mimetypes*/ &mimetypes,
      /*size*/ 54,
      /*result*/
      enterprise_connectors::EventResultToString(
          enterprise_connectors::EventResult::BYPASSED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*scan_id*/ last_request().request_token(),
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

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
  validator_warn_run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SavePackageDeepScanningBrowserTest,
                       DiscardAfterWarning) {
  SetUpReporting();

  GURL url = embedded_test_server()->GetURL("/save_page/text.txt");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Prepares a scanning response that indicates the file should warn the user.
  enterprise_connectors::ContentAnalysisResponse response;
  auto* result = response.add_results();
  result->set_tag("dlp");
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* dlp_verdict = result->add_triggered_rules();
  dlp_verdict->set_action(enterprise_connectors::TriggeredRule::WARN);

  ExpectContentAnalysisResumableMetadataResponse({"dlp"});
  ExpectContentAnalysisResumableContentResponse(response);

  base::RunLoop save_package_run_loop;
  content::SavePackageFinishedObserver observer(
      browser()->profile()->GetDownloadManager(),
      save_package_run_loop.QuitClosure(), {download::DownloadItem::CANCELLED});
  base::FilePath main_file = GetSaveDir().AppendASCII("text.htm");
  base::FilePath extra_files_dir = GetSaveDir().AppendASCII("text_files");
  ASSERT_TRUE(browser()->tab_strip_model()->GetActiveWebContents()->SavePage(
      main_file, extra_files_dir, content::SAVE_PAGE_TYPE_AS_ONLY_HTML));
  WaitForDeepScanRequest();

  EXPECT_EQ(last_request().reason(),
            enterprise_connectors::ContentAnalysisRequest::SAVE_AS_DOWNLOAD);

  // That warning response should trigger a security event.
  base::RunLoop validator_run_loop;
  enterprise_connectors::test::EventReportValidator validator(client());
  validator.SetDoneClosure(validator_run_loop.QuitClosure());
  std::set<std::string> mimetypes = {"text/plain"};
  validator.ExpectSensitiveDataEvent(
      /*url*/ url.spec(),
      /*tab_url*/ url.spec(),
      /*source*/ "",
      /*destination*/ "",
      /*filename*/ main_file.AsUTF8Unsafe(),
      // sha256sum chrome/test/data/save_page/text.txt | tr a-f A-F
      "9789A2E12D50EFA4B891D4EF95C5189FA4C98E34C84E1F8017CD8F574CA035DD",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
      /*dlp_verdict*/ *result,
      /*mimetypes*/ &mimetypes,
      /*size*/ 54,
      /*result*/
      enterprise_connectors::EventResultToString(
          enterprise_connectors::EventResult::WARNED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*scan_id*/ last_request().request_token(),
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

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

  enterprise_connectors::ContentAnalysisResponse response;
  auto* result = response.add_results();
  result->set_tag("dlp");
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* dlp_verdict = result->add_triggered_rules();
  dlp_verdict->set_action(enterprise_connectors::TriggeredRule::BLOCK);

  base::RunLoop validator_run_loop;
  enterprise_connectors::test::EventReportValidator validator(client());
  validator.SetDoneClosure(validator_run_loop.QuitClosure());
  std::set<std::string> mimetypes = {"text/plain"};

  ExpectContentAnalysisResumableMetadataResponse({"dlp"});
  ExpectContentAnalysisResumableContentResponse(response);

  base::RunLoop save_package_run_loop;
  content::SavePackageFinishedObserver observer(
      browser()->profile()->GetDownloadManager(),
      save_package_run_loop.QuitClosure(), {download::DownloadItem::COMPLETE});
  base::FilePath main_file = GetSaveDir().AppendASCII("text.htm");
  base::FilePath extra_files_dir = GetSaveDir().AppendASCII("text_files");
  ASSERT_TRUE(browser()->tab_strip_model()->GetActiveWebContents()->SavePage(
      main_file, extra_files_dir, content::SAVE_PAGE_TYPE_AS_ONLY_HTML));
  WaitForDeepScanRequest();

  validator.ExpectSensitiveDataEvent(
      /*url*/ url.spec(),
      /*tab_url*/ url.spec(),
      /*source*/ "",
      /*destination*/ "",
      /*filename*/ main_file.AsUTF8Unsafe(),
      // sha256sum chrome/test/data/save_page/text.txt | tr a-f A-F
      "9789A2E12D50EFA4B891D4EF95C5189FA4C98E34C84E1F8017CD8F574CA035DD",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
      /*dlp_verdict*/ *result,
      /*mimetypes*/ &mimetypes,
      /*size*/ 54,
      /*result*/
      enterprise_connectors::EventResultToString(
          enterprise_connectors::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*scan_id*/ last_request().request_token(),
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  EXPECT_EQ(last_request().reason(),
            enterprise_connectors::ContentAnalysisRequest::SAVE_AS_DOWNLOAD);

  // Opening the save package before the async response is obtained will
  // generate a warning event once it does come back, complete the
  // download and move the file to its final destination.
  ASSERT_EQ(download_items().size(), 1u);
  download::DownloadItem* item = *download_items().begin();
  DownloadItemModel model(item);
  model.CompleteSafeBrowsingScan();
  save_package_run_loop.Run();

  validator_run_loop.Run();

  EXPECT_EQ(item->GetDangerType(),
            download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_USER_VALIDATED);
  EXPECT_EQ(item->GetState(), download::DownloadItem::COMPLETE);

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(main_file));
  EXPECT_TRUE(base::ContentsEqual(GetTestFilePath(), main_file));
  EXPECT_FALSE(base::PathExists(extra_files_dir));
}

class FileSystemAccessDeepScanningBrowserTest
    : public DownloadDeepScanningBrowserTestBase {
 public:
  FileSystemAccessDeepScanningBrowserTest()
      : DownloadDeepScanningBrowserTestBase(/*connectors_machine_scope=*/true,
                                            /*is_consumer=*/false,
                                            /*is_obfuscated=*/false) {
    scoped_feature_list_.InitAndEnableFeature(
        safe_browsing::kEnterpriseFileSystemAccessDeepScan);
  }

  void SetUpOnMainThread() override {
    DownloadDeepScanningBrowserTestBase::SetUpOnMainThread();

    // Setup a temporary directory for the file save path.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Have file save path automatically selected by picker.
    ui::SelectFileDialog::SetFactory(
        std::make_unique<SelectPredeterminedFileDialogFactory>(
            std::vector<base::FilePath>{GetTestFilePath()}));

    // Navigate to a simple page for FS API calls to run from.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/title1.html")));
  }

  void TearDownOnMainThread() override {
    ui::SelectFileDialog::SetFactory(nullptr);
    DownloadDeepScanningBrowserTestBase::TearDownOnMainThread();
  }

  base::FilePath GetTestFilePath() {
    return temp_dir_.GetPath().AppendASCII("test_fsa_file.txt");
  }

  void InitiateWriteViaFSA(content::WebContents* web_contents,
                           const std::string& content) {
    // Script that triggers FSA API write. Executes async and creates promise.
    constexpr char js_script[] = R"(
      window.fsaPromise = (async (content) => {
        const handle = await window.showSaveFilePicker();
        const writable = await handle.createWritable();
        await writable.write(content);
        await writable.close();
      })($1);
    )";

    content::ExecuteScriptAsync(web_contents,
                                content::JsReplace(js_script, content));
  }

 protected:
  base::ScopedTempDir temp_dir_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// For FSA writes that trigger a block deep scan verdict, write should be
// blocked and the destination file empty.
IN_PROC_BROWSER_TEST_F(FileSystemAccessDeepScanningBrowserTest, BlockedWrite) {
  SetUpReporting();

  // Prepare the scan response with DLP block and successful malware scan.
  enterprise_connectors::ContentAnalysisResponse response;
  auto* dlp_result = response.add_results();
  dlp_result->set_tag("dlp");
  dlp_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* dlp_rule = dlp_result->add_triggered_rules();
  dlp_rule->set_action(enterprise_connectors::TriggeredRule::BLOCK);

  auto* malware_result = response.add_results();
  malware_result->set_tag("malware");
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);

  ExpectContentAnalysisResumableMetadataResponse({"dlp", "malware"});
  ExpectContentAnalysisResumableContentResponse(response);

  // Setup message queue for JS responses.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  content::DOMMessageQueue message_queue(web_contents);

  base::RunLoop validator_run_loop;
  enterprise_connectors::test::EventReportValidator validator(client());
  validator.SetDoneClosure(validator_run_loop.QuitClosure());

  InitiateWriteViaFSA(web_contents, kTestContent);

  WaitForDeepScanRequest();

  GURL url = embedded_test_server()->GetURL("/title1.html");
  std::set<std::string> mimetypes = {"text/plain"};
  validator.ExpectSensitiveDataEvent(
      /*url*/ url.spec(),
      /*tab_url*/ url.spec(),
      /*source*/ "",
      /*destination*/ "",
      /*filename*/ GetTestFilePath().AsUTF8Unsafe(),
      // echo -n [kTestContent] | sha256sum | tr a-f A-F
      /*sha*/
      "6AE8A75555209FD6C44157C0AED8016E763FF435A19CF186F76863140143FF72",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
      /*dlp_verdict*/ response.results(0),
      /*mimetypes*/ &mimetypes,
      /*size*/ sizeof(kTestContent) - 1,
      enterprise_connectors::EventResultToString(
          enterprise_connectors::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*scan_id*/ last_request().request_token(),
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  EXPECT_EQ(last_request().reason(),
            enterprise_connectors::ContentAnalysisRequest::NORMAL_DOWNLOAD);

  validator_run_loop.Run();

  content::EvalJsResult result =
      content::EvalJs(web_contents, "window.fsaPromise");

  ASSERT_THAT(result, content::EvalJsResult::IsError());

  // TODO(crbug.com/407065784): Improve error message for SB checks.
  EXPECT_EQ(result.error,
            "a JavaScript error: \"AbortError: Blocked by Safe Browsing.\"\n");

  // File is created but remains empty due to block.
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(GetTestFilePath()));
  EXPECT_EQ(0, base::GetFileSize(GetTestFilePath()));
}

// For FSA writes that trigger a safe deep scan verdict, write should be allowed
// and the destination file populated appropriately.
IN_PROC_BROWSER_TEST_F(FileSystemAccessDeepScanningBrowserTest, AllowedWrite) {
  SetUpReporting();

  // Prepare the scan response with no DLP or malware rules triggered.
  enterprise_connectors::ContentAnalysisResponse response;
  auto* dlp_result = response.add_results();
  dlp_result->set_tag("dlp");
  dlp_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);

  auto* malware_result = response.add_results();
  malware_result->set_tag("malware");
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);

  ExpectContentAnalysisResumableMetadataResponse({"dlp", "malware"});
  ExpectContentAnalysisResumableContentResponse(response);

  // Setup message queue for JS responses.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  content::DOMMessageQueue message_queue(web_contents);

  InitiateWriteViaFSA(web_contents, kTestContent);

  WaitForDeepScanRequest();

  EXPECT_EQ(last_request().reason(),
            enterprise_connectors::ContentAnalysisRequest::NORMAL_DOWNLOAD);

  ASSERT_THAT(content::EvalJs(web_contents, "window.fsaPromise"),
              content::EvalJsResult::IsOk());

  // Checks that file is written successfully.
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(GetTestFilePath()));

  std::string file_content;
  EXPECT_TRUE(base::ReadFileToString(GetTestFilePath(), &file_content));
  EXPECT_EQ(file_content, kTestContent);
}

// For FSA writes that trigger a warn deep scan verdict, write should be allowed
// and the destination file populated appropriately.
IN_PROC_BROWSER_TEST_F(FileSystemAccessDeepScanningBrowserTest, WarnedWrite) {
  SetUpReporting();

  // Prepare the scan response with warn DLP rule triggered.
  enterprise_connectors::ContentAnalysisResponse response;
  auto* dlp_result = response.add_results();
  dlp_result->set_tag("dlp");
  dlp_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  auto* dlp_rule = dlp_result->add_triggered_rules();
  dlp_rule->set_action(enterprise_connectors::TriggeredRule::WARN);

  auto* malware_result = response.add_results();
  malware_result->set_tag("malware");
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);

  ExpectContentAnalysisResumableMetadataResponse({"dlp", "malware"});
  ExpectContentAnalysisResumableContentResponse(response);

  // Setup message queue for JS responses.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  content::DOMMessageQueue message_queue(web_contents);

  base::RunLoop validator_run_loop;
  enterprise_connectors::test::EventReportValidator validator(client());
  validator.SetDoneClosure(validator_run_loop.QuitClosure());

  InitiateWriteViaFSA(web_contents, kTestContent);

  WaitForDeepScanRequest();

  GURL url = embedded_test_server()->GetURL("/title1.html");
  std::set<std::string> mimetypes = {"text/plain"};
  validator.ExpectSensitiveDataEvent(
      /*url*/ url.spec(),
      /*tab_url*/ url.spec(),
      /*source*/ "",
      /*destination*/ "",
      /*filename*/ GetTestFilePath().AsUTF8Unsafe(),
      // echo -n [kTestContent] | sha256sum | tr a-f A-F
      /*sha*/
      "6AE8A75555209FD6C44157C0AED8016E763FF435A19CF186F76863140143FF72",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
      /*dlp_verdict*/ response.results(0),
      /*mimetypes*/ &mimetypes,
      /*size*/ sizeof(kTestContent) - 1,
      enterprise_connectors::EventResultToString(
          enterprise_connectors::EventResult::WARNED),
      /*username*/ kUserName,
      /*profile_identifier*/ GetProfileIdentifier(),
      /*scan_id*/ last_request().request_token(),
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  validator_run_loop.Run();

  // For warn verdicts, we allow the write to happen as there is currently no
  // dialog that allows the user to bypass warnings.
  ASSERT_THAT(content::EvalJs(web_contents, "window.fsaPromise"),
              content::EvalJsResult::IsOk());

  // Checks that file is written successfully.
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(GetTestFilePath()));

  std::string file_content;
  EXPECT_TRUE(base::ReadFileToString(GetTestFilePath(), &file_content));
  EXPECT_EQ(file_content, kTestContent);
}

// Fail-open behavior for failed deep scan requests on FSA writes.
IN_PROC_BROWSER_TEST_F(FileSystemAccessDeepScanningBrowserTest,
                       DeepScanFailure) {
  SetUpReporting();

  // Configure scan response with failed status.
  enterprise_connectors::ContentAnalysisResponse response;
  auto* dlp_result = response.add_results();
  dlp_result->set_tag("dlp");
  dlp_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::FAILURE);

  auto* malware_result = response.add_results();
  malware_result->set_tag("malware");
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::FAILURE);

  ExpectContentAnalysisResumableMetadataResponse({"dlp", "malware"});
  ExpectContentAnalysisResumableContentResponse(response);

  // Setup message queue for JS responses.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  content::DOMMessageQueue message_queue(web_contents);

  InitiateWriteViaFSA(web_contents, kTestContent);

  WaitForDeepScanRequest();

  // When scan fails, write should still succeed (fail-open behavior).
  ASSERT_THAT(content::EvalJs(web_contents, "window.fsaPromise"),
              content::EvalJsResult::IsOk());

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(GetTestFilePath()));

  std::string file_content;
  EXPECT_TRUE(base::ReadFileToString(GetTestFilePath(), &file_content));
  EXPECT_EQ(file_content, kTestContent);
}

}  // namespace safe_browsing
