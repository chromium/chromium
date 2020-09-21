// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/test_binary_upload_service.h"
#include "chrome/browser/safe_browsing/download_protection/check_native_file_system_write_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_feedback_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/download_protection/ppapi_download_request.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_reporting_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/test_extension_event_observer.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "chrome/common/safe_browsing/mock_binary_feature_extractor.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/content/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/safe_browsing/core/db/test_database_manager.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/file_type_policies_test_util.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "components/safe_browsing/core/proto/webprotect.pb.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/test_event_router.h"
#include "net/base/url_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip.h"
#include "url/gurl.h"

using base::RunLoop;
using content::BrowserThread;
using content::NativeFileSystemWriteItem;
using ::testing::_;
using ::testing::Assign;
using ::testing::ContainerEq;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::SizeIs;
using ::testing::StrictMock;

namespace OnDangerousDownloadOpened =
    extensions::api::safe_browsing_private::OnDangerousDownloadOpened;

namespace safe_browsing {

namespace {

// A SafeBrowsingDatabaseManager implementation that returns a fixed result for
// a given URL.
class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager() {}

  MOCK_METHOD1(MatchDownloadWhitelistUrl, bool(const GURL&));
  MOCK_METHOD1(MatchDownloadWhitelistString, bool(const std::string&));
  MOCK_METHOD2(CheckDownloadUrl,
               bool(const std::vector<GURL>& url_chain,
                    SafeBrowsingDatabaseManager::Client* client));

 private:
  ~MockSafeBrowsingDatabaseManager() override {}
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingDatabaseManager);
};

std::unique_ptr<KeyedService> CreateTestBinaryUploadService(
    content::BrowserContext* browser_context) {
  return std::make_unique<TestBinaryUploadService>();
}

class FakeSafeBrowsingService : public TestSafeBrowsingService {
 public:
  explicit FakeSafeBrowsingService(Profile* profile)
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        download_report_count_(0) {
    services_delegate_ = ServicesDelegate::CreateForTest(this, this);
    BinaryUploadServiceFactory::GetInstance()->SetTestingFactory(
        profile, base::BindRepeating(&CreateTestBinaryUploadService));
    mock_database_manager_ = new MockSafeBrowsingDatabaseManager();
  }

  // Returned pointer has the same lifespan as the database_manager_ refcounted
  // object.
  MockSafeBrowsingDatabaseManager* mock_database_manager() {
    return mock_database_manager_.get();
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return test_shared_loader_factory_;
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory(
      Profile* profile) override {
    if (!base::FeatureList::IsEnabled(kSafeBrowsingSeparateNetworkContexts))
      return GetURLLoaderFactory();
    auto it = test_shared_loader_factory_map_.find(profile);
    if (it == test_shared_loader_factory_map_.end())
      return nullptr;
    return it->second;
  }

  void SendSerializedDownloadReport(Profile* profile,
                                    const std::string& unused_report) override {
    download_report_count_++;
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  network::TestURLLoaderFactory* GetTestURLLoaderFactory(Profile* profile) {
    if (!base::FeatureList::IsEnabled(kSafeBrowsingSeparateNetworkContexts))
      return test_url_loader_factory();
    auto it = test_url_loader_factory_map_.find(profile);
    if (it == test_url_loader_factory_map_.end())
      return nullptr;
    return it->second.get();
  }

  void CreateTestURLLoaderFactoryForProfile(Profile* profile) {
    test_url_loader_factory_map_[profile] =
        std::make_unique<network::TestURLLoaderFactory>();
    test_shared_loader_factory_map_[profile] =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            test_url_loader_factory_map_[profile].get());
  }

  int download_report_count() { return download_report_count_; }

 protected:
  ~FakeSafeBrowsingService() override {}

  void RegisterAllDelayedAnalysis() override {}

 private:
  // ServicesDelegate::ServicesCreator:
  bool CanCreateDatabaseManager() override { return true; }
  bool CanCreateIncidentReportingService() override { return true; }
  safe_browsing::SafeBrowsingDatabaseManager* CreateDatabaseManager() override {
    return mock_database_manager_.get();
  }
  IncidentReportingService* CreateIncidentReportingService() override {
    return new IncidentReportingService(nullptr);
  }

  // TODO(crbug/1049833): Remove these as we rollout separate network contexts.
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  base::flat_map<Profile*, std::unique_ptr<network::TestURLLoaderFactory>>
      test_url_loader_factory_map_;
  base::flat_map<Profile*, scoped_refptr<network::SharedURLLoaderFactory>>
      test_shared_loader_factory_map_;

  scoped_refptr<MockSafeBrowsingDatabaseManager> mock_database_manager_;
  int download_report_count_;

  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingService);
};

using NiceMockDownloadItem = NiceMock<download::MockDownloadItem>;

}  // namespace

ACTION_P(SetCertificateContents, contents) {
  arg1->add_certificate_chain()->add_element()->set_certificate(contents);
}

ACTION_P(SetDosHeaderContents, contents) {
  arg2->mutable_pe_headers()->set_dos_header(contents);
  return true;
}

ACTION_P(TrustSignature, contents) {
  arg1->set_trusted(true);
  // Add a certificate chain.  Note that we add the certificate twice so that
  // it appears as its own issuer.

  ClientDownloadRequest_CertificateChain* chain = arg1->add_certificate_chain();
  chain->add_element()->set_certificate(contents.data(), contents.size());
  chain->add_element()->set_certificate(contents.data(), contents.size());
}

ACTION_P(CheckDownloadUrlDone, threat_type) {
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SafeBrowsingDatabaseManager::Client::OnCheckDownloadUrlResult,
          base::Unretained(arg1), arg0, threat_type));
}

class DownloadProtectionServiceTestBase
    : public ChromeRenderViewHostTestHarness {
 public:
  DownloadProtectionServiceTestBase()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    in_process_utility_thread_helper_ =
        std::make_unique<content::InProcessUtilityThreadHelper>();

    // Use TestPasswordStore to remove a possible race. Normally the
    // PasswordStore does its database manipulation on the DB thread, which
    // creates a possible race during navigation. Specifically the
    // PasswordManager will ignore any forms in a page if the load from the
    // PasswordStore has not completed. ChromePasswordProtectionService uses
    // PasswordStore.
    PasswordStoreFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                content::BrowserContext, password_manager::TestPasswordStore>));
    if (base::FeatureList::IsEnabled(
            password_manager::features::kEnablePasswordsAccountStorage)) {
      AccountPasswordStoreFactory::GetInstance()->SetTestingFactory(
          profile(), base::BindRepeating(&password_manager::BuildPasswordStore<
                                         content::BrowserContext,
                                         password_manager::TestPasswordStore>));
    }

    // Start real threads for the IO and File threads so that the DCHECKs
    // to test that we're on the correct thread work.
    sb_service_ =
        base::MakeRefCounted<StrictMock<FakeSafeBrowsingService>>(profile());
    sb_service_->Initialize();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        sb_service_.get());
    binary_feature_extractor_ =
        base::MakeRefCounted<StrictMock<MockBinaryFeatureExtractor>>();
    ON_CALL(*binary_feature_extractor_, ExtractImageFeatures(_, _, _, _))
        .WillByDefault(Return(true));
    download_service_ = sb_service_->download_protection_service();
    download_service_->binary_feature_extractor_ = binary_feature_extractor_;
    download_service_->SetEnabled(true);
    client_download_request_subscription_ =
        download_service_->RegisterClientDownloadRequestCallback(
            base::BindRepeating(
                &DownloadProtectionServiceTestBase::OnClientDownloadRequest,
                base::Unretained(this)));
    ppapi_download_request_subscription_ =
        download_service_->RegisterPPAPIDownloadRequestCallback(
            base::BindRepeating(
                &DownloadProtectionServiceTestBase::OnPPAPIDownloadRequest,
                base::Unretained(this)));
    native_file_system_write_request_subscription_ =
        download_service_->RegisterNativeFileSystemWriteRequestCallback(
            base::BindRepeating(
                &DownloadProtectionServiceTestBase::OnPPAPIDownloadRequest,
                base::Unretained(this)));
    RunLoop().RunUntilIdle();
    has_result_ = false;

    base::FilePath source_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_path));
    testdata_path_ = source_path.AppendASCII("chrome")
                         .AppendASCII("test")
                         .AppendASCII("data")
                         .AppendASCII("safe_browsing")
                         .AppendASCII("download_protection");

    ASSERT_TRUE(profile()->CreateHistoryService());

    // Setup a directory to place test files in.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Turn off binary sampling by default.
    SetBinarySamplingProbability(0.0);

    // |test_event_router_| is owned by KeyedServiceFactory.
    test_event_router_ = extensions::CreateAndUseTestEventRouter(profile());
    extensions::SafeBrowsingPrivateEventRouterFactory::GetInstance()
        ->SetTestingFactory(
            profile(),
            base::BindRepeating(&BuildSafeBrowsingPrivateEventRouter));

    SetDMTokenForTesting(
        policy::DMToken::CreateValidTokenForTesting("dm_token"));

    ASSERT_TRUE(testing_profile_manager_.SetUp());

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());

    enterprise_connectors::ConnectorsManager::GetInstance()->SetUpForTesting();
  }

  void TearDown() override {
    enterprise_connectors::ConnectorsManager::GetInstance()
        ->TearDownForTesting();
    client_download_request_subscription_.reset();
    ppapi_download_request_subscription_.reset();
    native_file_system_write_request_subscription_.reset();
    sb_service_->ShutDown();
    // Flush all of the thread message loops to ensure that there are no
    // tasks currently running.
    FlushThreadMessageLoops();
    sb_service_ = nullptr;
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
    in_process_utility_thread_helper_ = nullptr;
    identity_test_env_adaptor_.reset();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  void EnableFeatures(const std::vector<base::Feature>& features) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(features, {});
  }

  void DisableFeatures(const std::vector<base::Feature>& features) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures({}, features);
  }

  void SetWhitelistedDownloadSampleRate(double target_rate) {
    download_service_->whitelist_sample_rate_ = target_rate;
  }

  void SetBinarySamplingProbability(double target_rate) {
    std::unique_ptr<DownloadFileTypeConfig> config =
        policies_.DuplicateConfig();
    config->set_sampled_ping_probability(target_rate);
    policies_.SwapConfig(config);
  }

  bool RequestContainsResource(const ClientDownloadRequest& request,
                               ClientDownloadRequest::ResourceType type,
                               const std::string& url,
                               const std::string& referrer) {
    for (int i = 0; i < request.resources_size(); ++i) {
      if (request.resources(i).url() == url &&
          request.resources(i).type() == type &&
          (referrer.empty() || request.resources(i).referrer() == referrer)) {
        return true;
      }
    }
    return false;
  }

  // At this point we only set the server IP for the download itself.
  bool RequestContainsServerIp(const ClientDownloadRequest& request,
                               const std::string& remote_address) {
    for (int i = 0; i < request.resources_size(); ++i) {
      // We want the last DOWNLOAD_URL in the chain.
      if (request.resources(i).type() == ClientDownloadRequest::DOWNLOAD_URL &&
          (i + 1 == request.resources_size() ||
           request.resources(i + 1).type() !=
               ClientDownloadRequest::DOWNLOAD_URL)) {
        return remote_address == request.resources(i).remote_ip();
      }
    }
    return false;
  }

  static const ClientDownloadRequest_ArchivedBinary* GetRequestArchivedBinary(
      const ClientDownloadRequest& request,
      const std::string& file_basename) {
    for (const auto& archived_binary : request.archived_binary()) {
      if (archived_binary.file_basename() == file_basename)
        return &archived_binary;
    }
    return nullptr;
  }

  // Flushes any pending tasks in the message loops of all threads.
  void FlushThreadMessageLoops() {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    FlushMessageLoop(BrowserThread::IO);
    RunLoop().RunUntilIdle();
  }

  // Reads a single PEM-encoded certificate from the testdata directory.
  // Returns nullptr on failure.
  scoped_refptr<net::X509Certificate> ReadTestCertificate(
      const std::string& filename) {
    std::string cert_data;
    if (!base::ReadFileToString(testdata_path_.AppendASCII(filename),
                                &cert_data)) {
      return nullptr;
    }
    net::CertificateList certs =
        net::X509Certificate::CreateCertificateListFromBytes(
            cert_data.data(), cert_data.size(),
            net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
    return certs.empty() ? nullptr : certs[0];
  }

  const ClientDownloadRequest* GetClientDownloadRequest() const {
    return last_client_download_request_.get();
  }

  bool HasClientDownloadRequest() const {
    return last_client_download_request_.get() != nullptr;
  }

  void ClearClientDownloadRequest() { last_client_download_request_.reset(); }

  void PrepareResponse(ClientDownloadResponse::Verdict verdict,
                       net::HttpStatusCode response_code,
                       int net_error,
                       bool upload_requested = false) {
    if (net_error != net::OK) {
      network::URLLoaderCompletionStatus status;
      sb_service_->test_url_loader_factory()->AddResponse(
          PPAPIDownloadRequest::GetDownloadRequestUrl(),
          network::mojom::URLResponseHead::New(), std::string(),
          network::URLLoaderCompletionStatus(net_error));
      return;
    }
    ClientDownloadResponse response;
    response.set_verdict(verdict);
    response.set_upload(upload_requested);
    sb_service_->test_url_loader_factory()->AddResponse(
        PPAPIDownloadRequest::GetDownloadRequestUrl().spec(),
        response.SerializeAsString());
  }

  void PrepareBasicDownloadItem(
      NiceMockDownloadItem* item,
      const std::vector<std::string> url_chain_items,
      const std::string& referrer_url,
      const base::FilePath::StringType& tmp_path_literal,
      const base::FilePath::StringType& final_path_literal) {
    base::FilePath tmp_path = temp_dir_.GetPath().Append(tmp_path_literal);
    base::FilePath final_path = temp_dir_.GetPath().Append(final_path_literal);
    PrepareBasicDownloadItemWithFullPaths(item, url_chain_items, referrer_url,
                                          tmp_path, final_path);
  }

  void PrepareBasicDownloadItemWithFullPaths(
      NiceMockDownloadItem* item,
      const std::vector<std::string> url_chain_items,
      const std::string& referrer_url,
      const base::FilePath& tmp_full_path,
      const base::FilePath& final_full_path) {
    url_chain_.clear();
    for (std::string item : url_chain_items)
      url_chain_.push_back(GURL(item));
    if (url_chain_.empty())
      url_chain_.push_back(GURL());
    referrer_ = GURL(referrer_url);
    tmp_path_ = tmp_full_path;
    final_path_ = final_full_path;
    hash_ = "hash";

    EXPECT_CALL(*item, GetURL()).WillRepeatedly(Invoke([&]() -> const GURL& {
      return url_chain_.back();
    }));
    EXPECT_CALL(*item, GetFullPath()).WillRepeatedly(ReturnRef(tmp_path_));
    EXPECT_CALL(*item, GetTargetFilePath())
        .WillRepeatedly(ReturnRef(final_path_));
    EXPECT_CALL(*item, GetUrlChain()).WillRepeatedly(ReturnRef(url_chain_));
    EXPECT_CALL(*item, GetReferrerUrl()).WillRepeatedly(ReturnRef(referrer_));
    EXPECT_CALL(*item, GetTabUrl())
        .WillRepeatedly(ReturnRef(GURL::EmptyGURL()));
    EXPECT_CALL(*item, GetTabReferrerUrl())
        .WillRepeatedly(ReturnRef(GURL::EmptyGURL()));
    EXPECT_CALL(*item, GetHash()).WillRepeatedly(ReturnRef(hash_));
    EXPECT_CALL(*item, GetReceivedBytes()).WillRepeatedly(Return(100));
    EXPECT_CALL(*item, HasUserGesture()).WillRepeatedly(Return(true));
    EXPECT_CALL(*item, GetRemoteAddress()).WillRepeatedly(Return(""));
  }

  std::unique_ptr<NativeFileSystemWriteItem>
  PrepareBasicNativeFileSystemWriteItem(
      const base::FilePath::StringType& tmp_path_literal,
      const base::FilePath::StringType& final_path_literal) {
    base::FilePath tmp_path = temp_dir_.GetPath().Append(tmp_path_literal);
    base::FilePath final_path = temp_dir_.GetPath().Append(final_path_literal);
    return PrepareBasicNativeFileSystemWriteItemWithFullPaths(tmp_path,
                                                              final_path);
  }

  std::unique_ptr<NativeFileSystemWriteItem>
  PrepareBasicNativeFileSystemWriteItemWithFullPaths(
      const base::FilePath& tmp_path,
      const base::FilePath& final_path) {
    tmp_path_ = tmp_path;
    final_path_ = final_path;
    hash_ = "hash";
    auto result = std::make_unique<NativeFileSystemWriteItem>();
    result->target_file_path = final_path;
    result->full_path = tmp_path;
    result->sha256_hash = hash_;
    result->size = 100;
    result->frame_url = GURL("https://example.com/foo/bar");
    result->has_user_gesture = true;
    result->web_contents = web_contents();
    result->browser_context = profile();
    return result;
  }

  std::unique_ptr<NativeFileSystemWriteItem> CloneNativeFileSystemWriteItem(
      NativeFileSystemWriteItem* in) {
    auto result = std::make_unique<NativeFileSystemWriteItem>();
    result->target_file_path = in->target_file_path;
    result->full_path = in->full_path;
    result->sha256_hash = in->sha256_hash;
    result->size = in->size;
    result->frame_url = in->frame_url;
    result->has_user_gesture = in->has_user_gesture;
    result->web_contents = in->web_contents;
    result->browser_context = in->browser_context;
    return result;
  }

  void AddDomainToEnterpriseWhitelist(const std::string& domain) {
    ListPrefUpdate update(profile()->GetPrefs(),
                          prefs::kSafeBrowsingWhitelistDomains);
    update.Get()->AppendString(domain);
  }

  void SetPasswordProtectedAllowedPref(
      AllowPasswordProtectedFilesValues value) {
    SetAllowPasswordProtectedFilesPolicyForConnectors(value);
  }

  void SetBlockLargeFilesPref(BlockLargeFileTransferValues value) {
    SetBlockLargeFileTransferPolicyForConnectors(value);
  }

  void SetBlockUnsupportedFiletypePref(BlockUnsupportedFiletypesValues value) {
    SetBlockUnsupportedFileTypesPolicyForConnectors(value);
  }

  void SetSendFilesForMalwareCheckPref(SendFilesForMalwareCheckValues value) {
    SetMalwarePolicyForConnectors(value);
  }

  void SetCheckContentCompliancePref(CheckContentComplianceValues value) {
    SetDlpPolicyForConnectors(value);
  }

  void SetUrlToCheckContentCompliance(const std::string& url_pattern) {
    AddUrlsToCheckComplianceOfDownloadsForConnectors({url_pattern});
  }

  // Helper function to simulate a user gesture, then a link click.
  // The usual NavigateAndCommit is unsuitable because it creates
  // browser-initiated navigations, causing us to drop the referrer.
  // TODO(drubery): This function could be eliminated if we dropped referrer
  // depending on PageTransition. This could help eliminate edge cases in
  // browser/renderer navigations.
  void SimulateLinkClick(const GURL& url) {
    content::WebContentsTester::For(web_contents())
        ->TestDidReceiveMouseDownEvent();
    std::unique_ptr<content::NavigationSimulator> navigation =
        content::NavigationSimulator::CreateRendererInitiated(
            url, web_contents()->GetMainFrame());

    navigation->SetTransition(ui::PAGE_TRANSITION_LINK);
    navigation->Commit();
  }

 private:
  // Helper functions for FlushThreadMessageLoops.
  void RunAllPendingAndQuitUI(const base::Closure& quit_closure) {
    RunLoop().RunUntilIdle();
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, quit_closure);
  }

  void PostRunMessageLoopTask(BrowserThread::ID thread,
                              const base::Closure& quit_closure) {
    base::PostTask(
        FROM_HERE, {thread},
        base::BindOnce(
            &DownloadProtectionServiceTestBase::RunAllPendingAndQuitUI,
            base::Unretained(this), quit_closure));
  }

  void FlushMessageLoop(BrowserThread::ID thread) {
    RunLoop run_loop;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &DownloadProtectionServiceTestBase::PostRunMessageLoopTask,
            base::Unretained(this), thread, run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnClientDownloadRequest(download::DownloadItem* download,
                               const ClientDownloadRequest* request) {
    if (request)
      last_client_download_request_.reset(new ClientDownloadRequest(*request));
    else
      last_client_download_request_.reset();
  }

  void OnPPAPIDownloadRequest(const ClientDownloadRequest* request) {
    if (request)
      last_client_download_request_.reset(new ClientDownloadRequest(*request));
    else
      last_client_download_request_.reset();
  }

 public:
  enum ArchiveType { ZIP, DMG };

  void CheckDoneCallback(const base::Closure& quit_closure,
                         DownloadCheckResult result) {
    result_ = result;
    has_result_ = true;

    // Scanning has not completed in this case, allow it to continue.
    if (result != DownloadCheckResult::ASYNC_SCANNING)
      quit_closure.Run();
  }

  void SyncCheckDoneCallback(DownloadCheckResult result) {
    result_ = result;
    has_result_ = true;
  }

  testing::AssertionResult IsResult(DownloadCheckResult expected) {
    if (!has_result_)
      return testing::AssertionFailure() << "No result";
    has_result_ = false;
    return result_ == expected ? testing::AssertionSuccess()
                               : testing::AssertionFailure()
                                     << "Expected "
                                     << static_cast<int>(expected) << ", got "
                                     << static_cast<int>(result_);
  }

  void SetExtendedReportingPreference(bool is_extended_reporting) {
    SetExtendedReportingPrefForTests(profile()->GetPrefs(),
                                     is_extended_reporting);
  }

  // Verify that corrupted ZIP/DMGs do send a ping.
  void CheckClientDownloadReportCorruptArchive(ArchiveType type);

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  // This will effectively mask the global Singleton while this is in scope.
  FileTypePoliciesTestOverlay policies_;

  scoped_refptr<FakeSafeBrowsingService> sb_service_;
  scoped_refptr<MockBinaryFeatureExtractor> binary_feature_extractor_;
  DownloadProtectionService* download_service_;
  DownloadCheckResult result_;
  bool has_result_;
  std::unique_ptr<content::InProcessUtilityThreadHelper>
      in_process_utility_thread_helper_;
  base::FilePath testdata_path_;
  ClientDownloadRequestSubscription client_download_request_subscription_;
  PPAPIDownloadRequestSubscription ppapi_download_request_subscription_;
  NativeFileSystemWriteRequestSubscription
      native_file_system_write_request_subscription_;
  std::unique_ptr<ClientDownloadRequest> last_client_download_request_;
  // The following 5 fields are used by PrepareBasicDownloadItem() function to
  // store attributes of the last download item. They can be modified
  // afterwards and the *item will return the new values.
  std::vector<GURL> url_chain_;
  GURL referrer_;
  base::FilePath tmp_path_;
  base::FilePath final_path_;
  std::string hash_;
  base::ScopedTempDir temp_dir_;
  extensions::TestEventRouter* test_event_router_;
  TestingProfileManager testing_profile_manager_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

using DownloadProtectionServiceTest = DownloadProtectionServiceTestBase;

class DeepScanningDownloadTest : public DownloadProtectionServiceTestBase,
                                 public ::testing::WithParamInterface<bool> {
 public:
  DeepScanningDownloadTest() {
    // Enable the feature early to prevent race condition trying to access
    // the enabled features set.  This happens for example when the history
    // service is started below.
    if (flag_enabled())
      EnableFeatures({enterprise_connectors::kEnterpriseConnectorsEnabled});
    else
      DisableFeatures({enterprise_connectors::kEnterpriseConnectorsEnabled});
  }

  bool flag_enabled() const { return GetParam(); }
};

// A test with the appropriate feature flags enabled to test the behavior for
// Enhanced Protection users.
class EnhancedProtectionDownloadTest
    : public DownloadProtectionServiceTestBase {
 public:
  EnhancedProtectionDownloadTest() {
    // Enable the feature early to prevent race condition trying to access
    // the enabled features set.  This happens for example when the history
    // service is started below.
    EnableFeatures({kEnhancedProtection, kDownloadRequestWithToken});
  }
};

void DownloadProtectionServiceTestBase::CheckClientDownloadReportCorruptArchive(
    ArchiveType type) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  NiceMockDownloadItem item;
  if (type == ZIP) {
    PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.zip"},  // url_chain
                             "http://www.google.com/",              // referrer
                             FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                             FILE_PATH_LITERAL("a.zip"));  // final_path
  } else if (type == DMG) {
    PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.dmg"},  // url_chain
                             "http://www.google.com/",              // referrer
                             FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                             FILE_PATH_LITERAL("a.dmg"));  // final_path
  }

  std::string file_contents = "corrupt archive file";
  ASSERT_EQ(
      static_cast<int>(file_contents.size()),
      base::WriteFile(tmp_path_, file_contents.data(), file_contents.size()));

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTestBase::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(HasClientDownloadRequest());
  EXPECT_EQ(0, GetClientDownloadRequest()->archived_binary_size());
  EXPECT_TRUE(GetClientDownloadRequest()->has_download_type());
  ClientDownloadRequest::DownloadType expected_type =
      type == ZIP
          ? ClientDownloadRequest_DownloadType_INVALID_ZIP
          : ClientDownloadRequest_DownloadType_MAC_ARCHIVE_FAILED_PARSING;
  EXPECT_EQ(expected_type, GetClientDownloadRequest()->download_type());
  ClearClientDownloadRequest();

  Mock::VerifyAndClearExpectations(sb_service_.get());
  Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
}

// TODO(crbug.com/721964): Create specific unit tests for
// check_client_download_request.*, download_url_sb_client.*, and
// ppapi_download_request.*.
TEST_F(DownloadProtectionServiceTest, CheckClientDownloadInvalidUrl) {
  NiceMockDownloadItem item;
  {
    PrepareBasicDownloadItem(&item,
                             std::vector<std::string>(),   // empty url_chain
                             "http://www.google.com/",     // referrer
                             FILE_PATH_LITERAL("a.tmp"),   // tmp_path
                             FILE_PATH_LITERAL("a.exe"));  // final_path
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
    EXPECT_FALSE(HasClientDownloadRequest());
    Mock::VerifyAndClearExpectations(&item);
  }

  {
    PrepareBasicDownloadItem(&item, {"file://www.google.com/"},  // url_chain
                             "http://www.google.com/",           // referrer
                             FILE_PATH_LITERAL("a.tmp"),         // tmp_path
                             FILE_PATH_LITERAL("a.exe"));        // final_path
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadNotABinary) {
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item,
                           std::vector<std::string>(),   // empty url_chain
                           "http://www.google.com/",     // referrer
                           FILE_PATH_LITERAL("a.tmp"),   // tmp_path
                           FILE_PATH_LITERAL("a.txt"));  // final_path
  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
  EXPECT_FALSE(HasClientDownloadRequest());
}

TEST_F(DownloadProtectionServiceTest,
       CheckClientDownloadWhitelistedUrlWithoutSampling) {
  // Response to any requests will be DANGEROUS.
  PrepareResponse(ClientDownloadResponse::DANGEROUS, net::HTTP_OK, net::OK);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item,
                           std::vector<std::string>(),   // empty url_chain
                           "",                           // referrer
                           FILE_PATH_LITERAL("a.tmp"),   // tmp_path
                           FILE_PATH_LITERAL("a.exe"));  // final_path

  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(3);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(3);

  // We should not get whilelist checks for other URLs than specified below.
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .Times(0);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(GURL("http://www.evil.com/bla.exe")))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(GURL("http://www.google.com/a.exe")))
      .WillRepeatedly(Return(true));

  // Set sample rate to 0 to prevent sampling.
  SetWhitelistedDownloadSampleRate(0);
  {
    // With no referrer and just the bad url, should be marked DANGEROUS.
    url_chain_.push_back(GURL("http://www.evil.com/bla.exe"));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::DANGEROUS));
    ASSERT_TRUE(HasClientDownloadRequest());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_url_whitelist());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_certificate_whitelist());
    ClearClientDownloadRequest();
  }
  {
    // Check that the referrer is not matched against the whitelist.
    referrer_ = GURL("http://www.google.com/");
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_TRUE(IsResult(DownloadCheckResult::DANGEROUS));
    ASSERT_TRUE(HasClientDownloadRequest());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_url_whitelist());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_certificate_whitelist());
    ClearClientDownloadRequest();
  }

  {
    // Redirect from a site shouldn't be checked either.
    url_chain_.insert(url_chain_.begin(),
                      GURL("http://www.google.com/redirect"));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::DANGEROUS));
    ASSERT_TRUE(HasClientDownloadRequest());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_url_whitelist());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_certificate_whitelist());
    ClearClientDownloadRequest();
  }
  {
    // Only if the final url is whitelisted should it be SAFE.
    url_chain_.push_back(GURL("http://www.google.com/a.exe"));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    // TODO(grt): Make the service produce the request even when the URL is
    // whitelisted.
    EXPECT_FALSE(HasClientDownloadRequest());
  }
}

TEST_F(DownloadProtectionServiceTest,
       CheckClientDownloadWhitelistedUrlWithSampling) {
  // Server responses "SAFE" to every requests coming from whitelisted
  // download.
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item,
                           std::vector<std::string>(),   // empty url_chain
                           "http://www.google.com/",     // referrer
                           FILE_PATH_LITERAL("a.tmp"),   // tmp_path
                           FILE_PATH_LITERAL("a.exe"));  // final_path
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(3);
  // Assume http://www.whitelist.com/a.exe is on the whitelist.
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .Times(0);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(GURL("http://www.whitelist.com/a.exe")))
      .WillRepeatedly(Return(true));
  url_chain_.push_back(GURL("http://www.whitelist.com/a.exe"));
  // Set sample rate to 1.00, so download_service_ will always send download
  // pings for whitelisted downloads.
  SetWhitelistedDownloadSampleRate(1.00);

  {
    // Case (1): is_extended_reporting && is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    SetExtendedReportingPreference(true);
    content::DownloadItemUtils::AttachInfo(
        &item, profile()->GetPrimaryOTRProfile(), nullptr);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
  {
    // Case (2): !is_extended_reporting && is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    SetExtendedReportingPreference(false);
    content::DownloadItemUtils::AttachInfo(
        &item, profile()->GetPrimaryOTRProfile(), nullptr);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
  {
    // Case (3): !is_extended_reporting && !is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
  {
    // Case (4): is_extended_reporting && !is_incognito &&
    //           Download matches URL whitelist.
    //           ClientDownloadRequest should be sent.
    SetExtendedReportingPreference(true);
    content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    ASSERT_TRUE(HasClientDownloadRequest());
    EXPECT_TRUE(GetClientDownloadRequest()->skipped_url_whitelist());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_certificate_whitelist());
    ClearClientDownloadRequest();
  }

  // Setup trusted and whitelisted certificates for test cases (5) and (6).
  scoped_refptr<net::X509Certificate> test_cert(
      ReadTestCertificate("test_cn.pem"));
  ASSERT_TRUE(test_cert.get());
  std::string test_cert_der(
      net::x509_util::CryptoBufferAsStringPiece(test_cert->cert_buffer()));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .WillRepeatedly(TrustSignature(test_cert_der));
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistString(_))
      .WillRepeatedly(Return(true));

  {
    // Case (6): is_extended_reporting && !is_incognito &&
    //           Download matches certificate whitelist.
    //           ClientDownloadRequest should be sent.
    content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);
    EXPECT_CALL(
        *sb_service_->mock_database_manager(),
        MatchDownloadWhitelistUrl(GURL("http://www.whitelist.com/a.exe")))
        .WillRepeatedly(Return(false));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    ASSERT_TRUE(HasClientDownloadRequest());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_url_whitelist());
    EXPECT_TRUE(GetClientDownloadRequest()->skipped_certificate_whitelist());
    ClearClientDownloadRequest();
  }
  {
    // Case (7): is_extended_reporting && !is_incognito &&
    //           Download matches both URL and certificate whitelists.
    //           ClientDownloadRequest should be sent.
    content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);
    EXPECT_CALL(
        *sb_service_->mock_database_manager(),
        MatchDownloadWhitelistUrl(GURL("http://www.whitelist.com/a.exe")))
        .WillRepeatedly(Return(true));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    ASSERT_TRUE(HasClientDownloadRequest());
    EXPECT_TRUE(GetClientDownloadRequest()->skipped_url_whitelist());
    // Since download matches URL whitelist and gets sampled, no need to
    // do certificate whitelist checking and sampling.
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_certificate_whitelist());
    ClearClientDownloadRequest();
  }
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadSampledFile) {
  // Server response will be discarded.
  PrepareResponse(ClientDownloadResponse::DANGEROUS, net::HTTP_OK, net::OK);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(
      &item,
      // Add paths so we can check they are properly removed.
      {"http://referrer.com/1/2", "http://referrer.com/3/4",
       "http://download.com/path/a.foobar_unknown_type"},
      "http://referrer.com/3/4",    // Referrer
      FILE_PATH_LITERAL("a.tmp"),   // tmp_path
      FILE_PATH_LITERAL("a.txt"));  // final_path, txt is set to SAMPLED_PING
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(1);

  // Set ping sample rate to 1.00 so download_service_ will always send a
  // "light" ping for unknown types if allowed.
  SetBinarySamplingProbability(1.0);

  {
    // Case (1): is_extended_reporting && is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    SetExtendedReportingPreference(true);
    content::DownloadItemUtils::AttachInfo(
        &item, profile()->GetPrimaryOTRProfile(), nullptr);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
  {
    // Case (2): is_extended_reporting && !is_incognito.
    //           A "light" ClientDownloadRequest should be sent.
    content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
    ASSERT_TRUE(HasClientDownloadRequest());

    // Verify it's a "light" ping, check that URLs don't have paths.
    auto* req = GetClientDownloadRequest();
    EXPECT_EQ(ClientDownloadRequest::SAMPLED_UNSUPPORTED_FILE,
              req->download_type());
    EXPECT_EQ(GURL(req->url()).GetOrigin().spec(), req->url());
    for (auto resource : req->resources()) {
      EXPECT_EQ(GURL(resource.url()).GetOrigin().spec(), resource.url());
      EXPECT_EQ(GURL(resource.referrer()).GetOrigin().spec(),
                resource.referrer());
    }
    ClearClientDownloadRequest();
  }
  {
    // Case (3): !is_extended_reporting && is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    SetExtendedReportingPreference(false);
    content::DownloadItemUtils::AttachInfo(
        &item, profile()->GetPrimaryOTRProfile(), nullptr);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
  {
    // Case (4): !is_extended_reporting && !is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadFetchFailed) {
  // HTTP request will fail.
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_INTERNAL_SERVER_ERROR,
                  net::ERR_FAILED);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));
  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadSuccess) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(8);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(8);
  std::string feedback_ping;
  std::string feedback_response;
  ClientDownloadResponse expected_response;

  {
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
  {
    // Invalid response should result in SAFE (default value in proto).
    ClientDownloadResponse invalid_response;
    sb_service_->test_url_loader_factory()->AddResponse(
        PPAPIDownloadRequest::GetDownloadRequestUrl().spec(),
        invalid_response.SerializePartialAsString());
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
    EXPECT_FALSE(DownloadFeedbackService::GetPingsForDownloadForTesting(
        item, &feedback_ping, &feedback_response));
  }
  {
    // If the response is dangerous the result should also be marked as
    // dangerous, and should not upload if not requested.
    PrepareResponse(ClientDownloadResponse::DANGEROUS, net::HTTP_OK, net::OK,
                    false /* upload_requested */);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_FALSE(DownloadFeedbackService::GetPingsForDownloadForTesting(
        item, &feedback_ping, &feedback_response));
    EXPECT_TRUE(IsResult(DownloadCheckResult::DANGEROUS));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
  {
    // If the response is dangerous and the server requests an upload,
    // we should upload.
    PrepareResponse(ClientDownloadResponse::DANGEROUS, net::HTTP_OK, net::OK,
                    true /* upload_requested */);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(DownloadFeedbackService::GetPingsForDownloadForTesting(
        item, &feedback_ping, &feedback_response));
    EXPECT_TRUE(IsResult(DownloadCheckResult::DANGEROUS));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
  {
    // If the response is uncommon the result should also be marked as uncommon.
    PrepareResponse(ClientDownloadResponse::UNCOMMON, net::HTTP_OK, net::OK,
                    true /* upload_requested */);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNCOMMON));
    EXPECT_TRUE(DownloadFeedbackService::GetPingsForDownloadForTesting(
        item, &feedback_ping, &feedback_response));
    ClientDownloadRequest decoded_request;
    EXPECT_TRUE(decoded_request.ParseFromString(feedback_ping));
    EXPECT_EQ(url_chain_.back().spec(), decoded_request.url());
    expected_response.set_verdict(ClientDownloadResponse::UNCOMMON);
    expected_response.set_upload(true);
    EXPECT_EQ(expected_response.SerializeAsString(), feedback_response);
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
  {
    // If the response is dangerous_host the result should also be marked as
    // dangerous_host.
    PrepareResponse(ClientDownloadResponse::DANGEROUS_HOST, net::HTTP_OK,
                    net::OK, true /* upload_requested */);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::DANGEROUS_HOST));
    EXPECT_TRUE(DownloadFeedbackService::GetPingsForDownloadForTesting(
        item, &feedback_ping, &feedback_response));
    expected_response.set_verdict(ClientDownloadResponse::DANGEROUS_HOST);
    expected_response.set_upload(true);
    EXPECT_EQ(expected_response.SerializeAsString(), feedback_response);
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
  {
    // If the response is POTENTIALLY_UNWANTED the result should also be marked
    // as POTENTIALLY_UNWANTED.
    PrepareResponse(ClientDownloadResponse::POTENTIALLY_UNWANTED, net::HTTP_OK,
                    net::OK);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::POTENTIALLY_UNWANTED));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
  {
    // If the response is UNKNOWN the result should also be marked as
    // UNKNOWN. And if the server requests an upload, we should upload.
    PrepareResponse(ClientDownloadResponse::UNKNOWN, net::HTTP_OK, net::OK,
                    true /* upload_requested */);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(DownloadFeedbackService::GetPingsForDownloadForTesting(
        item, &feedback_ping, &feedback_response));
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadHTTPS) {
  PrepareResponse(ClientDownloadResponse::DANGEROUS, net::HTTP_OK, net::OK);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(1);
  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(IsResult(DownloadCheckResult::DANGEROUS));
  EXPECT_TRUE(HasClientDownloadRequest());
  ClearClientDownloadRequest();
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadBlob) {
  PrepareResponse(ClientDownloadResponse::DANGEROUS, net::HTTP_OK, net::OK);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(
      &item, {"blob:http://www.evil.com/50b85f60-71e4-11e4-82f8-0800200c9a66"},
      "http://www.google.com/",     // referrer
      FILE_PATH_LITERAL("a.tmp"),   // tmp_path
      FILE_PATH_LITERAL("a.exe"));  // final_path

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(1);

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(IsResult(DownloadCheckResult::DANGEROUS));
  EXPECT_TRUE(HasClientDownloadRequest());
  ClearClientDownloadRequest();
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadData) {
  PrepareResponse(ClientDownloadResponse::DANGEROUS, net::HTTP_OK, net::OK);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(
      &item,
      {"data:text/html:base64,", "data:text/html:base64,blahblahblah",
       "data:application/octet-stream:base64,blahblah"},  // url_chain
      "data:text/html:base64,foobar",                     // referrer
      FILE_PATH_LITERAL("a.tmp"),                         // tmp_path
      FILE_PATH_LITERAL("a.exe"));                        // final_path

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(1);

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(IsResult(DownloadCheckResult::DANGEROUS));
  ASSERT_TRUE(HasClientDownloadRequest());
  const ClientDownloadRequest& request = *GetClientDownloadRequest();
  const char kExpectedUrl[] =
      "data:application/octet-stream:base64,"
      "ACBF6DFC6F907662F566CA0241DFE8690C48661F440BA1BBD0B86C582845CCC8";
  const char kExpectedRedirect1[] = "data:text/html:base64,";
  const char kExpectedRedirect2[] =
      "data:text/html:base64,"
      "620680767E15717A57DB11D94D1BEBD32B3344EBC5994DF4FB07B0D473F4EF6B";
  const char kExpectedReferrer[] =
      "data:text/html:base64,"
      "06E2C655B9F7130B508FFF86FD19B57E6BF1A1CFEFD6EFE1C3EB09FE24EF456A";
  EXPECT_EQ(hash_, request.digests().sha256());
  EXPECT_EQ(kExpectedUrl, request.url());
  EXPECT_EQ(3, request.resources_size());
  EXPECT_TRUE(RequestContainsResource(request,
                                      ClientDownloadRequest::DOWNLOAD_REDIRECT,
                                      kExpectedRedirect1, ""));
  EXPECT_TRUE(RequestContainsResource(request,
                                      ClientDownloadRequest::DOWNLOAD_REDIRECT,
                                      kExpectedRedirect2, ""));
  EXPECT_TRUE(RequestContainsResource(request,
                                      ClientDownloadRequest::DOWNLOAD_URL,
                                      kExpectedUrl, kExpectedReferrer));
  ClearClientDownloadRequest();
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadZip) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.zip"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.zip"));           // final_path

  // Write out a zip archive to the temporary file.
  base::ScopedTempDir zip_source_dir;
  ASSERT_TRUE(zip_source_dir.CreateUniqueTempDir());
  std::string file_contents = "dummy file";
  {
    // In this case, it only contains a text file.
    ASSERT_EQ(static_cast<int>(file_contents.size()),
              base::WriteFile(zip_source_dir.GetPath().Append(
                                  FILE_PATH_LITERAL("file.txt")),
                              file_contents.data(), file_contents.size()));
    ASSERT_TRUE(zip::Zip(zip_source_dir.GetPath(), tmp_path_, false));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
    EXPECT_FALSE(HasClientDownloadRequest());
    Mock::VerifyAndClearExpectations(sb_service_.get());
    Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
  }
  {
    // Now check with an executable in the zip file as well.
    ASSERT_EQ(static_cast<int>(file_contents.size()),
              base::WriteFile(zip_source_dir.GetPath().Append(
                                  FILE_PATH_LITERAL("file.exe")),
                              file_contents.data(), file_contents.size()));
    ASSERT_TRUE(zip::Zip(zip_source_dir.GetPath(), tmp_path_, false));
    EXPECT_CALL(*sb_service_->mock_database_manager(),
                MatchDownloadWhitelistUrl(_))
        .WillRepeatedly(Return(false));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    ASSERT_TRUE(HasClientDownloadRequest());
    const ClientDownloadRequest& request = *GetClientDownloadRequest();
    EXPECT_TRUE(request.has_download_type());
    EXPECT_EQ(ClientDownloadRequest_DownloadType_ZIPPED_EXECUTABLE,
              request.download_type());
    EXPECT_EQ(1, request.archived_binary_size());
    const ClientDownloadRequest_ArchivedBinary* archived_binary =
        GetRequestArchivedBinary(request, "file.exe");
    ASSERT_NE(nullptr, archived_binary);
    EXPECT_EQ(ClientDownloadRequest_DownloadType_WIN_EXECUTABLE,
              archived_binary->download_type());
    EXPECT_EQ(static_cast<int64_t>(file_contents.size()),
              archived_binary->length());
    ClearClientDownloadRequest();
    Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
  }
  {
    // If the response is dangerous the result should also be marked as
    // dangerous.
    PrepareResponse(ClientDownloadResponse::DANGEROUS, net::HTTP_OK, net::OK);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::DANGEROUS));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
    Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
  }
  {
    // Repeat the test with an archive inside the zip file in addition to the
    // executable.
    ASSERT_EQ(static_cast<int>(file_contents.size()),
              base::WriteFile(zip_source_dir.GetPath().Append(
                                  FILE_PATH_LITERAL("file.rar")),
                              file_contents.data(), file_contents.size()));
    ASSERT_TRUE(zip::Zip(zip_source_dir.GetPath(), tmp_path_, false));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    ASSERT_TRUE(HasClientDownloadRequest());
    EXPECT_EQ(2, GetClientDownloadRequest()->archived_binary_size());
    EXPECT_TRUE(GetClientDownloadRequest()->has_download_type());
    EXPECT_EQ(ClientDownloadRequest_DownloadType_ZIPPED_EXECUTABLE,
              GetClientDownloadRequest()->download_type());
    ClearClientDownloadRequest();
    Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
  }
  {
    // Repeat the test with just the archive inside the zip file.
    ASSERT_TRUE(
        base::DeleteFile(zip_source_dir.GetPath().AppendASCII("file.exe")));
    ASSERT_TRUE(zip::Zip(zip_source_dir.GetPath(), tmp_path_, false));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    ASSERT_TRUE(HasClientDownloadRequest());
    EXPECT_EQ(1, GetClientDownloadRequest()->archived_binary_size());
    EXPECT_TRUE(GetClientDownloadRequest()->has_download_type());
    EXPECT_EQ(ClientDownloadRequest_DownloadType_ZIPPED_ARCHIVE,
              GetClientDownloadRequest()->download_type());
    ClearClientDownloadRequest();
    Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
  }
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadReportCorruptZip) {
  CheckClientDownloadReportCorruptArchive(ZIP);
}

#if defined(OS_MAC)
TEST_F(DownloadProtectionServiceTest, CheckClientDownloadReportCorruptDmg) {
  CheckClientDownloadReportCorruptArchive(DMG);
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadReportValidDmg) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  base::FilePath test_dmg;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dmg));
  test_dmg = test_dmg.AppendASCII("safe_browsing")
                 .AppendASCII("mach_o")
                 .AppendASCII("signed-archive.dmg");

  NiceMockDownloadItem item;
  PrepareBasicDownloadItemWithFullPaths(
      &item, {"http://www.evil.com/a.dmg"},                     // url_chain
      "http://www.google.com/",                                 // referrer
      test_dmg,                                                 // tmp_path
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("a.dmg")));  // final_path

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(HasClientDownloadRequest());
  EXPECT_TRUE(GetClientDownloadRequest()->archive_valid());

  ClearClientDownloadRequest();

  Mock::VerifyAndClearExpectations(sb_service_.get());
  Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
}

// Tests that signatures get recorded and uploaded for signed DMGs.
TEST_F(DownloadProtectionServiceTest,
       CheckClientDownloadReportDmgWithSignature) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  base::FilePath signed_dmg;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &signed_dmg));
  signed_dmg = signed_dmg.AppendASCII("safe_browsing")
                   .AppendASCII("mach_o")
                   .AppendASCII("signed-archive.dmg");

  NiceMockDownloadItem item;
  PrepareBasicDownloadItemWithFullPaths(
      &item, {"http://www.evil.com/a.dmg"},                     // url_chain
      "http://www.google.com/",                                 // referrer
      signed_dmg,                                               // tmp_path
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("a.dmg")));  // final_path

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(HasClientDownloadRequest());
  EXPECT_TRUE(GetClientDownloadRequest()->has_udif_code_signature());
  EXPECT_EQ(2215u, GetClientDownloadRequest()->udif_code_signature().length());

  base::FilePath signed_dmg_signature;
  EXPECT_TRUE(
      base::PathService::Get(chrome::DIR_TEST_DATA, &signed_dmg_signature));
  signed_dmg_signature = signed_dmg_signature.AppendASCII("safe_browsing")
                             .AppendASCII("mach_o")
                             .AppendASCII("signed-archive-signature.data");

  std::string signature;
  base::ReadFileToString(signed_dmg_signature, &signature);
  EXPECT_EQ(2215u, signature.length());
  EXPECT_EQ(signature, GetClientDownloadRequest()->udif_code_signature());

  ClearClientDownloadRequest();

  Mock::VerifyAndClearExpectations(sb_service_.get());
  Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
}

// Tests that no signature gets recorded and uploaded for unsigned DMGs.
TEST_F(DownloadProtectionServiceTest,
       CheckClientDownloadReportDmgWithoutSignature) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  base::FilePath unsigned_dmg;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &unsigned_dmg));
  unsigned_dmg = unsigned_dmg.AppendASCII("safe_browsing")
                     .AppendASCII("dmg")
                     .AppendASCII("data")
                     .AppendASCII("mach_o_in_dmg.dmg");

  NiceMockDownloadItem item;
  PrepareBasicDownloadItemWithFullPaths(
      &item, {"http://www.evil.com/a.dmg"},                     // url_chain
      "http://www.google.com/",                                 // referrer
      unsigned_dmg,                                             // tmp_path
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("a.dmg")));  // final_path

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(HasClientDownloadRequest());
  EXPECT_TRUE(GetClientDownloadRequest()->archive_valid());
  EXPECT_FALSE(GetClientDownloadRequest()->has_udif_code_signature());

  ClearClientDownloadRequest();

  Mock::VerifyAndClearExpectations(sb_service_.get());
  Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
}

// Test that downloaded files with no disk image extension that have a 'koly'
// trailer are treated as disk images and processed accordingly.
TEST_F(DownloadProtectionServiceTest,
       CheckClientDownloadReportDmgWithoutExtension) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  base::FilePath test_data;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data));
  test_data = test_data.AppendASCII("safe_browsing")
                  .AppendASCII("dmg")
                  .AppendASCII("data")
                  .AppendASCII("mach_o_in_dmg.txt");

  NiceMockDownloadItem item;
  PrepareBasicDownloadItemWithFullPaths(
      &item, {"http://www.evil.com/a.dmg"},                     // url_chain
      "http://www.google.com/",                                 // referrer
      test_data,                                                // tmp_path
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("a.dmg")));  // final_path

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(HasClientDownloadRequest());
  EXPECT_TRUE(GetClientDownloadRequest()->archive_valid());
  ClearClientDownloadRequest();

  Mock::VerifyAndClearExpectations(sb_service_.get());
  Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
}

// Demonstrate that a .dmg file whose a) extension has been changed to .txt and
// b) 'koly' signature has been removed is not processed as a disk image.
TEST_F(DownloadProtectionServiceTest, CheckClientDownloadReportDmgWithoutKoly) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  base::FilePath test_data;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data));
  test_data = test_data.AppendASCII("safe_browsing")
                  .AppendASCII("dmg")
                  .AppendASCII("data")
                  .AppendASCII("mach_o_in_dmg_no_koly_signature.txt");

  NiceMockDownloadItem item;
  PrepareBasicDownloadItemWithFullPaths(
      &item, {"http://www.evil.com/a.dmg"},                     // url_chain
      "http://www.google.com/",                                 // referrer
      test_data,                                                // tmp_path
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("a.dmg")));  // final_path

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(HasClientDownloadRequest());
  EXPECT_FALSE(GetClientDownloadRequest()->archive_valid());
  ClearClientDownloadRequest();

  Mock::VerifyAndClearExpectations(sb_service_.get());
  Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
}

// Test that a large DMG (size equals max value of 64 bit signed int) is not
// unpacked for binary feature analysis.
TEST_F(DownloadProtectionServiceTest, CheckClientDownloadReportLargeDmg) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  base::FilePath unsigned_dmg;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &unsigned_dmg));
  unsigned_dmg = unsigned_dmg.AppendASCII("safe_browsing")
                     .AppendASCII("dmg")
                     .AppendASCII("data")
                     .AppendASCII("mach_o_in_dmg.dmg");

  NiceMockDownloadItem item;
  PrepareBasicDownloadItemWithFullPaths(
      &item, {"http://www.evil.com/a.dmg"},                     // url_chain
      "http://www.google.com/",                                 // referrer
      unsigned_dmg,                                             // tmp_path
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("a.dmg")));  // final_path

  // Set the max file size to unpack to 0, so that this DMG is now "too large"
  std::unique_ptr<DownloadFileTypeConfig> config = policies_.DuplicateConfig();
  for (int i = 0; i < config->file_types_size(); i++) {
    if (config->file_types(i).extension() == "dmg") {
      for (int j = 0; j < config->file_types(i).platform_settings_size(); j++) {
        config->mutable_file_types(i)
            ->mutable_platform_settings(j)
            ->set_max_file_size_to_analyze(0);
      }
    }
  }
  policies_.SwapConfig(config);

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(HasClientDownloadRequest());
  // Even though the test DMG is valid, it is not unpacked due to its large
  // size.
  EXPECT_FALSE(GetClientDownloadRequest()->archive_valid());
  ClearClientDownloadRequest();

  Mock::VerifyAndClearExpectations(sb_service_.get());
  Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
}

// Verifies the results of DMG analysis end-to-end.
TEST_F(DownloadProtectionServiceTest, DMGAnalysisEndToEnd) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  base::FilePath dmg;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &dmg));
  dmg = dmg.AppendASCII("safe_browsing")
            .AppendASCII("dmg")
            .AppendASCII("data")
            .AppendASCII("mach_o_in_dmg.dmg");

  NiceMockDownloadItem item;
  PrepareBasicDownloadItemWithFullPaths(
      &item, {"http://www.evil.com/a.dmg"},                     // url_chain
      "http://www.google.com/",                                 // referrer
      dmg,                                                      // tmp_path
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("a.dmg")));  // final_path

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(HasClientDownloadRequest());

  auto* request = GetClientDownloadRequest();

  EXPECT_TRUE(request->archive_valid());
  EXPECT_FALSE(request->has_udif_code_signature());
  EXPECT_EQ(ClientDownloadRequest_DownloadType_MAC_EXECUTABLE,
            request->download_type());

  ASSERT_EQ(2, request->archived_binary().size());
  for (const auto& binary : request->archived_binary()) {
    EXPECT_FALSE(binary.file_basename().empty());
    EXPECT_EQ(ClientDownloadRequest_DownloadType_MAC_EXECUTABLE,
              binary.download_type());
    ASSERT_TRUE(binary.has_digests());
    EXPECT_TRUE(binary.digests().has_sha256());
    EXPECT_TRUE(binary.has_length());
    ASSERT_TRUE(binary.has_image_headers());
    ASSERT_FALSE(binary.image_headers().mach_o_headers().empty());
    EXPECT_FALSE(
        binary.image_headers().mach_o_headers().Get(0).mach_header().empty());
    EXPECT_FALSE(
        binary.image_headers().mach_o_headers().Get(0).load_commands().empty());
  }

  ASSERT_EQ(1, request->detached_code_signature().size());
  EXPECT_FALSE(request->detached_code_signature().Get(0).file_name().empty());
  EXPECT_FALSE(request->detached_code_signature().Get(0).contents().empty());

  ClearClientDownloadRequest();

  Mock::VerifyAndClearExpectations(sb_service_.get());
  Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
}

#endif  // OS_MAC

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadValidateRequest) {
#if defined(OS_MAC)
  std::string download_file_path("ftp://www.google.com/bla.dmg");
#else
  std::string download_file_path("ftp://www.google.com/bla.exe");
#endif  //  OS_MAC

  NiceMockDownloadItem item;
#if defined(OS_MAC)
  PrepareBasicDownloadItem(
      &item, {"http://www.google.com/", download_file_path},  // url_chain
      "http://www.google.com/",                               // referrer
      FILE_PATH_LITERAL("bla.tmp"),                           // tmp_path
      FILE_PATH_LITERAL("bla.dmg"));                          // final_path
#else
  PrepareBasicDownloadItem(
      &item, {"http://www.google.com/", download_file_path},  // url_chain
      "http://www.google.com/",                               // referrer
      FILE_PATH_LITERAL("bla.tmp"),                           // tmp_path
      FILE_PATH_LITERAL("bla.exe"));                          // final_path
#endif  // OS_MAC

  std::string remote_address = "10.11.12.13";
  EXPECT_CALL(item, GetRemoteAddress()).WillRepeatedly(Return(remote_address));

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
#if !defined(OS_MAC)
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .WillOnce(SetCertificateContents("dummy cert data"));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .WillOnce(SetDosHeaderContents("dummy dos header"));
#endif  // OS_MAC

  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));

  // Wait until processing is finished.
  run_loop.Run();

  ASSERT_TRUE(HasClientDownloadRequest());
  const ClientDownloadRequest* request = GetClientDownloadRequest();
  EXPECT_EQ(download_file_path, request->url());
  EXPECT_EQ(hash_, request->digests().sha256());
  EXPECT_EQ(item.GetReceivedBytes(), request->length());
  EXPECT_EQ(item.HasUserGesture(), request->user_initiated());
  EXPECT_TRUE(RequestContainsServerIp(*request, remote_address));
  EXPECT_EQ(2, request->resources_size());
  EXPECT_TRUE(RequestContainsResource(*request,
                                      ClientDownloadRequest::DOWNLOAD_REDIRECT,
                                      "http://www.google.com/", ""));
  EXPECT_TRUE(RequestContainsResource(*request,
                                      ClientDownloadRequest::DOWNLOAD_URL,
                                      download_file_path, referrer_.spec()));
  ASSERT_TRUE(request->has_signature());
#if !defined(OS_MAC)
  ASSERT_EQ(1, request->signature().certificate_chain_size());
  const ClientDownloadRequest_CertificateChain& chain =
      request->signature().certificate_chain(0);
  ASSERT_EQ(1, chain.element_size());
  EXPECT_EQ("dummy cert data", chain.element(0).certificate());
  EXPECT_TRUE(request->has_image_headers());
  const ClientDownloadRequest_ImageHeaders& headers = request->image_headers();
  EXPECT_TRUE(headers.has_pe_headers());
  EXPECT_TRUE(headers.pe_headers().has_dos_header());
  EXPECT_EQ("dummy dos header", headers.pe_headers().dos_header());
#endif  // OS_MAC
}

// Similar to above, but with an unsigned binary.
TEST_F(DownloadProtectionServiceTest,
       CheckClientDownloadValidateRequestNoSignature) {
#if defined(OS_MAC)
  std::string download_file_path("ftp://www.google.com/bla.dmg");
#else
  std::string download_file_path("ftp://www.google.com/bla.exe");
#endif  // OS_MAC

  NiceMockDownloadItem item;
#if defined(OS_MAC)
  PrepareBasicDownloadItem(
      &item, {"http://www.google.com/", download_file_path},  // url_chain
      "http://www.google.com/",                               // referrer
      FILE_PATH_LITERAL("bla.tmp"),                           // tmp_path
      FILE_PATH_LITERAL("bla.dmg"));                          // final_path
#else
  PrepareBasicDownloadItem(
      &item, {"http://www.google.com/", download_file_path},  // url_chain
      "http://www.google.com/",                               // referrer
      FILE_PATH_LITERAL("bla.tmp"),                           // tmp_path
      FILE_PATH_LITERAL("bla.exe"));                          // final_path
#endif  // OS_MAC

  std::string remote_address = "10.11.12.13";
  EXPECT_CALL(item, GetRemoteAddress()).WillRepeatedly(Return(remote_address));
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
#if !defined(OS_MAC)
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));
#endif  // OS_MAC

  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));

  // Wait until processing is finished.
  run_loop.Run();

  ASSERT_TRUE(HasClientDownloadRequest());
  const ClientDownloadRequest* request = GetClientDownloadRequest();
  EXPECT_EQ(download_file_path, request->url());
  EXPECT_EQ(hash_, request->digests().sha256());
  EXPECT_EQ(item.GetReceivedBytes(), request->length());
  EXPECT_EQ(item.HasUserGesture(), request->user_initiated());
  EXPECT_TRUE(RequestContainsServerIp(*request, remote_address));
  EXPECT_EQ(2, request->resources_size());
  EXPECT_TRUE(RequestContainsResource(*request,
                                      ClientDownloadRequest::DOWNLOAD_REDIRECT,
                                      "http://www.google.com/", ""));
  EXPECT_TRUE(RequestContainsResource(*request,
                                      ClientDownloadRequest::DOWNLOAD_URL,
                                      download_file_path, referrer_.spec()));
  ASSERT_TRUE(request->has_signature());
  EXPECT_EQ(0, request->signature().certificate_chain_size());
}

// Similar to above, but with tab history.
TEST_F(DownloadProtectionServiceTest,
       CheckClientDownloadValidateRequestTabHistory) {
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(
      &item,
      {"http://www.google.com/", "http://www.google.com/bla.exe"},  // url_chain
      "http://www.google.com/",                                     // referrer
      FILE_PATH_LITERAL("bla.tmp"),                                 // tmp_path
      FILE_PATH_LITERAL("bla.exe"));  // final_path

  GURL tab_url("http://tab.com/final");
  GURL tab_referrer("http://tab.com/referrer");
  std::string remote_address = "10.11.12.13";
  EXPECT_CALL(item, GetTabUrl()).WillRepeatedly(ReturnRef(tab_url));
  EXPECT_CALL(item, GetTabReferrerUrl())
      .WillRepeatedly(ReturnRef(tab_referrer));
  EXPECT_CALL(item, GetRemoteAddress()).WillRepeatedly(Return(remote_address));
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .WillRepeatedly(SetCertificateContents("dummy cert data"));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .WillRepeatedly(SetDosHeaderContents("dummy dos header"));

  // First test with no history match for the tab URL.
  {
    RunLoop interceptor_run_loop;

    std::string upload_data;
    sb_service_->test_url_loader_factory()->SetInterceptor(
        base::BindLambdaForTesting(
            [&](const network::ResourceRequest& request) {
              upload_data = network::GetUploadData(request);
              if (!upload_data.empty())
                interceptor_run_loop.Quit();
            }));

    PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));

    interceptor_run_loop.Run();

    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();

    ClientDownloadRequest request;
    EXPECT_TRUE(request.ParseFromString(upload_data));
    EXPECT_EQ("http://www.google.com/bla.exe", request.url());
    EXPECT_EQ(hash_, request.digests().sha256());
    EXPECT_EQ(item.GetReceivedBytes(), request.length());
    EXPECT_EQ(item.HasUserGesture(), request.user_initiated());
    EXPECT_TRUE(RequestContainsServerIp(request, remote_address));
    EXPECT_EQ(3, request.resources_size());
    EXPECT_TRUE(RequestContainsResource(
        request, ClientDownloadRequest::DOWNLOAD_REDIRECT,
        "http://www.google.com/", ""));
    EXPECT_TRUE(RequestContainsResource(
        request, ClientDownloadRequest::DOWNLOAD_URL,
        "http://www.google.com/bla.exe", referrer_.spec()));
    EXPECT_TRUE(RequestContainsResource(request, ClientDownloadRequest::TAB_URL,
                                        tab_url.spec(), tab_referrer.spec()));
    EXPECT_TRUE(request.has_signature());
    ASSERT_EQ(1, request.signature().certificate_chain_size());
    const ClientDownloadRequest_CertificateChain& chain =
        request.signature().certificate_chain(0);
    ASSERT_EQ(1, chain.element_size());
    EXPECT_EQ("dummy cert data", chain.element(0).certificate());
    EXPECT_TRUE(request.has_image_headers());
    const ClientDownloadRequest_ImageHeaders& headers = request.image_headers();
    EXPECT_TRUE(headers.has_pe_headers());
    EXPECT_TRUE(headers.pe_headers().has_dos_header());
    EXPECT_EQ("dummy dos header", headers.pe_headers().dos_header());

    sb_service_->test_url_loader_factory()->SetInterceptor(
        network::TestURLLoaderFactory::Interceptor());

    // Simulate the request finishing.
    run_loop.Run();
  }

  // Now try with a history match.
  {
    RunLoop interceptor_run_loop;

    std::string upload_data;
    sb_service_->test_url_loader_factory()->SetInterceptor(
        base::BindLambdaForTesting(
            [&](const network::ResourceRequest& request) {
              upload_data = network::GetUploadData(request);
              if (!upload_data.empty())
                interceptor_run_loop.Quit();
            }));

    history::RedirectList redirects;
    redirects.push_back(GURL("http://tab.com/ref1"));
    redirects.push_back(GURL("http://tab.com/ref2"));
    redirects.push_back(tab_url);
    HistoryServiceFactory::GetForProfile(profile(),
                                         ServiceAccessType::EXPLICIT_ACCESS)
        ->AddPage(tab_url, base::Time::Now(),
                  reinterpret_cast<history::ContextID>(1), 0, GURL(), redirects,
                  ui::PAGE_TRANSITION_TYPED, history::SOURCE_BROWSED, false,
                  false);

    PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));

    interceptor_run_loop.Run();

    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
    ClientDownloadRequest request;
    EXPECT_TRUE(request.ParseFromString(upload_data));
    EXPECT_EQ("http://www.google.com/bla.exe", request.url());
    EXPECT_EQ(hash_, request.digests().sha256());
    EXPECT_EQ(item.GetReceivedBytes(), request.length());
    EXPECT_EQ(item.HasUserGesture(), request.user_initiated());
    EXPECT_TRUE(RequestContainsServerIp(request, remote_address));
    EXPECT_EQ(5, request.resources_size());
    EXPECT_TRUE(RequestContainsResource(
        request, ClientDownloadRequest::DOWNLOAD_REDIRECT,
        "http://www.google.com/", ""));
    EXPECT_TRUE(RequestContainsResource(
        request, ClientDownloadRequest::DOWNLOAD_URL,
        "http://www.google.com/bla.exe", referrer_.spec()));
    EXPECT_TRUE(RequestContainsResource(request,
                                        ClientDownloadRequest::TAB_REDIRECT,
                                        "http://tab.com/ref1", ""));
    EXPECT_TRUE(RequestContainsResource(request,
                                        ClientDownloadRequest::TAB_REDIRECT,
                                        "http://tab.com/ref2", ""));
    EXPECT_TRUE(RequestContainsResource(request, ClientDownloadRequest::TAB_URL,
                                        tab_url.spec(), tab_referrer.spec()));
    EXPECT_TRUE(request.has_signature());
    ASSERT_EQ(1, request.signature().certificate_chain_size());
    const ClientDownloadRequest_CertificateChain& chain =
        request.signature().certificate_chain(0);
    ASSERT_EQ(1, chain.element_size());
    EXPECT_EQ("dummy cert data", chain.element(0).certificate());

    // Simulate the request finishing.
    run_loop.Run();
  }
}

TEST_F(DownloadProtectionServiceTest, TestCheckDownloadUrl) {
  std::vector<GURL> url_chain;
  url_chain.push_back(GURL("http://www.google.com/"));
  url_chain.push_back(GURL("http://www.google.com/bla.exe"));
  GURL referrer("http://www.google.com/");
  std::string hash = "hash";

  NiceMockDownloadItem item;
  EXPECT_CALL(item, GetURL()).WillRepeatedly(ReturnRef(url_chain.back()));
  EXPECT_CALL(item, GetUrlChain()).WillRepeatedly(ReturnRef(url_chain));
  EXPECT_CALL(item, GetReferrerUrl()).WillRepeatedly(ReturnRef(referrer));
  EXPECT_CALL(item, GetHash()).WillRepeatedly(ReturnRef(hash));

  {
    // CheckDownloadURL returns immediately which means the client object
    // callback will never be called.  Nevertheless the callback provided
    // to CheckClientDownload must still be called.
    EXPECT_CALL(*sb_service_->mock_database_manager(),
                CheckDownloadUrl(ContainerEq(url_chain), NotNull()))
        .WillOnce(Return(true));
    RunLoop run_loop;
    download_service_->CheckDownloadUrl(
        &item, base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                              base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    Mock::VerifyAndClearExpectations(sb_service_.get());
  }
  {
    EXPECT_CALL(*sb_service_->mock_database_manager(),
                CheckDownloadUrl(ContainerEq(url_chain), NotNull()))
        .WillOnce(
            DoAll(CheckDownloadUrlDone(SB_THREAT_TYPE_SAFE), Return(false)));
    RunLoop run_loop;
    download_service_->CheckDownloadUrl(
        &item, base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                              base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    Mock::VerifyAndClearExpectations(sb_service_.get());
  }
  {
    EXPECT_CALL(*sb_service_->mock_database_manager(),
                CheckDownloadUrl(ContainerEq(url_chain), NotNull()))
        .WillOnce(DoAll(CheckDownloadUrlDone(SB_THREAT_TYPE_URL_MALWARE),
                        Return(false)));
    RunLoop run_loop;
    download_service_->CheckDownloadUrl(
        &item, base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                              base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    Mock::VerifyAndClearExpectations(sb_service_.get());
  }
  {
    EXPECT_CALL(*sb_service_->mock_database_manager(),
                CheckDownloadUrl(ContainerEq(url_chain), NotNull()))
        .WillOnce(DoAll(CheckDownloadUrlDone(SB_THREAT_TYPE_URL_BINARY_MALWARE),
                        Return(false)));
    RunLoop run_loop;
    download_service_->CheckDownloadUrl(
        &item, base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                              base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::DANGEROUS));
  }
}

TEST_F(DownloadProtectionServiceTest,
       TestCheckDownloadUrlOnPolicyWhitelistedDownload) {
  AddDomainToEnterpriseWhitelist("example.com");

  // Prepares download item that its download url (last url in url chain)
  // matches enterprise whitelist.
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(
      &item,
      {"https://landingpage.com", "http://example.com/download_page/a.exe"},
      "http://referrer.com",        // referrer
      FILE_PATH_LITERAL("a.tmp"),   // tmp_path
      FILE_PATH_LITERAL("a.exe"));  // final_path
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  content::DownloadItemUtils::AttachInfo(&item, profile(), web_contents.get());
  EXPECT_CALL(*sb_service_->mock_database_manager(), CheckDownloadUrl(_, _))
      .Times(0);
  RunLoop run_loop;
  download_service_->CheckDownloadUrl(
      &item, base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));

  // Prepares download item that other url in the url chain matches enterprise
  // whitelist.
  NiceMockDownloadItem item2;
  PrepareBasicDownloadItem(
      &item2, {"https://example.com/landing", "http://otherdomain.com/a.exe"},
      "http://referrer.com",        // referrer
      FILE_PATH_LITERAL("a.tmp"),   // tmp_path
      FILE_PATH_LITERAL("a.exe"));  // final_path
  content::DownloadItemUtils::AttachInfo(&item2, profile(), web_contents.get());
  EXPECT_CALL(*sb_service_->mock_database_manager(), CheckDownloadUrl(_, _))
      .Times(0);
  RunLoop run_loop2;
  download_service_->CheckDownloadUrl(
      &item2, base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                             base::Unretained(this), run_loop2.QuitClosure()));
  run_loop2.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
}

TEST_F(DownloadProtectionServiceTest, TestDownloadRequestTimeout) {
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/bla.exe"},  // url_chain
                           "http://www.google.com/",                // referrer
                           FILE_PATH_LITERAL("a.tmp"),              // tmp_path
                           FILE_PATH_LITERAL("a.exe"));  // final_path

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

  download_service_->download_request_timeout_ms_ = 10;
  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));

  // The request should time out because the HTTP request hasn't returned
  // anything yet.
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
  EXPECT_TRUE(HasClientDownloadRequest());
  ClearClientDownloadRequest();
}

TEST_F(DownloadProtectionServiceTest, TestDownloadItemDestroyed) {
  {
    NiceMockDownloadItem item;
    PrepareBasicDownloadItem(&item,
                             {"http://www.evil.com/bla.exe"},  // url_chain
                             "http://www.google.com/",         // referrer
                             FILE_PATH_LITERAL("a.tmp"),       // tmp_path
                             FILE_PATH_LITERAL("a.exe"));      // final_path
    GURL tab_url("http://www.google.com/tab");
    EXPECT_CALL(item, GetTabUrl()).WillRepeatedly(ReturnRef(tab_url));
    EXPECT_CALL(*sb_service_->mock_database_manager(),
                MatchDownloadWhitelistUrl(_))
        .WillRepeatedly(Return(false));

    // Expects that MockDownloadItem will go out of scope while asynchronous
    // processing is checking whitelist, and thus will return after whitelist
    // check rather than continuing to process the download, since
    // OnDownloadDestroyed will be called to terminate the processing.
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
        .Times(0);
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
        .Times(0);

    download_service_->CheckClientDownload(
        &item, base::BindRepeating(
                   &DownloadProtectionServiceTest::SyncCheckDoneCallback,
                   base::Unretained(this)));
    // MockDownloadItem going out of scope triggers the OnDownloadDestroyed
    // notification.
  }

  // When download is destroyed, no need to check for client download request
  // result.
  EXPECT_TRUE(has_result_);
  EXPECT_FALSE(HasClientDownloadRequest());
}

TEST_F(DownloadProtectionServiceTest,
       TestDownloadItemDestroyedDuringWhitelistCheck) {
  std::unique_ptr<NiceMockDownloadItem> item(new NiceMockDownloadItem);
  PrepareBasicDownloadItem(item.get(),
                           {"http://www.evil.com/bla.exe"},  // url_chain
                           "http://www.google.com/",         // referrer
                           FILE_PATH_LITERAL("a.tmp"),       // tmp_path
                           FILE_PATH_LITERAL("a.exe"));      // final_path
  GURL tab_url("http://www.google.com/tab");
  EXPECT_CALL(*item, GetTabUrl()).WillRepeatedly(ReturnRef(tab_url));

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Invoke([&item](const GURL&) {
        item.reset();
        return false;
      }));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(0);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(0);

  download_service_->CheckClientDownload(
      item.get(),
      base::BindRepeating(&DownloadProtectionServiceTest::SyncCheckDoneCallback,
                          base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(has_result_);
  EXPECT_FALSE(HasClientDownloadRequest());
}

namespace {

class MockPageNavigator : public content::PageNavigator {
 public:
  MOCK_METHOD1(OpenURL, content::WebContents*(const content::OpenURLParams&));
};

// A custom matcher that matches a OpenURLParams value with a url with a query
// parameter patching |value|.
MATCHER_P(OpenURLParamsWithContextValue, value, "") {
  std::string query_value;
  return net::GetValueForKeyInQuery(arg.url, "ctx", &query_value) &&
         query_value == value;
}

}  // namespace

// ShowDetailsForDownload() should open a URL showing more information about why
// a download was flagged by SafeBrowsing. The URL should have a &ctx= parameter
// whose value is the DownloadDangerType.
TEST_F(DownloadProtectionServiceTest, ShowDetailsForDownloadHasContext) {
  StrictMock<MockPageNavigator> mock_page_navigator;
  StrictMock<download::MockDownloadItem> mock_download_item;

  EXPECT_CALL(mock_download_item, GetDangerType())
      .WillOnce(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST));
  EXPECT_CALL(mock_page_navigator, OpenURL(OpenURLParamsWithContextValue("7")));

  download_service_->ShowDetailsForDownload(&mock_download_item,
                                            &mock_page_navigator);
}

TEST_F(DownloadProtectionServiceTest, GetAndSetDownloadPingToken) {
  NiceMockDownloadItem item;
  EXPECT_TRUE(DownloadProtectionService::GetDownloadPingToken(&item).empty());
  std::string token = "download_ping_token";
  DownloadProtectionService::SetDownloadPingToken(&item, token);
  EXPECT_EQ(token, DownloadProtectionService::GetDownloadPingToken(&item));

  DownloadProtectionService::SetDownloadPingToken(&item, std::string());
  EXPECT_TRUE(DownloadProtectionService::GetDownloadPingToken(&item).empty());
}

TEST_F(DownloadProtectionServiceTest, PPAPIDownloadRequest_Unsupported) {
  base::FilePath default_file_path(FILE_PATH_LITERAL("/foo/bar/test.jpg"));
  std::vector<base::FilePath::StringType> alternate_extensions{
      FILE_PATH_LITERAL(".jpeg")};
  download_service_->CheckPPAPIDownloadRequest(
      GURL("http://example.com/foo"), GURL(), nullptr, default_file_path,
      alternate_extensions, profile(),
      base::BindOnce(&DownloadProtectionServiceTest::SyncCheckDoneCallback,
                     base::Unretained(this)));
  ASSERT_TRUE(IsResult(DownloadCheckResult::SAFE));
}

TEST_F(DownloadProtectionServiceTest, PPAPIDownloadRequest_SupportedDefault) {
  base::FilePath default_file_path(FILE_PATH_LITERAL("/foo/bar/test.crx"));
  std::vector<base::FilePath::StringType> alternate_extensions;
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  struct {
    ClientDownloadResponse::Verdict verdict;
    DownloadCheckResult expected_result;
  } kExpectedResults[] = {
      {ClientDownloadResponse::SAFE, DownloadCheckResult::SAFE},
      {ClientDownloadResponse::DANGEROUS, DownloadCheckResult::DANGEROUS},
      {ClientDownloadResponse::UNCOMMON, DownloadCheckResult::UNCOMMON},
      {ClientDownloadResponse::DANGEROUS_HOST,
       DownloadCheckResult::DANGEROUS_HOST},
      {ClientDownloadResponse::POTENTIALLY_UNWANTED,
       DownloadCheckResult::POTENTIALLY_UNWANTED},
      {ClientDownloadResponse::UNKNOWN, DownloadCheckResult::UNKNOWN},
  };

  for (const auto& test_case : kExpectedResults) {
    sb_service_->test_url_loader_factory()->ClearResponses();
    PrepareResponse(test_case.verdict, net::HTTP_OK, net::OK);
    SetExtendedReportingPreference(true);
    RunLoop run_loop;
    download_service_->CheckPPAPIDownloadRequest(
        GURL("http://example.com/foo"), GURL(), nullptr, default_file_path,
        alternate_extensions, profile(),
        base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    ASSERT_TRUE(IsResult(test_case.expected_result));
    ASSERT_EQ(ChromeUserPopulation::EXTENDED_REPORTING,
              GetClientDownloadRequest()->population().user_population());
  }
}

TEST_F(DownloadProtectionServiceTest, PPAPIDownloadRequest_SupportedAlternate) {
  base::FilePath default_file_path(FILE_PATH_LITERAL("/foo/bar/test.txt"));
  std::vector<base::FilePath::StringType> alternate_extensions{
      FILE_PATH_LITERAL(".tmp"), FILE_PATH_LITERAL(".crx")};
  PrepareResponse(ClientDownloadResponse::DANGEROUS, net::HTTP_OK, net::OK);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  SetExtendedReportingPreference(false);
  RunLoop run_loop;
  download_service_->CheckPPAPIDownloadRequest(
      GURL("http://example.com/foo"), GURL(), nullptr, default_file_path,
      alternate_extensions, profile(),
      base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(IsResult(DownloadCheckResult::DANGEROUS));
  ASSERT_EQ(ChromeUserPopulation::SAFE_BROWSING,
            GetClientDownloadRequest()->population().user_population());
}

TEST_F(DownloadProtectionServiceTest, PPAPIDownloadRequest_WhitelistedURL) {
  base::FilePath default_file_path(FILE_PATH_LITERAL("/foo/bar/test.crx"));
  std::vector<base::FilePath::StringType> alternate_extensions;
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(true));

  RunLoop run_loop;
  download_service_->CheckPPAPIDownloadRequest(
      GURL("http://example.com/foo"), GURL(), nullptr, default_file_path,
      alternate_extensions, profile(),
      base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(IsResult(DownloadCheckResult::SAFE));
}

TEST_F(DownloadProtectionServiceTest, PPAPIDownloadRequest_FetchFailed) {
  base::FilePath default_file_path(FILE_PATH_LITERAL("/foo/bar/test.crx"));
  std::vector<base::FilePath::StringType> alternate_extensions;
  PrepareResponse(ClientDownloadResponse::DANGEROUS, net::HTTP_OK,
                  net::ERR_FAILED);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  RunLoop run_loop;
  download_service_->CheckPPAPIDownloadRequest(
      GURL("http://example.com/foo"), GURL(), nullptr, default_file_path,
      alternate_extensions, profile(),
      base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
}

TEST_F(DownloadProtectionServiceTest, PPAPIDownloadRequest_InvalidResponse) {
  base::FilePath default_file_path(FILE_PATH_LITERAL("/foo/bar/test.crx"));
  std::vector<base::FilePath::StringType> alternate_extensions;
  sb_service_->test_url_loader_factory()->AddResponse(
      PPAPIDownloadRequest::GetDownloadRequestUrl().spec(), "Hello world!");
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  RunLoop run_loop;
  download_service_->CheckPPAPIDownloadRequest(
      GURL("http://example.com/foo"), GURL(), nullptr, default_file_path,
      alternate_extensions, profile(),
      base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
}

TEST_F(DownloadProtectionServiceTest, PPAPIDownloadRequest_Timeout) {
  base::FilePath default_file_path(FILE_PATH_LITERAL("/foo/bar/test.crx"));
  std::vector<base::FilePath::StringType> alternate_extensions;
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);
  download_service_->download_request_timeout_ms_ = 0;
  RunLoop run_loop;
  download_service_->CheckPPAPIDownloadRequest(
      GURL("http://example.com/foo"), GURL(), nullptr, default_file_path,
      alternate_extensions, profile(),
      base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
}

TEST_F(DownloadProtectionServiceTest, PPAPIDownloadRequest_Payload) {
  RunLoop interceptor_run_loop;

  std::string upload_data;
  sb_service_->test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        upload_data = network::GetUploadData(request);
      }));

  base::FilePath default_file_path(FILE_PATH_LITERAL("/foo/bar/test.crx"));
  std::vector<base::FilePath::StringType> alternate_extensions{
      FILE_PATH_LITERAL(".txt"), FILE_PATH_LITERAL(".abc"),
      FILE_PATH_LITERAL(""), FILE_PATH_LITERAL(".sdF")};
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);
  const GURL kRequestorUrl("http://example.com/foo");
  RunLoop run_loop;
  download_service_->CheckPPAPIDownloadRequest(
      kRequestorUrl, GURL(), nullptr, default_file_path, alternate_extensions,
      profile(),
      base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_FALSE(upload_data.empty());

  ClientDownloadRequest request;
  ASSERT_TRUE(request.ParseFromString(upload_data));

  EXPECT_EQ(ClientDownloadRequest::PPAPI_SAVE_REQUEST, request.download_type());
  EXPECT_EQ(kRequestorUrl.spec(), request.url());
  EXPECT_EQ("test.crx", request.file_basename());
  ASSERT_EQ(3, request.alternate_extensions_size());
  EXPECT_EQ(".txt", request.alternate_extensions(0));
  EXPECT_EQ(".abc", request.alternate_extensions(1));
  EXPECT_EQ(".sdF", request.alternate_extensions(2));
}

TEST_F(DownloadProtectionServiceTest,
       PPAPIDownloadRequest_WhitelistedByPolicy) {
  AddDomainToEnterpriseWhitelist("example.com");
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  base::FilePath default_file_path(FILE_PATH_LITERAL("/foo/bar/test.txt"));
  std::vector<base::FilePath::StringType> alternate_extensions{
      FILE_PATH_LITERAL(".tmp"), FILE_PATH_LITERAL(".asdfasdf")};
  download_service_->CheckPPAPIDownloadRequest(
      GURL("http://example.com/foo"), GURL(), web_contents.get(),
      default_file_path, alternate_extensions, profile(),
      base::BindOnce(&DownloadProtectionServiceTest::SyncCheckDoneCallback,
                     base::Unretained(this)));
  ASSERT_TRUE(IsResult(DownloadCheckResult::WHITELISTED_BY_POLICY));
}

TEST_F(DownloadProtectionServiceTest,
       VerifyMaybeSendDangerousDownloadOpenedReport) {
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item,
                           std::vector<std::string>(),   // empty url_chain
                           "http://www.google.com/",     // referrer
                           FILE_PATH_LITERAL("a.tmp"),   // tmp_path
                           FILE_PATH_LITERAL("a.exe"));  // final_path
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);
  ASSERT_EQ(0, sb_service_->download_report_count());

  // No report sent if download item without token field.
  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, false);
  EXPECT_EQ(0, sb_service_->download_report_count());

  // No report sent if user is in incognito mode.
  DownloadProtectionService::SetDownloadPingToken(&item, "token");
  content::DownloadItemUtils::AttachInfo(
      &item, profile()->GetPrimaryOTRProfile(), nullptr);
  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, false);
  EXPECT_EQ(0, sb_service_->download_report_count());

  // No report sent if user is not in extended reporting  group.
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);
  SetExtendedReportingPreference(false);
  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, false);
  EXPECT_EQ(0, sb_service_->download_report_count());

  // Report successfully sent if user opted-in extended reporting, not in
  // incognito, and download item has a token stored.
  SetExtendedReportingPreference(true);
  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, false);
  EXPECT_EQ(1, sb_service_->download_report_count());
  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, true);
  EXPECT_EQ(2, sb_service_->download_report_count());
}

TEST_F(DownloadProtectionServiceTest, VerifyDangerousDownloadOpenedAPICall) {
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item,
                           {"http://example.com/a.exe"},  // empty url_chain
                           "http://example.com/",         // referrer
                           FILE_PATH_LITERAL("a.tmp"),    // tmp_path
                           FILE_PATH_LITERAL("a.exe"));   // final_path
  std::string hash = "hash";
  EXPECT_CALL(item, GetHash()).WillRepeatedly(ReturnRef(hash));
  base::FilePath target_path;
  target_path = target_path.AppendASCII("filepath");
  EXPECT_CALL(item, GetTargetFilePath()).WillRepeatedly(ReturnRef(target_path));

  TestExtensionEventObserver event_observer(test_event_router_);
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);
  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, false);
  ASSERT_EQ(1, test_event_router_->GetEventCount(
                   OnDangerousDownloadOpened::kEventName));
  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("http://example.com/a.exe",
            captured_args.FindKey("url")->GetString());
  EXPECT_EQ(base::HexEncode(hash.data(), hash.size()),
            captured_args.FindKey("downloadDigestSha256")->GetString());
  EXPECT_EQ(target_path.MaybeAsASCII(),
            captured_args.FindKey("fileName")->GetString());

  // No event is triggered if in incognito mode..
  content::DownloadItemUtils::AttachInfo(
      &item, profile()->GetPrimaryOTRProfile(), nullptr);
  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, false);
  EXPECT_EQ(1, test_event_router_->GetEventCount(
                   OnDangerousDownloadOpened::kEventName));
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadWhitelistedByPolicy) {
  AddDomainToEnterpriseWhitelist("example.com");
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://example.com/a.exe"},  // url_chain
                           "http://www.google.com/",             // referrer
                           FILE_PATH_LITERAL("a.tmp"),           // tmp_path
                           FILE_PATH_LITERAL("a.exe"));          // final_path
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .Times(0);
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(_, _)).Times(0);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(0);

  RunLoop run_loop;
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_FALSE(HasClientDownloadRequest());
  EXPECT_TRUE(IsResult(DownloadCheckResult::WHITELISTED_BY_POLICY));
}

TEST_F(DownloadProtectionServiceTest, CheckOffTheRecordDoesNotSendFeedback) {
  NiceMockDownloadItem item;
  EXPECT_FALSE(download_service_->MaybeBeginFeedbackForDownload(
      profile()->GetPrimaryOTRProfile(), &item, DownloadCommands::KEEP));

  EXPECT_FALSE(download_service_->MaybeBeginFeedbackForDownload(
      profile()->GetOffTheRecordProfile(
          Profile::OTRProfileID("Test::DownloadProtectionService")),
      &item, DownloadCommands::KEEP));
}

// ------------ class DownloadProtectionServiceFlagTest ----------------
class DownloadProtectionServiceFlagTest
    : public DownloadProtectionServiceTestBase,
      public testing::WithParamInterface<bool> {
 protected:
  DownloadProtectionServiceFlagTest()
      :  // Matches unsigned.exe within zipfile_one_unsigned_binary.zip
        blacklisted_hash_hex_(
            "1e954d9ce0389e2ba7447216f21761f98d1e6540c2abecdbecff570e36c493d"
            "b") {}

  void SetUp() override {
    ASSERT_TRUE(
        base::HexStringToString(blacklisted_hash_hex_, &blacklisted_hash_) &&
        blacklisted_hash_.size() == 32);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        safe_browsing::switches::kSbManualDownloadBlacklist,
        blacklisted_hash_hex_);

    DownloadProtectionServiceTestBase::SetUp();
  }

  // Hex 64 chars
  const std::string blacklisted_hash_hex_;
  // Binary 32 bytes
  std::string blacklisted_hash_;
};

TEST_F(DownloadProtectionServiceFlagTest, CheckClientDownloadOverridenByFlag) {
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path
  EXPECT_CALL(item, GetHash()).WillRepeatedly(ReturnRef(blacklisted_hash_));
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_FALSE(HasClientDownloadRequest());
  // Overriden by flag:
  EXPECT_TRUE(IsResult(DownloadCheckResult::DANGEROUS));
}

// Test a real .zip with a real .exe in it, where the .exe is manually
// blacklisted by hash.
TEST_F(DownloadProtectionServiceFlagTest,
       CheckClientDownloadZipOverridenByFlag) {
  NiceMockDownloadItem item;

  PrepareBasicDownloadItemWithFullPaths(
      &item, {"http://www.evil.com/a.exe"},  // url_chain
      "http://www.google.com/",              // referrer
      testdata_path_.AppendASCII(
          "zipfile_one_unsigned_binary.zip"),                   // tmp_path
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("a.zip")));  // final_path

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_FALSE(HasClientDownloadRequest());
  // Overriden by flag:
  EXPECT_TRUE(IsResult(DownloadCheckResult::DANGEROUS));
}

TEST_F(DownloadProtectionServiceTest,
       VerifyReferrerChainWithEmptyNavigationHistory) {
  // Setup a web_contents with "http://example.com" as its last committed url.
  NavigateAndCommit(GURL("http://example.com"));

  NiceMockDownloadItem item;
  std::vector<GURL> url_chain = {GURL("http://example.com/referrer"),
                                 GURL("http://example.com/evil.exe")};
  EXPECT_CALL(item, GetURL()).WillRepeatedly(ReturnRef(url_chain.back()));
  EXPECT_CALL(item, GetUrlChain()).WillRepeatedly(ReturnRef(url_chain));
  content::DownloadItemUtils::AttachInfo(&item, nullptr, web_contents());
  std::unique_ptr<ReferrerChainData> referrer_chain_data =
      download_service_->IdentifyReferrerChain(item);
  ReferrerChain* referrer_chain = referrer_chain_data->GetReferrerChain();

  ASSERT_EQ(1u, referrer_chain_data->referrer_chain_length());
  EXPECT_EQ(item.GetUrlChain().back(), referrer_chain->Get(0).url());
  EXPECT_EQ(web_contents()->GetLastCommittedURL().spec(),
            referrer_chain->Get(0).referrer_url());
  EXPECT_EQ(ReferrerChainEntry::EVENT_URL, referrer_chain->Get(0).type());
  EXPECT_EQ(static_cast<int>(item.GetUrlChain().size()),
            referrer_chain->Get(0).server_redirect_chain_size());
  EXPECT_FALSE(referrer_chain->Get(0).is_retargeting());
}

TEST_F(DownloadProtectionServiceTest,
       VerifyReferrerChainLengthForExtendedReporting) {
  SafeBrowsingNavigationObserver::MaybeCreateForWebContents(web_contents());

  // Simulate 6 user interactions
  SimulateLinkClick(GURL("http://example.com/0"));
  SimulateLinkClick(GURL("http://example.com/1"));
  SimulateLinkClick(GURL("http://example.com/2"));
  SimulateLinkClick(GURL("http://example.com/3"));
  SimulateLinkClick(GURL("http://example.com/4"));
  SimulateLinkClick(GURL("http://example.com/5"));
  SimulateLinkClick(GURL("http://example.com/evil.exe"));

  NiceMockDownloadItem item;
  std::vector<GURL> url_chain = {GURL("http://example.com/evil.exe")};
  EXPECT_CALL(item, GetURL()).WillRepeatedly(ReturnRef(url_chain.back()));
  EXPECT_CALL(item, GetUrlChain()).WillRepeatedly(ReturnRef(url_chain));

  content::DownloadItemUtils::AttachInfo(&item, nullptr, web_contents());

  SetExtendedReportingPrefForTests(profile()->GetPrefs(), true);
  std::unique_ptr<ReferrerChainData> referrer_chain_data =
      download_service_->IdentifyReferrerChain(item);
  // 6 entries means 5 interactions between entries.
  EXPECT_EQ(referrer_chain_data->referrer_chain_length(), 6u);

  SetExtendedReportingPrefForTests(profile()->GetPrefs(), false);
  referrer_chain_data = download_service_->IdentifyReferrerChain(item);
  // 3 entries means 2 interactions between entries.
  EXPECT_EQ(referrer_chain_data->referrer_chain_length(), 3u);
}

TEST_F(DownloadProtectionServiceTest, DoesNotSendPingForCancelledDownloads) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.test/a.exe"},  // url_chain
                           "http://www.google.com/",               // referrer
                           FILE_PATH_LITERAL("a.tmp"),             // tmp_path
                           FILE_PATH_LITERAL("a.exe"));            // final_path

  // Mock a cancelled download.
  EXPECT_CALL(item, GetState())
      .WillRepeatedly(Return(download::DownloadItem::CANCELLED));

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(has_result_);
  EXPECT_FALSE(HasClientDownloadRequest());
}

TEST_P(DeepScanningDownloadTest, PasswordProtectedArchivesBlockedByPreference) {
  if (!flag_enabled())
    return;

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII("safe_browsing")
                 .AppendASCII("download_protection")
                 .AppendASCII("encrypted.zip");

  NiceMockDownloadItem item;
  PrepareBasicDownloadItemWithFullPaths(
      &item, {"http://www.evil.com/encrypted.zip"},  // url_chain
      "http://www.google.com/",                      // referrer
      test_zip,                                      // tmp_path
      temp_dir_.GetPath().Append(
          FILE_PATH_LITERAL("encrypted.zip")));  // final_path
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);

  TestBinaryUploadService* test_upload_service =
      static_cast<TestBinaryUploadService*>(
          BinaryUploadServiceFactory::GetForProfile(profile()));
  test_upload_service->SetResponse(
      BinaryUploadService::Result::FILE_ENCRYPTED,
      enterprise_connectors::ContentAnalysisResponse());

  for (AllowPasswordProtectedFilesValues pref : {ALLOW_NONE, ALLOW_UPLOADS}) {
    SetSendFilesForMalwareCheckPref(
        SendFilesForMalwareCheckValues::SEND_DOWNLOADS);
    SetPasswordProtectedAllowedPref(pref);
    PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::BLOCKED_PASSWORD_PROTECTED));
    EXPECT_TRUE(HasClientDownloadRequest());
  }

  for (AllowPasswordProtectedFilesValues pref :
       {ALLOW_DOWNLOADS, ALLOW_UPLOADS_AND_DOWNLOADS}) {
    SetPasswordProtectedAllowedPref(pref);
    PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
}

TEST_P(DeepScanningDownloadTest, LargeFileBlockedByPreference) {
  if (!flag_enabled())
    return;

  base::FilePath test_zip;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII("safe_browsing")
                 .AppendASCII("download_protection")
                 .AppendASCII("encrypted.zip");

  NiceMockDownloadItem item;
  PrepareBasicDownloadItemWithFullPaths(
      &item, {"http://www.evil.com/encrypted.zip"},  // url_chain
      "http://www.google.com/",                      // referrer
      test_zip,                                      // tmp_path
      temp_dir_.GetPath().Append(
          FILE_PATH_LITERAL("encrypted.zip")));  // final_path
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);

  EXPECT_CALL(item, GetReceivedBytes())
      .WillRepeatedly(Return(100 * 1024 * 1024));

  TestBinaryUploadService* test_upload_service =
      static_cast<TestBinaryUploadService*>(
          BinaryUploadServiceFactory::GetForProfile(profile()));
  test_upload_service->SetResponse(
      BinaryUploadService::Result::FILE_TOO_LARGE,
      enterprise_connectors::ContentAnalysisResponse());

  for (BlockLargeFileTransferValues pref :
       {BLOCK_LARGE_DOWNLOADS, BLOCK_LARGE_UPLOADS_AND_DOWNLOADS}) {
    SetSendFilesForMalwareCheckPref(
        SendFilesForMalwareCheckValues::SEND_DOWNLOADS);
    SetBlockLargeFilesPref(pref);
    PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::BLOCKED_TOO_LARGE));
    EXPECT_TRUE(HasClientDownloadRequest());
  }

  for (BlockLargeFileTransferValues pref : {BLOCK_NONE, BLOCK_LARGE_UPLOADS}) {
    SetBlockLargeFilesPref(pref);
    PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
}

TEST_P(DeepScanningDownloadTest, UnsupportedFiletypeBlockedByPreference) {
  if (!flag_enabled())
    return;

  base::FilePath test_file;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_file));
  test_file = test_file.AppendASCII("safe_browsing")
                  .AppendASCII("download_protection")
                  .AppendASCII("signed.exe");

  NiceMockDownloadItem item;
  PrepareBasicDownloadItemWithFullPaths(
      &item, {"http://www.evil.com/signed.exe"},  // url_chain
      "http://www.google.com/",                   // referrer
      test_file,                                  // tmp_path
      temp_dir_.GetPath().Append(
          FILE_PATH_LITERAL("signed.exe")));  // final_path
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);

  TestBinaryUploadService* test_upload_service =
      static_cast<TestBinaryUploadService*>(
          BinaryUploadServiceFactory::GetForProfile(profile()));
  test_upload_service->SetResponse(
      BinaryUploadService::Result::DLP_SCAN_UNSUPPORTED_FILE_TYPE,
      enterprise_connectors::ContentAnalysisResponse());

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(4);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(4);

  SetCheckContentCompliancePref(CheckContentComplianceValues::CHECK_DOWNLOADS);
  SetUrlToCheckContentCompliance("www.evil.com");

  for (BlockUnsupportedFiletypesValues pref :
       {BLOCK_UNSUPPORTED_FILETYPES_DOWNLOADS,
        BLOCK_UNSUPPORTED_FILETYPES_UPLOADS_AND_DOWNLOADS}) {
    SetBlockUnsupportedFiletypePref(pref);
    PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::BLOCKED_UNSUPPORTED_FILE_TYPE));
    EXPECT_TRUE(HasClientDownloadRequest());
  }

  for (BlockUnsupportedFiletypesValues pref :
       {BLOCK_UNSUPPORTED_FILETYPES_NONE,
        BLOCK_UNSUPPORTED_FILETYPES_UPLOADS}) {
    SetBlockUnsupportedFiletypePref(pref);
    PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
}

TEST_F(DownloadProtectionServiceTest, NativeFileSystemWriteRequest_NotABinary) {
  auto item = PrepareBasicNativeFileSystemWriteItem(
      /*tmp_path=*/FILE_PATH_LITERAL("a.txt.crswap"),
      /*final_path=*/FILE_PATH_LITERAL("a.txt"));

  RunLoop run_loop;
  download_service_->CheckNativeFileSystemWrite(
      std::move(item),
      base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
  EXPECT_FALSE(HasClientDownloadRequest());
}

TEST_F(DownloadProtectionServiceTest,
       NativeFileSystemWriteRequest_SampledFile) {
  // Server response will be discarded.
  PrepareResponse(ClientDownloadResponse::DANGEROUS, net::HTTP_OK, net::OK);

  auto item = PrepareBasicNativeFileSystemWriteItem(
      /*tmp_path=*/FILE_PATH_LITERAL("a.txt.crswap"),
      /*final_path=*/FILE_PATH_LITERAL("a.txt"));

  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(1);

  // Set ping sample rate to 1.00 so download_service_ will always send a
  // "light" ping for unknown types if allowed.
  SetBinarySamplingProbability(1.0);

  {
    // Case (1): is_extended_reporting && is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    SetExtendedReportingPreference(true);
    item->browser_context = profile()->GetPrimaryOTRProfile();

    RunLoop run_loop;
    download_service_->CheckNativeFileSystemWrite(
        std::move(item),
        base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
  {
    // Case (2): is_extended_reporting && !is_incognito.
    //           A "light" ClientDownloadRequest should be sent.
    item = PrepareBasicNativeFileSystemWriteItem(
        /*tmp_path=*/FILE_PATH_LITERAL("a.txt.crswap"),
        /*final_path=*/FILE_PATH_LITERAL("a.txt"));
    RunLoop run_loop;
    download_service_->CheckNativeFileSystemWrite(
        std::move(item),
        base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
    ASSERT_TRUE(HasClientDownloadRequest());

    // Verify it's a "light" ping, check that URLs don't have paths.
    auto* req = GetClientDownloadRequest();
    EXPECT_EQ(ClientDownloadRequest::SAMPLED_UNSUPPORTED_FILE,
              req->download_type());
    EXPECT_EQ(GURL(req->url()).GetOrigin().spec(), req->url());
    for (auto resource : req->resources()) {
      EXPECT_EQ(GURL(resource.url()).GetOrigin().spec(), resource.url());
      EXPECT_EQ(GURL(resource.referrer()).GetOrigin().spec(),
                resource.referrer());
    }
    ClearClientDownloadRequest();
  }
  {
    // Case (3): !is_extended_reporting && is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    SetExtendedReportingPreference(false);
    item = PrepareBasicNativeFileSystemWriteItem(
        /*tmp_path=*/FILE_PATH_LITERAL("a.txt.crswap"),
        /*final_path=*/FILE_PATH_LITERAL("a.txt"));
    item->browser_context = profile()->GetPrimaryOTRProfile();

    RunLoop run_loop;
    download_service_->CheckNativeFileSystemWrite(
        std::move(item),
        base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
  {
    // Case (4): !is_extended_reporting && !is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    item = PrepareBasicNativeFileSystemWriteItem(
        /*tmp_path=*/FILE_PATH_LITERAL("a.txt.crswap"),
        /*final_path=*/FILE_PATH_LITERAL("a.txt"));
    RunLoop run_loop;
    download_service_->CheckNativeFileSystemWrite(
        std::move(item),
        base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
}

TEST_F(DownloadProtectionServiceTest,
       NativeFileSystemWriteRequest_FetchFailed) {
  // HTTP request will fail.
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_INTERNAL_SERVER_ERROR,
                  net::ERR_FAILED);

  auto item = PrepareBasicNativeFileSystemWriteItem(
      /*tmp_path=*/FILE_PATH_LITERAL("a.exe.crswap"),
      /*final_path=*/FILE_PATH_LITERAL("a.exe"));

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));
  RunLoop run_loop;
  download_service_->CheckNativeFileSystemWrite(
      std::move(item),
      base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
}

TEST_F(DownloadProtectionServiceTest, NativeFileSystemWriteRequest_Success) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  auto item = PrepareBasicNativeFileSystemWriteItem(
      /*tmp_path=*/FILE_PATH_LITERAL("a.exe.crswap"),
      /*final_path=*/FILE_PATH_LITERAL("a.exe"));

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(8);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(8);
  ClientDownloadResponse expected_response;

  {
    RunLoop run_loop;
    download_service_->CheckNativeFileSystemWrite(
        CloneNativeFileSystemWriteItem(item.get()),
        base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
  {
    // Invalid response should result in SAFE (default value in proto).
    ClientDownloadResponse invalid_response;
    sb_service_->test_url_loader_factory()->AddResponse(
        PPAPIDownloadRequest::GetDownloadRequestUrl().spec(),
        invalid_response.SerializePartialAsString());
    RunLoop run_loop;
    download_service_->CheckNativeFileSystemWrite(
        CloneNativeFileSystemWriteItem(item.get()),
        base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }

  struct {
    ClientDownloadResponse::Verdict verdict;
    DownloadCheckResult expected_result;
  } kExpectedResults[] = {
      {ClientDownloadResponse::SAFE, DownloadCheckResult::SAFE},
      {ClientDownloadResponse::DANGEROUS, DownloadCheckResult::DANGEROUS},
      {ClientDownloadResponse::UNCOMMON, DownloadCheckResult::UNCOMMON},
      {ClientDownloadResponse::DANGEROUS_HOST,
       DownloadCheckResult::DANGEROUS_HOST},
      {ClientDownloadResponse::POTENTIALLY_UNWANTED,
       DownloadCheckResult::POTENTIALLY_UNWANTED},
      {ClientDownloadResponse::UNKNOWN, DownloadCheckResult::UNKNOWN},
  };
  for (const auto& test_case : kExpectedResults) {
    PrepareResponse(test_case.verdict, net::HTTP_OK, net::OK);
    RunLoop run_loop;
    download_service_->CheckNativeFileSystemWrite(
        CloneNativeFileSystemWriteItem(item.get()),
        base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(test_case.expected_result));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
}

TEST_F(DownloadProtectionServiceTest,
       NativeFileSystemWriteRequest_WhitelistedByPolicy) {
  AddDomainToEnterpriseWhitelist("example.com");

  auto item = PrepareBasicNativeFileSystemWriteItem(
      /*tmp_path=*/FILE_PATH_LITERAL("a.txt.crswap"),
      /*final_path=*/FILE_PATH_LITERAL("a.txt"));
  item->frame_url = GURL("https://example.com/foo");
  download_service_->CheckNativeFileSystemWrite(
      std::move(item),
      base::BindOnce(&DownloadProtectionServiceTest::SyncCheckDoneCallback,
                     base::Unretained(this)));
  ASSERT_TRUE(IsResult(DownloadCheckResult::WHITELISTED_BY_POLICY));
}

TEST_F(DownloadProtectionServiceTest,
       NativeFileSystemWriteRequest_CheckRequest) {
  auto item = PrepareBasicNativeFileSystemWriteItem(
      /*tmp_path=*/FILE_PATH_LITERAL("a.exe.crswap"),
      /*final_path=*/FILE_PATH_LITERAL("a.exe"));
  item->frame_url = GURL("http://www.google.com/");

  GURL tab_url("http://tab.com/final");
  GURL tab_referrer("http://tab.com/referrer");

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      tab_url, web_contents());
  navigation->SetReferrer(blink::mojom::Referrer::New(
      tab_referrer, network::mojom::ReferrerPolicy::kDefault));
  navigation->Commit();

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .WillRepeatedly(SetCertificateContents("dummy cert data"));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .WillRepeatedly(SetDosHeaderContents("dummy dos header"));

  // First test with no history match for the tab URL.
  {
    RunLoop interceptor_run_loop;

    std::string upload_data;
    sb_service_->test_url_loader_factory()->SetInterceptor(
        base::BindLambdaForTesting(
            [&](const network::ResourceRequest& request) {
              upload_data = network::GetUploadData(request);
              if (!upload_data.empty())
                interceptor_run_loop.Quit();
            }));

    PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

    RunLoop run_loop;
    download_service_->CheckNativeFileSystemWrite(
        CloneNativeFileSystemWriteItem(item.get()),
        base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    interceptor_run_loop.Run();

    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();

    ClientDownloadRequest request;
    EXPECT_TRUE(request.ParseFromString(upload_data));
    EXPECT_EQ("blob:http://www.google.com/native-file-system-write",
              request.url());
    EXPECT_EQ(hash_, request.digests().sha256());
    EXPECT_EQ(item->size, request.length());
    EXPECT_EQ(item->has_user_gesture, request.user_initiated());
    EXPECT_EQ(2, request.resources_size());
    EXPECT_TRUE(RequestContainsResource(
        request, ClientDownloadRequest::DOWNLOAD_URL,
        "blob:http://www.google.com/native-file-system-write",
        referrer_.spec()));
    EXPECT_TRUE(RequestContainsResource(request, ClientDownloadRequest::TAB_URL,
                                        tab_url.spec(), tab_referrer.spec()));
    EXPECT_TRUE(request.has_signature());
    ASSERT_EQ(1, request.signature().certificate_chain_size());
    const ClientDownloadRequest_CertificateChain& chain =
        request.signature().certificate_chain(0);
    ASSERT_EQ(1, chain.element_size());
    EXPECT_EQ("dummy cert data", chain.element(0).certificate());
    EXPECT_TRUE(request.has_image_headers());
    const ClientDownloadRequest_ImageHeaders& headers = request.image_headers();
    EXPECT_TRUE(headers.has_pe_headers());
    EXPECT_TRUE(headers.pe_headers().has_dos_header());
    EXPECT_EQ("dummy dos header", headers.pe_headers().dos_header());

    sb_service_->test_url_loader_factory()->SetInterceptor(
        network::TestURLLoaderFactory::Interceptor());

    // Simulate the request finishing.
    run_loop.Run();
  }

  // Now try with a history match.
  {
    RunLoop interceptor_run_loop;

    std::string upload_data;
    sb_service_->test_url_loader_factory()->SetInterceptor(
        base::BindLambdaForTesting(
            [&](const network::ResourceRequest& request) {
              upload_data = network::GetUploadData(request);
              if (!upload_data.empty())
                interceptor_run_loop.Quit();
            }));

    history::RedirectList redirects;
    redirects.push_back(GURL("http://tab.com/ref1"));
    redirects.push_back(GURL("http://tab.com/ref2"));
    redirects.push_back(tab_url);
    HistoryServiceFactory::GetForProfile(profile(),
                                         ServiceAccessType::EXPLICIT_ACCESS)
        ->AddPage(tab_url, base::Time::Now(),
                  reinterpret_cast<history::ContextID>(1), 0, GURL(), redirects,
                  ui::PAGE_TRANSITION_TYPED, history::SOURCE_BROWSED, false,
                  false);

    PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

    RunLoop run_loop;
    download_service_->CheckNativeFileSystemWrite(
        CloneNativeFileSystemWriteItem(item.get()),
        base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    interceptor_run_loop.Run();

    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
    ClientDownloadRequest request;
    EXPECT_TRUE(request.ParseFromString(upload_data));
    EXPECT_EQ("blob:http://www.google.com/native-file-system-write",
              request.url());
    EXPECT_EQ(hash_, request.digests().sha256());
    EXPECT_EQ(item->size, request.length());
    EXPECT_EQ(item->has_user_gesture, request.user_initiated());
    EXPECT_EQ(4, request.resources_size());
    EXPECT_TRUE(RequestContainsResource(
        request, ClientDownloadRequest::DOWNLOAD_URL,
        "blob:http://www.google.com/native-file-system-write",
        referrer_.spec()));
    EXPECT_TRUE(RequestContainsResource(request,
                                        ClientDownloadRequest::TAB_REDIRECT,
                                        "http://tab.com/ref1", ""));
    EXPECT_TRUE(RequestContainsResource(request,
                                        ClientDownloadRequest::TAB_REDIRECT,
                                        "http://tab.com/ref2", ""));
    EXPECT_TRUE(RequestContainsResource(request, ClientDownloadRequest::TAB_URL,
                                        tab_url.spec(), tab_referrer.spec()));
    EXPECT_TRUE(request.has_signature());
    ASSERT_EQ(1, request.signature().certificate_chain_size());
    const ClientDownloadRequest_CertificateChain& chain =
        request.signature().certificate_chain(0);
    ASSERT_EQ(1, chain.element_size());
    EXPECT_EQ("dummy cert data", chain.element(0).certificate());

    // Simulate the request finishing.
    run_loop.Run();
  }
}

TEST_F(EnhancedProtectionDownloadTest, AccessTokenForEnhancedProtectionUsers) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  identity_test_env_adaptor_->identity_test_env()->MakePrimaryAccountAvailable(
      "test@example.com");
  identity_test_env_adaptor_->identity_test_env()
      ->SetAutomaticIssueOfAccessTokens(/*grant=*/true);

  WebUIInfoSingleton::GetInstance()->AddListenerForTesting();

  {
    SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);
    NiceMockDownloadItem item;
    content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);
    PrepareBasicDownloadItem(&item,
                             {"http://www.evil.com/bla.exe"},  // url_chain
                             "",                               // referrer
                             FILE_PATH_LITERAL("a.tmp"),       // tmp_path
                             FILE_PATH_LITERAL("a.exe"));      // final_path

    EXPECT_CALL(*sb_service_->mock_database_manager(),
                MatchDownloadWhitelistUrl(GURL("http://www.evil.com/bla.exe")))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
        .Times(1);
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
        .Times(1);

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
        WebUIInfoSingleton::GetInstance()->client_download_requests_sent();
    ASSERT_EQ(requests.size(), 1u);
    EXPECT_EQ(requests[0]->access_token(), "access_token");
  }

  {
    SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), false);
    NiceMockDownloadItem item;
    content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);
    PrepareBasicDownloadItem(&item,
                             {"http://www.evil.com/bla.exe"},  // url_chain
                             "",                               // referrer
                             FILE_PATH_LITERAL("a.tmp"),       // tmp_path
                             FILE_PATH_LITERAL("a.exe"));      // final_path

    EXPECT_CALL(*sb_service_->mock_database_manager(),
                MatchDownloadWhitelistUrl(GURL("http://www.evil.com/bla.exe")))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
        .Times(1);
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
        .Times(1);

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
        WebUIInfoSingleton::GetInstance()->client_download_requests_sent();
    ASSERT_EQ(requests.size(), 2u);
    EXPECT_TRUE(requests[1]->access_token().empty());
  }

  WebUIInfoSingleton::GetInstance()->ClearListenerForTesting();
}

TEST_F(EnhancedProtectionDownloadTest, AccessTokenOnlyWhenSignedIn) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  identity_test_env_adaptor_->identity_test_env()
      ->SetAutomaticIssueOfAccessTokens(/*grant=*/true);
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  WebUIInfoSingleton::GetInstance()->AddListenerForTesting();

  {
    NiceMockDownloadItem item;
    content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);
    PrepareBasicDownloadItem(&item,
                             {"http://www.evil.com/bla.exe"},  // url_chain
                             "",                               // referrer
                             FILE_PATH_LITERAL("a.tmp"),       // tmp_path
                             FILE_PATH_LITERAL("a.exe"));      // final_path

    EXPECT_CALL(*sb_service_->mock_database_manager(),
                MatchDownloadWhitelistUrl(GURL("http://www.evil.com/bla.exe")))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
        .Times(1);
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
        .Times(1);

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
        WebUIInfoSingleton::GetInstance()->client_download_requests_sent();
    ASSERT_EQ(requests.size(), 1u);
    EXPECT_TRUE(requests[0]->access_token().empty());
  }

  identity_test_env_adaptor_->identity_test_env()->MakePrimaryAccountAvailable(
      "test@example.com");

  {
    NiceMockDownloadItem item;
    content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);
    PrepareBasicDownloadItem(&item,
                             {"http://www.evil.com/bla.exe"},  // url_chain
                             "",                               // referrer
                             FILE_PATH_LITERAL("a.tmp"),       // tmp_path
                             FILE_PATH_LITERAL("a.exe"));      // final_path

    EXPECT_CALL(*sb_service_->mock_database_manager(),
                MatchDownloadWhitelistUrl(GURL("http://www.evil.com/bla.exe")))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
        .Times(1);
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
        .Times(1);

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
        WebUIInfoSingleton::GetInstance()->client_download_requests_sent();
    ASSERT_EQ(requests.size(), 2u);
    EXPECT_EQ(requests[1]->access_token(), "access_token");
  }

  WebUIInfoSingleton::GetInstance()->ClearListenerForTesting();
}

TEST_F(EnhancedProtectionDownloadTest, NoAccessTokenWhileIncognito) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  WebUIInfoSingleton::GetInstance()->AddListenerForTesting();

  {
    SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);
    NiceMockDownloadItem item;
    content::DownloadItemUtils::AttachInfo(
        &item, profile()->GetPrimaryOTRProfile(), nullptr);
    PrepareBasicDownloadItem(&item,
                             {"http://www.evil.com/bla.exe"},  // url_chain
                             "",                               // referrer
                             FILE_PATH_LITERAL("a.tmp"),       // tmp_path
                             FILE_PATH_LITERAL("a.exe"));      // final_path

    EXPECT_CALL(*sb_service_->mock_database_manager(),
                MatchDownloadWhitelistUrl(GURL("http://www.evil.com/bla.exe")))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
        .Times(1);
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
        .Times(1);

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
        WebUIInfoSingleton::GetInstance()->client_download_requests_sent();
    ASSERT_EQ(requests.size(), 1u);
    EXPECT_TRUE(requests[0]->access_token().empty());
  }

  WebUIInfoSingleton::GetInstance()->ClearListenerForTesting();
}

class DownloadSeparateNetworkContextsTest
    : public DownloadProtectionServiceTestBase,
      public testing::WithParamInterface<bool> {
 public:
  DownloadSeparateNetworkContextsTest() {
    EnableFeatures({kSafeBrowsingSeparateNetworkContexts});
  }
};

TEST_F(DownloadSeparateNetworkContextsTest,
       DifferentProfilesUseDifferentNetworkContexts) {
  Profile* profile1 =
      testing_profile_manager_.CreateTestingProfile("profile 1");
  sb_service_->CreateTestURLLoaderFactoryForProfile(profile1);

  Profile* profile2 =
      testing_profile_manager_.CreateTestingProfile("profile 2");
  sb_service_->CreateTestURLLoaderFactoryForProfile(profile2);

  {
    RunLoop run_loop;

    NiceMockDownloadItem item1;
    PrepareBasicDownloadItem(&item1,
                             {"http://www.evil.com/a.exe"},  // url_chain
                             "http://www.google.com/",       // referrer
                             FILE_PATH_LITERAL("a.tmp"),     // tmp_path
                             FILE_PATH_LITERAL("a.exe"));    // final_path
    content::DownloadItemUtils::AttachInfo(&item1, profile1, nullptr);

    ClientDownloadResponse response;
    response.set_verdict(ClientDownloadResponse::SAFE);
    sb_service_->GetTestURLLoaderFactory(profile1)->AddResponse(
        PPAPIDownloadRequest::GetDownloadRequestUrl().spec(),
        response.SerializeAsString());

    EXPECT_CALL(*sb_service_->mock_database_manager(),
                MatchDownloadWhitelistUrl(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

    download_service_->CheckClientDownload(
        &item1, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                           base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(sb_service_->GetTestURLLoaderFactory(profile2)->NumPending(), 0);

  {
    RunLoop run_loop;

    NiceMockDownloadItem item2;
    PrepareBasicDownloadItem(&item2,
                             {"http://www.evil.com/a.exe"},  // url_chain
                             "http://www.google.com/",       // referrer
                             FILE_PATH_LITERAL("a.tmp"),     // tmp_path
                             FILE_PATH_LITERAL("a.exe"));    // final_path
    content::DownloadItemUtils::AttachInfo(&item2, profile2, nullptr);

    ClientDownloadResponse response;
    response.set_verdict(ClientDownloadResponse::SAFE);
    sb_service_->GetTestURLLoaderFactory(profile2)->AddResponse(
        PPAPIDownloadRequest::GetDownloadRequestUrl().spec(),
        response.SerializeAsString());

    EXPECT_CALL(*sb_service_->mock_database_manager(),
                MatchDownloadWhitelistUrl(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));
    download_service_->CheckClientDownload(
        &item2, base::Bind(&DownloadProtectionServiceTest::CheckDoneCallback,
                           base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(sb_service_->GetTestURLLoaderFactory(profile1)->NumPending(), 0);

  testing_profile_manager_.DeleteTestingProfile("profile 1");
  testing_profile_manager_.DeleteTestingProfile("profile 2");
}

TEST_P(DeepScanningDownloadTest, PolicyEnabled) {
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

  SetSendFilesForMalwareCheckPref(
      SendFilesForMalwareCheckValues::SEND_DOWNLOADS);

  TestBinaryUploadService* test_upload_service =
      static_cast<TestBinaryUploadService*>(
          BinaryUploadServiceFactory::GetForProfile(profile()));

  {
    PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);
    test_upload_service->SetResponse(
        BinaryUploadService::Result::UPLOAD_FAILURE,
        enterprise_connectors::ContentAnalysisResponse());

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    if (flag_enabled()) {
      EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
      EXPECT_TRUE(HasClientDownloadRequest());
      EXPECT_TRUE(test_upload_service->was_called());
    } else {
      EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
      EXPECT_TRUE(HasClientDownloadRequest());
      EXPECT_FALSE(test_upload_service->was_called());
    }
  }
}

TEST_P(DeepScanningDownloadTest, PolicyDisabled) {
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path
  content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadWhitelistUrl(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

  SetSendFilesForMalwareCheckPref(SendFilesForMalwareCheckValues::DO_NOT_SCAN);

  TestBinaryUploadService* test_upload_service =
      static_cast<TestBinaryUploadService*>(
          BinaryUploadServiceFactory::GetForProfile(profile()));

  {
    PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    EXPECT_TRUE(HasClientDownloadRequest());
    EXPECT_FALSE(test_upload_service->was_called());
  }
}

TEST_P(DeepScanningDownloadTest, SafeVerdictPrecedence) {
  // These responses have precedence over safe deep scanning results.
  std::vector<std::pair<ClientDownloadResponse::Verdict, DownloadCheckResult>>
      responses = {
          {ClientDownloadResponse::DANGEROUS, DownloadCheckResult::DANGEROUS},
          {ClientDownloadResponse::DANGEROUS_HOST,
           DownloadCheckResult::DANGEROUS_HOST},
          {ClientDownloadResponse::POTENTIALLY_UNWANTED,
           DownloadCheckResult::POTENTIALLY_UNWANTED},
          {ClientDownloadResponse::UNCOMMON, DownloadCheckResult::UNCOMMON},
      };
  for (const auto& response : responses) {
    NiceMockDownloadItem item;
    PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                             "http://www.google.com/",              // referrer
                             FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                             FILE_PATH_LITERAL("a.exe"));  // final_path
    content::DownloadItemUtils::AttachInfo(&item, profile(), nullptr);
    EXPECT_CALL(*sb_service_->mock_database_manager(),
                MatchDownloadWhitelistUrl(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

    SetSendFilesForMalwareCheckPref(
        SendFilesForMalwareCheckValues::SEND_DOWNLOADS);
    SetCheckContentCompliancePref(
        CheckContentComplianceValues::CHECK_DOWNLOADS);
    SetUrlToCheckContentCompliance("evil.com");

    TestBinaryUploadService* test_upload_service =
        static_cast<TestBinaryUploadService*>(
            BinaryUploadServiceFactory::GetForProfile(profile()));

    PrepareResponse(response.first, net::HTTP_OK, net::OK);
    test_upload_service->SetResponse(
        BinaryUploadService::Result::SUCCESS,
        enterprise_connectors::ContentAnalysisResponse());

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_TRUE(IsResult(response.second));
    EXPECT_TRUE(HasClientDownloadRequest());
    EXPECT_EQ(test_upload_service->was_called(), flag_enabled());
  }
}

INSTANTIATE_TEST_SUITE_P(, DeepScanningDownloadTest, testing::Bool());

}  // namespace safe_browsing
