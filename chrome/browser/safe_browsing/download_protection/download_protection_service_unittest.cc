// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/optional_ref.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/download_protection/check_file_system_access_write_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_unittest_util.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "chrome/common/safe_browsing/mock_binary_feature_extractor.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer.h"
#include "components/safe_browsing/content/browser/web_ui/web_ui_content_info_singleton.h"
#include "components/safe_browsing/content/common/file_type_policies_test_util.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/url_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "third_party/zlib/google/zip.h"
#include "url/gurl.h"

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/safe_browsing/test_extension_event_observer.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "extensions/browser/test_event_router.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/test_binary_upload_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_feedback.h"
#include "chrome/browser/safe_browsing/download_protection/download_feedback_service.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_reporting_service.h"
#else
#include "chrome/browser/safe_browsing/android/download_protection_metrics_data.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_delegate_android.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#endif

using base::RunLoop;
using content::BrowserThread;
using content::FileSystemAccessWriteItem;
using ::testing::_;
using ::testing::Assign;
using ::testing::AtMost;
using ::testing::ContainerEq;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::SizeIs;
using ::testing::StrictMock;

#if BUILDFLAG(ENABLE_EXTENSIONS)
namespace OnDangerousDownloadOpened =
    extensions::api::safe_browsing_private::OnDangerousDownloadOpened;
#endif

namespace safe_browsing {

namespace {
// Default filename that should appear as a a file eligible for download
// protection checks.
#if BUILDFLAG(IS_ANDROID)
const base::FilePath::CharType kEligibleFilename[] = FILE_PATH_LITERAL("a.apk");
#else
const base::FilePath::CharType kEligibleFilename[] = FILE_PATH_LITERAL("a.exe");
#endif

#if BUILDFLAG(IS_ANDROID)
// Fake content-URI values for Android tests.
const base::FilePath::CharType kContentUri[] =
    FILE_PATH_LITERAL("content://media/foo.bar");
const base::FilePath::CharType kTempContentUri[] =
    FILE_PATH_LITERAL("content://media/temp/foo.bar");

// Default APK filename for Android tests.
const base::FilePath::CharType kApkFilename[] = FILE_PATH_LITERAL("a.apk");

const char kAndroidDownloadProtectionOutcomeHistogram[] =
    "SBClientDownload.Android.DownloadProtectionOutcome";
#endif

// A SafeBrowsingDatabaseManager implementation that returns a fixed result for
// a given URL.
class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager()
      : safe_browsing::TestSafeBrowsingDatabaseManager(
            content::GetUIThreadTaskRunner({})) {}
  MockSafeBrowsingDatabaseManager(const MockSafeBrowsingDatabaseManager&) =
      delete;
  MockSafeBrowsingDatabaseManager& operator=(
      const MockSafeBrowsingDatabaseManager&) = delete;

  MOCK_METHOD2(MatchDownloadAllowlistUrl,
               void(const GURL&, base::OnceCallback<void(bool)>));
  MOCK_METHOD2(CheckDownloadUrl,
               bool(const std::vector<GURL>& url_chain,
                    SafeBrowsingDatabaseManager::Client* client));

 protected:
  ~MockSafeBrowsingDatabaseManager() override = default;
};

#if !BUILDFLAG(IS_ANDROID)
std::unique_ptr<KeyedService> CreateTestBinaryUploadService(
    content::BrowserContext* browser_context) {
  return std::make_unique<TestBinaryUploadService>();
}
#endif

#if !BUILDFLAG(IS_ANDROID)
class MockDownloadFeedbackService : public DownloadFeedbackService {
 public:
  MockDownloadFeedbackService() : DownloadFeedbackService(nullptr, nullptr) {}
  ~MockDownloadFeedbackService() override = default;

  MOCK_METHOD(void,
              BeginFeedbackForDownload,
              (Profile * profile,
               download::DownloadItem* download,
               const std::string& ping_request,
               const std::string& ping_response));
};
#endif

class FakeSafeBrowsingService : public TestSafeBrowsingService {
 public:
  explicit FakeSafeBrowsingService(Profile* profile) {
    services_delegate_ = ServicesDelegate::CreateForTest(this, this);
#if !BUILDFLAG(IS_ANDROID)
    CloudBinaryUploadServiceFactory::GetInstance()->SetTestingFactory(
        profile, base::BindRepeating(&CreateTestBinaryUploadService));
#endif
    mock_database_manager_ = new NiceMock<MockSafeBrowsingDatabaseManager>();
  }
  FakeSafeBrowsingService(const FakeSafeBrowsingService&) = delete;
  FakeSafeBrowsingService& operator=(const FakeSafeBrowsingService&) = delete;

  // Returned pointer has the same lifespan as the database_manager_ refcounted
  // object.
  MockSafeBrowsingDatabaseManager* mock_database_manager() {
    return mock_database_manager_.get();
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory(
      content::BrowserContext* browser_context) override {
    auto* profile = Profile::FromBrowserContext(browser_context);
    auto it = test_shared_loader_factory_map_.find(profile);
    if (it == test_shared_loader_factory_map_.end()) {
      return nullptr;
    }
    return it->second;
  }

  void SendDownloadReport(
      download::DownloadItem* download,
      ClientSafeBrowsingReportRequest::ReportType report_type,
      bool did_proceed,
      std::optional<bool> show_download_in_folder) override {
    auto report = std::make_unique<ClientSafeBrowsingReportRequest>();
    report->set_type(report_type);
    report->set_download_verdict(
        DownloadProtectionService::GetDownloadProtectionVerdict(download));
    report->set_url(download->GetURL().spec());
    report->set_did_proceed(did_proceed);
    EXPECT_TRUE(show_download_in_folder.has_value());
    report->set_show_download_in_folder(show_download_in_folder.value());
    report->set_token(
        DownloadProtectionService::GetDownloadPingToken(download));
    report->SerializeToString(&latest_report_);
    download_report_count_++;
    return;
  }

  network::TestURLLoaderFactory* GetTestURLLoaderFactory(Profile* profile) {
    auto it = test_url_loader_factory_map_.find(profile);
    if (it == test_url_loader_factory_map_.end()) {
      return nullptr;
    }
    return it->second.get();
  }

  void CreateTestURLLoaderFactoryForProfile(Profile* profile) {
    test_url_loader_factory_map_[profile] =
        std::make_unique<network::TestURLLoaderFactory>();
    test_shared_loader_factory_map_[profile] =
        test_url_loader_factory_map_[profile]->GetSafeWeakWrapper();
  }

  int download_report_count() { return download_report_count_; }

  std::string latest_report() { return latest_report_; }

 protected:
  ~FakeSafeBrowsingService() override = default;

  void RegisterAllDelayedAnalysis() override {}

 private:
  // ServicesDelegate::ServicesCreator:
  bool CanCreateDatabaseManager() override { return true; }

  bool CanCreateIncidentReportingService() override {
#if BUILDFLAG(IS_ANDROID)
    // Android does not support Incident Reporting for downloads.
    return false;
#else
    return true;
#endif
  }

  safe_browsing::SafeBrowsingDatabaseManager* CreateDatabaseManager() override {
    return mock_database_manager_.get();
  }

  IncidentReportingService* CreateIncidentReportingService() override {
#if BUILDFLAG(IS_ANDROID)
    // Android does not support Incident Reporting for downloads.
    return nullptr;
#else
    return new IncidentReportingService(nullptr);
#endif
  }

  base::flat_map<Profile*, std::unique_ptr<network::TestURLLoaderFactory>>
      test_url_loader_factory_map_;
  base::flat_map<Profile*, scoped_refptr<network::SharedURLLoaderFactory>>
      test_shared_loader_factory_map_;

  scoped_refptr<MockSafeBrowsingDatabaseManager> mock_database_manager_;
  int download_report_count_ = 0;
  std::string latest_report_ = "";
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

// Templated on whether to explicitly set a mock database manager in the
// TestSafeBrowsingService.
template <bool ShouldSetDbManager>
class DownloadProtectionServiceTestBase
    : public ChromeRenderViewHostTestHarness {
 public:
  explicit DownloadProtectionServiceTestBase(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::SYSTEM_TIME)
      : ChromeRenderViewHostTestHarness(time_source),
        in_process_utility_thread_helper_(
            std::make_unique<content::InProcessUtilityThreadHelper>()),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
    std::vector<base::test::FeatureRef> enabled_features = {
        safe_browsing::kEnhancedFieldsForSecOps};
#if BUILDFLAG(IS_ANDROID)
    enabled_features.push_back(kMaliciousApkDownloadCheck);
#endif
    EnableFeatures(enabled_features);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Start real threads for the IO and File threads so that the DCHECKs
    // to test that we're on the correct thread work.
    sb_service_ =
        base::MakeRefCounted<StrictMock<FakeSafeBrowsingService>>(profile());
    // Set a mock database manager if desired.
    if constexpr (ShouldSetDbManager) {
      sb_service_->SetDatabaseManager(sb_service_->mock_database_manager());
    }
    sb_service_->Initialize();
    ON_CALL(*sb_service_->mock_database_manager(),
            MatchDownloadAllowlistUrl(_, _))
        .WillByDefault(
            [](const GURL& url, base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(false);
            });

    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        sb_service_.get());
    binary_feature_extractor_ =
        base::MakeRefCounted<StrictMock<MockBinaryFeatureExtractor>>();
    ON_CALL(*binary_feature_extractor_, ExtractImageFeatures(_, _, _, _))
        .WillByDefault(Return(true));
    download_service_ = sb_service_->download_protection_service();
#if !BUILDFLAG(IS_ANDROID)
    auto feedback_service = std::make_unique<MockDownloadFeedbackService>();
    feedback_service_ = feedback_service.get();
    download_service_->feedback_service_ = std::move(feedback_service);
#endif
    download_service_->binary_feature_extractor_ = binary_feature_extractor_;
    download_service_->SetEnabled(true);
    client_download_request_subscription_ =
        download_service_->RegisterClientDownloadRequestCallback(
            base::BindRepeating(
                &DownloadProtectionServiceTestBase<
                    ShouldSetDbManager>::OnClientDownloadRequest,
                base::Unretained(this)));
    file_system_access_write_request_subscription_ =
        download_service_->RegisterFileSystemAccessWriteRequestCallback(
            base::BindRepeating(
                &DownloadProtectionServiceTestBase<
                    ShouldSetDbManager>::OnClientDownloadRequest,
                base::Unretained(this), nullptr));
    has_result_ = false;

    base::FilePath source_path;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_path));
    testdata_path_ = source_path.AppendASCII("chrome")
                         .AppendASCII("test")
                         .AppendASCII("data")
                         .AppendASCII("safe_browsing")
                         .AppendASCII("download_protection");

    // Setup a directory to place test files in.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Turn off binary sampling by default.
    SetBinarySamplingProbability(0.0);

#if BUILDFLAG(ENABLE_EXTENSIONS)
    // |test_event_router_| is owned by KeyedServiceFactory.
    test_event_router_ = extensions::CreateAndUseTestEventRouter(profile());
    extensions::SafeBrowsingPrivateEventRouterFactory::GetInstance()
        ->SetTestingFactory(
            profile(),
            base::BindRepeating(&BuildSafeBrowsingPrivateEventRouter));
#endif

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
    enterprise_connectors::RealtimeReportingClientFactory::GetInstance()
        ->SetTestingFactory(profile(),
                            base::BindRepeating(&BuildRealtimeReportingClient));
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
    client_ = std::make_unique<policy::MockCloudPolicyClient>();

    SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));

    ASSERT_TRUE(testing_profile_manager_.SetUp());

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());

    SyncServiceFactory::GetInstance()->SetTestingFactory(
        profile(), BrowserContextKeyedServiceFactory::TestingFactory());

    sb_service_->CreateTestURLLoaderFactoryForProfile(profile());

    DownloadCoreServiceFactory::GetForBrowserContext(profile())
        ->SetDownloadManagerDelegateForTesting(
            std::make_unique<ChromeDownloadManagerDelegate>(profile()));
  }

  void TearDown() override {
    client_download_request_subscription_ = {};
    file_system_access_write_request_subscription_ = {};
#if !BUILDFLAG(IS_ANDROID)
    feedback_service_ = nullptr;
#endif
    sb_service_->ShutDown();
    sb_service_ = nullptr;
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
    in_process_utility_thread_helper_ = nullptr;
    identity_test_env_adaptor_.reset();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.emplace_back(HistoryServiceFactory::GetInstance(),
                           HistoryServiceFactory::GetDefaultFactory());
    // Use TestPasswordStore to remove a possible race. Normally the
    // PasswordStore does its database manipulation on the DB thread, which
    // creates a possible race during navigation. Specifically the
    // PasswordManager will ignore any forms in a page if the load from the
    // PasswordStore has not completed. ChromePasswordProtectionService uses
    // PasswordStore.
    factories.emplace_back(
        ProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                content::BrowserContext, password_manager::TestPasswordStore>));
    // It's fine to override unconditionally, GetForProfile() will still return
    // null if account storage is disabled.
    // TODO(crbug.com/41489644): Remove the comment above when the account store
    // is always non-null.
    factories.emplace_back(
        AccountPasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStoreWithArgs<
                content::BrowserContext, password_manager::TestPasswordStore,
                password_manager::IsAccountStore>,
            password_manager::IsAccountStore(true)));
    return factories;
  }

  void EnableFeatures(const std::vector<base::test::FeatureRef>& features) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(features, {});
  }

  void DisableFeatures(const std::vector<base::test::FeatureRef>& features) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures({}, features);
  }

  void SetAllowlistedDownloadSampleRate(double target_rate) {
    download_service_->allowlist_sample_rate_ = target_rate;
  }

  // Note: This only works for desktop platforms where the sampling rate depends
  // upon FileTypePolicies.
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
      const std::string& file_path) {
    for (const auto& archived_binary : request.archived_binary()) {
      if (archived_binary.file_path() == file_path) {
        return &archived_binary;
      }
    }
    return nullptr;
  }

  const ClientDownloadRequest* GetClientDownloadRequest() const {
    return last_client_download_request_.get();
  }

  bool HasClientDownloadRequest() const {
    return last_client_download_request_.get() != nullptr;
  }

  void ClearClientDownloadRequest() { last_client_download_request_.reset(); }

  void PrepareSuccessResponseForProfile(Profile* profile,
                                        ClientDownloadResponse::Verdict verdict,
                                        bool upload_requested,
                                        bool request_deep_scan) {
    ClientDownloadResponse response;
    response.set_verdict(verdict);
    response.set_upload(upload_requested);
    response.set_request_deep_scan(request_deep_scan);
    response.set_token("response_token");
    sb_service_->GetTestURLLoaderFactory(profile)->AddResponse(
        download_service_->GetDownloadRequestUrl().spec(),
        response.SerializeAsString());
  }

  void PrepareResponse(ClientDownloadResponse::Verdict verdict,
                       net::HttpStatusCode response_code,
                       int net_error,
                       bool upload_requested = false,
                       bool request_deep_scan = false) {
    if (net_error != net::OK) {
      network::URLLoaderCompletionStatus status;
      sb_service_->GetTestURLLoaderFactory(profile())->AddResponse(
          download_service_->GetDownloadRequestUrl(),
          network::mojom::URLResponseHead::New(), std::string(),
          network::URLLoaderCompletionStatus(net_error));
      return;
    }
    PrepareSuccessResponseForProfile(profile(), verdict, upload_requested,
                                     request_deep_scan);
  }

  // Common setup code for MockDownloadItem.
  // `tmp_path_literal` and `final_path_literal` are ignored on Android.
  void PrepareBasicDownloadItem(
      NiceMockDownloadItem* item,
      const std::vector<std::string> url_chain_items,
      const std::string& referrer_url,
      const base::FilePath::StringType& tmp_path_literal,
      const base::FilePath::StringType& final_path_literal,
      std::optional<base::FilePath> display_name = std::nullopt) {
#if BUILDFLAG(IS_ANDROID)
    // For realism on Android, the final path and temp path are typically
    // content-URIs, and only the display name should be used.
    base::FilePath tmp_path = base::FilePath(kTempContentUri);
    base::FilePath final_path = base::FilePath(kContentUri);

    // Supply a default display name for Android tests that is eligible for
    // download protection.
    display_name = display_name.value_or(base::FilePath(kApkFilename));
#else
    base::FilePath tmp_path = temp_dir_.GetPath().Append(tmp_path_literal);
    base::FilePath final_path = temp_dir_.GetPath().Append(final_path_literal);
#endif
    PrepareBasicDownloadItemWithFullPaths(item, url_chain_items, referrer_url,
                                          tmp_path, final_path, display_name);
  }

#if BUILDFLAG(IS_ANDROID)
  void PrepareBasicDownloadItemWithContentUri(
      NiceMockDownloadItem* item,
      const std::vector<std::string> url_chain_items,
      const std::string& referrer_url,
      const base::FilePath& display_name) {
    PrepareBasicDownloadItem(item, url_chain_items, referrer_url,
                             /*ignored*/ base::FilePath::StringType(),
                             /*ignored*/ base::FilePath::StringType(),
                             display_name);
  }
#endif

  void PrepareBasicDownloadItemWithFullPaths(
      NiceMockDownloadItem* item,
      const std::vector<std::string> url_chain_items,
      const std::string& referrer_url,
      const base::FilePath& tmp_full_path,
      const base::FilePath& final_full_path,
      std::optional<base::FilePath> display_name = std::nullopt) {
    url_chain_.clear();
    for (std::string url_chain_item : url_chain_items) {
      url_chain_.emplace_back(url_chain_item);
    }
    if (url_chain_.empty()) {
      url_chain_.emplace_back();
    }
    referrer_ = GURL(referrer_url);
    tmp_path_ = tmp_full_path;
    final_path_ = final_full_path;
    display_name_ = display_name.value_or(final_path_.BaseName());
    hash_ = "hash";

    EXPECT_CALL(*item, GetURL()).WillRepeatedly([&]() -> const GURL& {
      return url_chain_.back();
    });
    EXPECT_CALL(*item, GetFullPath()).WillRepeatedly(ReturnRef(tmp_path_));
    EXPECT_CALL(*item, GetTargetFilePath())
        .WillRepeatedly(ReturnRef(final_path_));
    EXPECT_CALL(*item, GetFileNameToReportUser())
        .WillRepeatedly(Return(display_name_));
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

  std::unique_ptr<FileSystemAccessWriteItem>
  PrepareBasicFileSystemAccessWriteItem(
      const base::FilePath::StringType& tmp_path_literal,
      const base::FilePath::StringType& final_path_literal) {
    base::FilePath tmp_path = temp_dir_.GetPath().Append(tmp_path_literal);
    base::FilePath final_path = temp_dir_.GetPath().Append(final_path_literal);
    return PrepareBasicFileSystemAccessWriteItemWithFullPaths(tmp_path,
                                                              final_path);
  }

  std::unique_ptr<FileSystemAccessWriteItem>
  PrepareBasicFileSystemAccessWriteItemWithFullPaths(
      const base::FilePath& tmp_path,
      const base::FilePath& final_path) {
    tmp_path_ = tmp_path;
    final_path_ = final_path;
    hash_ = "hash";
    auto result = std::make_unique<FileSystemAccessWriteItem>();
    result->target_file_path = final_path;
    result->full_path = tmp_path;
    result->sha256_hash = hash_;
    result->size = 100;
    result->frame_url = GURL("https://example.com/foo/bar");
    result->has_user_gesture = true;
    result->web_contents = web_contents()->GetWeakPtr();
    result->browser_context = profile();
    return result;
  }

  std::unique_ptr<FileSystemAccessWriteItem> CloneFileSystemAccessWriteItem(
      FileSystemAccessWriteItem* in) {
    auto result = std::make_unique<FileSystemAccessWriteItem>();
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

  void AddDomainToEnterpriseAllowlist(const std::string& domain) {
    ScopedListPrefUpdate update(profile()->GetPrefs(),
                                prefs::kSafeBrowsingAllowlistDomains);
    update->Append(domain);
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
            url, web_contents()->GetPrimaryMainFrame());

    navigation->SetTransition(ui::PAGE_TRANSITION_LINK);
    navigation->Commit();
  }

 private:
  void OnClientDownloadRequest(download::DownloadItem* download,
                               const ClientDownloadRequest* request) {
    if (request) {
      last_client_download_request_ =
          std::make_unique<ClientDownloadRequest>(*request);
    } else {
      last_client_download_request_.reset();
    }
  }

 public:
  enum ArchiveType { kZip, kDmg };

  void CheckDoneCallback(base::OnceClosure quit_closure,
                         DownloadCheckResult result) {
    result_ = result;
    has_result_ = true;

    // Scanning has not completed in this case, allow it to continue.
    if (result != DownloadCheckResult::ASYNC_SCANNING) {
      std::move(quit_closure).Run();
    }
  }

  void SyncCheckDoneCallback(DownloadCheckResult result) {
    result_ = result;
    has_result_ = true;
  }

  testing::AssertionResult IsResult(DownloadCheckResult expected) {
    if (!has_result_) {
      return testing::AssertionFailure() << "No result";
    }
    has_result_ = false;
    return result_ == expected
               ? testing::AssertionSuccess()
               : testing::AssertionFailure()
                     << "Expected " << DownloadCheckResultToString(expected)
                     << ", got " << DownloadCheckResultToString(result_);
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
#if !BUILDFLAG(IS_ANDROID)
  raw_ptr<MockDownloadFeedbackService> feedback_service_;
#endif
  raw_ptr<DownloadProtectionService, DanglingUntriaged> download_service_;
  DownloadCheckResult result_;
  bool has_result_;
  std::unique_ptr<content::InProcessUtilityThreadHelper>
      in_process_utility_thread_helper_;
  base::FilePath testdata_path_;
  base::CallbackListSubscription client_download_request_subscription_;
  base::CallbackListSubscription file_system_access_write_request_subscription_;
  std::unique_ptr<ClientDownloadRequest> last_client_download_request_;
  // The following 6 fields are used by PrepareBasicDownloadItem() function to
  // store attributes of the last download item. They can be modified
  // afterwards and the *item will return the new values.
  std::vector<GURL> url_chain_;
  GURL referrer_;
  base::FilePath tmp_path_;
  base::FilePath final_path_;
  base::FilePath display_name_;
  std::string hash_;
  base::ScopedTempDir temp_dir_;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  raw_ptr<extensions::TestEventRouter, DanglingUntriaged> test_event_router_;
#endif
  TestingProfileManager testing_profile_manager_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
};

// These tests use a mock database manager. By default on desktop platforms, the
// test fixture uses the mock database manager created by the
// TestSafeBrowsingService acting as ServicesCreator, so it doesn't need to be
// explicitly set. But on Android it must be set explicitly because
// ServicesDelegateAndroid doesn't respect CanCreateDatabaseManager().
#if BUILDFLAG(IS_ANDROID)
using DownloadProtectionServiceTest =
    DownloadProtectionServiceTestBase</*ShouldSetDbManager=*/true>;
#else
using DownloadProtectionServiceTest =
    DownloadProtectionServiceTestBase</*ShouldSetDbManager=*/false>;
#endif

using DeepScanningDownloadTest = DownloadProtectionServiceTest;

class DownloadProtectionServiceMockTimeTest
    : public DownloadProtectionServiceTest {
 public:
  DownloadProtectionServiceMockTimeTest()
      : DownloadProtectionServiceTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

// A test with the appropriate feature flags enabled to test the behavior for
// Enhanced Protection users.
class EnhancedProtectionDownloadTest : public DownloadProtectionServiceTest {
 public:
  EnhancedProtectionDownloadTest() {
#if BUILDFLAG(IS_ANDROID)
    EnableFeatures({kMaliciousApkDownloadCheck});
#endif
  }
};

template <bool ShouldSetDbManager>
void DownloadProtectionServiceTestBase<ShouldSetDbManager>::
    CheckClientDownloadReportCorruptArchive(ArchiveType type) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  NiceMockDownloadItem item;
  if (type == kZip) {
    PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.zip"},  // url_chain
                             "http://www.google.com/",              // referrer
                             FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                             FILE_PATH_LITERAL("a.zip"));  // final_path
    content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  } else if (type == kDmg) {
    PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.dmg"},  // url_chain
                             "http://www.google.com/",              // referrer
                             FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                             FILE_PATH_LITERAL("a.dmg"));  // final_path
    content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  }

  std::string file_contents = "corrupt archive file";
  ASSERT_TRUE(base::WriteFile(tmp_path_, file_contents));

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTestBase<
                              ShouldSetDbManager>::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(HasClientDownloadRequest());
  EXPECT_EQ(0, GetClientDownloadRequest()->archived_binary_size());
  EXPECT_TRUE(GetClientDownloadRequest()->has_download_type());
  ClientDownloadRequest::DownloadType expected_type =
      type == kZip
          ? ClientDownloadRequest_DownloadType_INVALID_ZIP
          : ClientDownloadRequest_DownloadType_MAC_ARCHIVE_FAILED_PARSING;
  EXPECT_EQ(expected_type, GetClientDownloadRequest()->download_type());
  ClearClientDownloadRequest();

  Mock::VerifyAndClearExpectations(sb_service_.get());
  Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
}

// TODO(crbug.com/41319255): Create specific unit tests for
// check_client_download_request.*, download_url_sb_client.*.
TEST_F(DownloadProtectionServiceTest, CheckClientDownloadInvalidUrl) {
  NiceMockDownloadItem item;
  {
    PrepareBasicDownloadItem(&item,
                             std::vector<std::string>(),   // empty url_chain
                             "http://www.google.com/",     // referrer
                             FILE_PATH_LITERAL("a.tmp"),   // tmp_path
                             FILE_PATH_LITERAL("a.exe"));  // final_path

    // Though MayCheckClientDownload returns false, if CheckClientDownload()
    // were in fact called anyway, test that we do not ultimately send a ping.
    EXPECT_FALSE(download_service_->delegate()->MayCheckClientDownload(&item));
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

    // Though MayCheckClientDownload returns false, if CheckClientDownload()
    // were in fact called anyway, test that we do not ultimately send a ping.
    EXPECT_FALSE(download_service_->delegate()->MayCheckClientDownload(&item));
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
  PrepareBasicDownloadItem(
      &item, {"http://www.evil.com/a.txt"},  // url_chain
      "http://www.google.com/",              // referrer
      FILE_PATH_LITERAL("a.tmp"),            // tmp_path
      FILE_PATH_LITERAL("a.txt"),            // final_path
      // Do not use the default APK filename override for Android in
      // PrepareBasicDownloadItem.
      base::FilePath(FILE_PATH_LITERAL("a.txt")));  // display_name
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  EXPECT_EQ(
      download_service_->delegate()->IsSupportedDownload(item, final_path_),
      MayCheckDownloadResult::kMaySendSampledPingOnly);

  // This returns true because of the possibility of sampling an unsupported
  // file type.
  RunLoop run_loop;
  EXPECT_TRUE(download_service_->MaybeCheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure())));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
  // But the ping is not ultimately sent because binary sampling is off.
  EXPECT_FALSE(HasClientDownloadRequest());
}

TEST_F(DownloadProtectionServiceTest,
       CheckClientDownloadAllowlistedUrlWithoutSampling) {
  // Response to any requests will be DANGEROUS.
  PrepareResponse(ClientDownloadResponse::DANGEROUS, net::HTTP_OK, net::OK);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item,
                           std::vector<std::string>(),   // empty url_chain
                           "",                           // referrer
                           FILE_PATH_LITERAL("a.tmp"),   // tmp_path
                           FILE_PATH_LITERAL("a.exe"));  // final_path
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(3);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(3);

  // We should not get allowlist checks for other URLs than specified below.
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .Times(0);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(GURL("http://www.evil.com/bla.exe"), _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(GURL("http://www.google.com/a.exe"), _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(true);
          });

  // Set sample rate to 0 to prevent sampling.
  SetAllowlistedDownloadSampleRate(0);
  {
    // With no referrer and just the bad url, should be marked DANGEROUS.
    url_chain_.emplace_back("http://www.evil.com/bla.exe");
    RunLoop run_loop;
    EXPECT_TRUE(download_service_->MaybeCheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure())));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::DANGEROUS));
    ASSERT_TRUE(HasClientDownloadRequest());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_url_allowlist());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_certificate_allowlist());
    ClearClientDownloadRequest();
  }
  {
    // Check that the referrer is not matched against the allowlist.
    referrer_ = GURL("http://www.google.com/");
    RunLoop run_loop;
    EXPECT_TRUE(download_service_->MaybeCheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure())));
    run_loop.Run();

    EXPECT_TRUE(IsResult(DownloadCheckResult::DANGEROUS));
    ASSERT_TRUE(HasClientDownloadRequest());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_url_allowlist());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_certificate_allowlist());
    ClearClientDownloadRequest();
  }

  {
    // Redirect from a site shouldn't be checked either.
    url_chain_.emplace(url_chain_.begin(), "http://www.google.com/redirect");
    RunLoop run_loop;
    EXPECT_TRUE(download_service_->MaybeCheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure())));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::DANGEROUS));
    ASSERT_TRUE(HasClientDownloadRequest());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_url_allowlist());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_certificate_allowlist());
    ClearClientDownloadRequest();
  }
  {
    // Only if the final url is allowlisted should it be SAFE.
    url_chain_.emplace_back("http://www.google.com/a.exe");
    RunLoop run_loop;
    EXPECT_TRUE(download_service_->MaybeCheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure())));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    // TODO(grt): Make the service produce the request even when the URL is
    // allowlisted.
    EXPECT_FALSE(HasClientDownloadRequest());
  }
}

TEST_F(DownloadProtectionServiceTest,
       CheckClientDownloadAllowlistedUrlWithSampling) {
  // Server responses "SAFE" to every requests coming from allowlisted
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
      .Times(1);
  // Assume http://www.allowlist.com/a.exe is on the allowlist.
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .Times(0);
  EXPECT_CALL(
      *sb_service_->mock_database_manager(),
      MatchDownloadAllowlistUrl(GURL("http://www.allowlist.com/a.exe"), _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(true);
          });
  url_chain_.emplace_back("http://www.allowlist.com/a.exe");
  // Set sample rate to 1.00, so download_service_ will always send download
  // pings for allowlisted downloads.
  SetAllowlistedDownloadSampleRate(1.00);

  {
    // Case (1): is_extended_reporting && is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    SetExtendedReportingPreference(true);
    content::DownloadItemUtils::AttachInfoForTesting(
        &item, profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
        nullptr);
    RunLoop run_loop;
    EXPECT_TRUE(download_service_->MaybeCheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure())));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
  {
    // Case (2): !is_extended_reporting && is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    SetExtendedReportingPreference(false);
    content::DownloadItemUtils::AttachInfoForTesting(
        &item, profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
        nullptr);
    RunLoop run_loop;
    EXPECT_TRUE(download_service_->MaybeCheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure())));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
  {
    // Case (3): !is_extended_reporting && !is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
    RunLoop run_loop;
    EXPECT_TRUE(download_service_->MaybeCheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure())));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
  {
    // Case (4): is_extended_reporting && !is_incognito &&
    //           Download matches URL allowlist.
    //           ClientDownloadRequest should be sent.
    SetExtendedReportingPreference(true);
    content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
    RunLoop run_loop;
    EXPECT_TRUE(download_service_->MaybeCheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure())));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    ASSERT_TRUE(HasClientDownloadRequest());
    EXPECT_TRUE(GetClientDownloadRequest()->skipped_url_allowlist());
    EXPECT_FALSE(GetClientDownloadRequest()->skipped_certificate_allowlist());
    ClearClientDownloadRequest();
  }
}

// "Light" pings for sampled files are not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
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
    content::DownloadItemUtils::AttachInfoForTesting(
        &item, profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
        nullptr);
    RunLoop run_loop;
    // This returns true because of the possibility of sampling an unsupported
    // file type.
    EXPECT_TRUE(download_service_->MaybeCheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure())));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
  {
    // Case (2): is_extended_reporting && !is_incognito.
    //           A "light" ClientDownloadRequest should be sent.
    content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
    RunLoop run_loop;
    EXPECT_TRUE(download_service_->MaybeCheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure())));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
    ASSERT_TRUE(HasClientDownloadRequest());

    // Verify it's a "light" ping, check that URLs don't have paths.
    auto* req = GetClientDownloadRequest();
    EXPECT_EQ(ClientDownloadRequest::SAMPLED_UNSUPPORTED_FILE,
              req->download_type());
    EXPECT_EQ(GURL(req->url()).DeprecatedGetOriginAsURL().spec(), req->url());
    for (auto resource : req->resources()) {
      EXPECT_EQ(GURL(resource.url()).DeprecatedGetOriginAsURL().spec(),
                resource.url());
      EXPECT_EQ(GURL(resource.referrer()).DeprecatedGetOriginAsURL().spec(),
                resource.referrer());
    }
    ClearClientDownloadRequest();
  }
  {
    // Case (3): !is_extended_reporting && is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    SetExtendedReportingPreference(false);
    content::DownloadItemUtils::AttachInfoForTesting(
        &item, profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
        nullptr);
    RunLoop run_loop;
    EXPECT_TRUE(download_service_->MaybeCheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure())));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
  {
    // Case (4): !is_extended_reporting && !is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
    RunLoop run_loop;
    EXPECT_TRUE(download_service_->MaybeCheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure())));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
}
#endif

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadFetchFailed) {
  base::HistogramTester histogram_tester;
  // HTTP request will fail.
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_INTERNAL_SERVER_ERROR,
                  net::ERR_FAILED);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
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
  histogram_tester.ExpectUniqueSample(
      /*name=*/"SBClientDownload.DownloadRequestNetworkResult",
      /*sample=*/net::ERR_FAILED,
      /*expected_bucket_count=*/1);
}

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadSuccess) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(9);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(9);
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);
#if !BUILDFLAG(IS_ANDROID)
  std::string feedback_ping;
  std::string feedback_response;
#endif
  ClientDownloadResponse expected_response;

  {
    base::HistogramTester histogram_tester;
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    EXPECT_TRUE(HasClientDownloadRequest());
    histogram_tester.ExpectUniqueSample(
        /*name=*/"SBClientDownload.DownloadRequestNetworkResult",
        /*sample=*/200,
        /*expected_bucket_count=*/1);
    ClearClientDownloadRequest();
  }
  {
    // Invalid response should result in SAFE (default value in proto).
    ClientDownloadResponse invalid_response;
    sb_service_->GetTestURLLoaderFactory(profile())->AddResponse(
        download_service_->GetDownloadRequestUrl().spec(),
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
#if !BUILDFLAG(IS_ANDROID)
    Mock::VerifyAndClearExpectations(feedback_service_);
#endif
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
    EXPECT_TRUE(IsResult(DownloadCheckResult::DANGEROUS));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
#if !BUILDFLAG(IS_ANDROID)
    Mock::VerifyAndClearExpectations(feedback_service_);
#endif
  }
  {
    // If the response is dangerous and the server requests an upload,
    // we should upload.
    PrepareResponse(ClientDownloadResponse::DANGEROUS, net::HTTP_OK, net::OK,
                    true /* upload_requested */);
#if !BUILDFLAG(IS_ANDROID)
    EXPECT_CALL(*feedback_service_,
                BeginFeedbackForDownload(profile(), &item, _, _));
#endif
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
#if !BUILDFLAG(IS_ANDROID)
    Mock::VerifyAndClearExpectations(feedback_service_);
#endif
    EXPECT_TRUE(IsResult(DownloadCheckResult::DANGEROUS));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
  {
    // If the response is uncommon the result should also be marked as uncommon.
    PrepareResponse(ClientDownloadResponse::UNCOMMON, net::HTTP_OK, net::OK,
                    true /* upload_requested */);
#if !BUILDFLAG(IS_ANDROID)
    EXPECT_CALL(*feedback_service_,
                BeginFeedbackForDownload(profile(), &item, _, _))
        .WillOnce(
            DoAll(SaveArg<2>(&feedback_ping), SaveArg<3>(&feedback_response)));
#endif
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNCOMMON));
#if !BUILDFLAG(IS_ANDROID)
    Mock::VerifyAndClearExpectations(feedback_service_);
    ClientDownloadRequest decoded_request;
    EXPECT_TRUE(decoded_request.ParseFromString(feedback_ping));
    EXPECT_EQ(url_chain_.back().spec(), decoded_request.url());
    expected_response.set_verdict(ClientDownloadResponse::UNCOMMON);
    expected_response.set_upload(true);
    expected_response.set_request_deep_scan(false);
    expected_response.set_token("response_token");
    EXPECT_EQ(expected_response.SerializeAsString(), feedback_response);
#endif
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
  {
    // If the response is dangerous_host the result should also be marked as
    // dangerous_host.
    PrepareResponse(ClientDownloadResponse::DANGEROUS_HOST, net::HTTP_OK,
                    net::OK, true /* upload_requested */);
#if !BUILDFLAG(IS_ANDROID)
    EXPECT_CALL(*feedback_service_,
                BeginFeedbackForDownload(profile(), &item, _, _))
        .WillOnce(
            DoAll(SaveArg<2>(&feedback_ping), SaveArg<3>(&feedback_response)));
#endif
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::DANGEROUS_HOST));
#if !BUILDFLAG(IS_ANDROID)
    Mock::VerifyAndClearExpectations(feedback_service_);
    expected_response.set_verdict(ClientDownloadResponse::DANGEROUS_HOST);
    expected_response.set_upload(true);
    EXPECT_EQ(expected_response.SerializeAsString(), feedback_response);
#endif
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }

  {
    // If the response is dangerous_account_compromise the result should
    // also be marked as dangerous_account_compromise.

    PrepareResponse(ClientDownloadResponse::DANGEROUS_ACCOUNT_COMPROMISE,
                    net::HTTP_OK, net::OK, true /* upload_requested */);
#if !BUILDFLAG(IS_ANDROID)
    EXPECT_CALL(*feedback_service_,
                BeginFeedbackForDownload(profile(), &item, _, _))
        .WillOnce(
            DoAll(SaveArg<2>(&feedback_ping), SaveArg<3>(&feedback_response)));
#endif
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::DANGEROUS_ACCOUNT_COMPROMISE));
#if !BUILDFLAG(IS_ANDROID)
    Mock::VerifyAndClearExpectations(feedback_service_);
    expected_response.set_verdict(
        ClientDownloadResponse::DANGEROUS_ACCOUNT_COMPROMISE);
    expected_response.set_upload(true);
    EXPECT_EQ(expected_response.SerializeAsString(), feedback_response);
#endif
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
#if !BUILDFLAG(IS_ANDROID)
    EXPECT_CALL(*feedback_service_,
                BeginFeedbackForDownload(profile(), &item, _, _))
        .WillOnce(
            DoAll(SaveArg<2>(&feedback_ping), SaveArg<3>(&feedback_response)));
#endif
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
#if !BUILDFLAG(IS_ANDROID)
    Mock::VerifyAndClearExpectations(feedback_service_);
#endif
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
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
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
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
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
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
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

// Zip file analysis is not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(DownloadProtectionServiceTest, CheckClientDownloadZip) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.zip"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.zip"));           // final_path
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  // Write out a zip archive to the temporary file.
  base::ScopedTempDir zip_source_dir;
  ASSERT_TRUE(zip_source_dir.CreateUniqueTempDir());
  std::string file_contents = "dummy file";
  {
    // In this case, it only contains a text file.
    ASSERT_TRUE(base::WriteFile(
        zip_source_dir.GetPath().Append(FILE_PATH_LITERAL("file.txt")),
        file_contents));
    // Create a unique temp file to avoid conflicts. These will get cleaned up
    // with `temp_dir_` eventually.
    ASSERT_TRUE(
        base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &tmp_path_));
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
    ASSERT_TRUE(base::WriteFile(
        zip_source_dir.GetPath().Append(FILE_PATH_LITERAL("file.exe")),
        file_contents));
    ASSERT_TRUE(
        base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &tmp_path_));
    ASSERT_TRUE(zip::Zip(zip_source_dir.GetPath(), tmp_path_, false));
    EXPECT_CALL(*sb_service_->mock_database_manager(),
                MatchDownloadAllowlistUrl(_, _))
        .WillRepeatedly(
            [](const GURL& url, base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(false);
            });
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
    ASSERT_TRUE(base::WriteFile(
        zip_source_dir.GetPath().Append(FILE_PATH_LITERAL("file.rar")),
        file_contents));
    ASSERT_TRUE(
        base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &tmp_path_));
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
    ASSERT_TRUE(
        base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &tmp_path_));
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
  CheckClientDownloadReportCorruptArchive(kZip);
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
TEST_F(DownloadProtectionServiceTest, CheckClientDownloadReportCorruptDmg) {
  CheckClientDownloadReportCorruptArchive(kDmg);
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
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(HasClientDownloadRequest());
  EXPECT_EQ(GetClientDownloadRequest()->archive_summary().parser_status(),
            ClientDownloadRequest::ArchiveSummary::VALID);

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
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

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
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(HasClientDownloadRequest());
  EXPECT_EQ(GetClientDownloadRequest()->archive_summary().parser_status(),
            ClientDownloadRequest::ArchiveSummary::VALID);
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
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(HasClientDownloadRequest());
  EXPECT_EQ(GetClientDownloadRequest()->archive_summary().parser_status(),
            ClientDownloadRequest::ArchiveSummary::VALID);
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
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(HasClientDownloadRequest());
  EXPECT_NE(GetClientDownloadRequest()->archive_summary().parser_status(),
            ClientDownloadRequest::ArchiveSummary::VALID);
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
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

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
  EXPECT_NE(GetClientDownloadRequest()->archive_summary().parser_status(),
            ClientDownloadRequest::ArchiveSummary::VALID);
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
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(HasClientDownloadRequest());

  auto* request = GetClientDownloadRequest();

  EXPECT_EQ(GetClientDownloadRequest()->archive_summary().parser_status(),
            ClientDownloadRequest::ArchiveSummary::VALID);
  EXPECT_FALSE(request->has_udif_code_signature());
  EXPECT_EQ(ClientDownloadRequest_DownloadType_MAC_EXECUTABLE,
            request->download_type());

  ASSERT_EQ(2, request->archived_binary().size());
  for (const auto& binary : request->archived_binary()) {
    EXPECT_FALSE(binary.file_path().empty());
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

#endif  // BUILDFLAG(IS_MAC)

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadValidateRequest) {
#if BUILDFLAG(IS_MAC)
  std::string download_file_path("ftp://www.google.com/bla.dmg");
#else
  std::string download_file_path("ftp://www.google.com/bla.exe");
#endif  //  OS_MAC

  NiceMockDownloadItem item;
#if BUILDFLAG(IS_MAC)
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
#endif  // BUILDFLAG(IS_MAC)
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  std::string remote_address = "10.11.12.13";
  EXPECT_CALL(item, GetRemoteAddress()).WillRepeatedly(Return(remote_address));

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
#if !BUILDFLAG(IS_MAC)
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .WillOnce(SetCertificateContents("dummy cert data"));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .WillOnce(SetDosHeaderContents("dummy dos header"));
#endif  // BUILDFLAG(IS_MAC)

  sb_service_->GetTestURLLoaderFactory(profile())->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_TRUE(
            request.headers.GetHeader(net::HttpRequestHeaders::kContentType)
                .has_value());
        std::string upload_data = network::GetUploadData(request);
        EXPECT_FALSE(upload_data.empty());
      }));

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
#if !BUILDFLAG(IS_MAC)
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
#endif  // BUILDFLAG(IS_MAC)
}

// Similar to above, but with an unsigned binary.
TEST_F(DownloadProtectionServiceTest,
       CheckClientDownloadValidateRequestNoSignature) {
#if BUILDFLAG(IS_MAC)
  std::string download_file_path("ftp://www.google.com/bla.dmg");
#else
  std::string download_file_path("ftp://www.google.com/bla.exe");
#endif  // BUILDFLAG(IS_MAC)

  NiceMockDownloadItem item;
#if BUILDFLAG(IS_MAC)
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
#endif  // BUILDFLAG(IS_MAC)
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  std::string remote_address = "10.11.12.13";
  EXPECT_CALL(item, GetRemoteAddress()).WillRepeatedly(Return(remote_address));
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
#if !BUILDFLAG(IS_MAC)
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));
#endif  // BUILDFLAG(IS_MAC)

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
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  GURL tab_url("http://tab.com/final");
  GURL tab_referrer("http://tab.com/referrer");
  std::string remote_address = "10.11.12.13";
  EXPECT_CALL(item, GetTabUrl()).WillRepeatedly(ReturnRef(tab_url));
  EXPECT_CALL(item, GetTabReferrerUrl())
      .WillRepeatedly(ReturnRef(tab_referrer));
  EXPECT_CALL(item, GetRemoteAddress()).WillRepeatedly(Return(remote_address));
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
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
    sb_service_->GetTestURLLoaderFactory(profile())->SetInterceptor(
        base::BindLambdaForTesting(
            [&](const network::ResourceRequest& request) {
              EXPECT_TRUE(request.headers
                              .GetHeader(net::HttpRequestHeaders::kContentType)
                              .has_value());
              upload_data = network::GetUploadData(request);
              if (!upload_data.empty()) {
                interceptor_run_loop.Quit();
              }
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

    sb_service_->GetTestURLLoaderFactory(profile())->SetInterceptor(
        network::TestURLLoaderFactory::Interceptor());

    // Simulate the request finishing.
    run_loop.Run();
  }

  // Now try with a history match.
  {
    RunLoop interceptor_run_loop;

    std::string upload_data;
    sb_service_->GetTestURLLoaderFactory(profile())->SetInterceptor(
        base::BindLambdaForTesting(
            [&](const network::ResourceRequest& request) {
              EXPECT_TRUE(request.headers
                              .GetHeader(net::HttpRequestHeaders::kContentType)
                              .has_value());
              upload_data = network::GetUploadData(request);
              if (!upload_data.empty()) {
                interceptor_run_loop.Quit();
              }
            }));

    history::RedirectList redirects;
    redirects.emplace_back("http://tab.com/ref1");
    redirects.emplace_back("http://tab.com/ref2");
    redirects.push_back(tab_url);
    HistoryServiceFactory::GetForProfile(profile(),
                                         ServiceAccessType::EXPLICIT_ACCESS)
        ->AddPage(tab_url, base::Time::Now(), 1, 0, GURL(), redirects,
                  ui::PAGE_TRANSITION_TYPED, history::SOURCE_BROWSED,
                  history::VisitResponseCodeCategory::kNot404, false);

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
  url_chain.emplace_back("http://www.google.com/");
  url_chain.emplace_back("http://www.google.com/bla.exe");
  GURL referrer("http://www.google.com/");
  std::string hash = "hash";

  NiceMockDownloadItem item;
  EXPECT_CALL(item, GetURL()).WillRepeatedly(ReturnRef(url_chain.back()));
  EXPECT_CALL(item, GetUrlChain()).WillRepeatedly(ReturnRef(url_chain));
  EXPECT_CALL(item, GetReferrerUrl()).WillRepeatedly(ReturnRef(referrer));
  EXPECT_CALL(item, GetHash()).WillRepeatedly(ReturnRef(hash));

  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(),
                                                   web_contents.get());
  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);

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
        .WillOnce(DoAll(CheckDownloadUrlDone(SBThreatType::SB_THREAT_TYPE_SAFE),
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
        .WillOnce(DoAll(
            CheckDownloadUrlDone(SBThreatType::SB_THREAT_TYPE_URL_MALWARE),
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
        .WillOnce(DoAll(CheckDownloadUrlDone(
                            SBThreatType::SB_THREAT_TYPE_URL_BINARY_MALWARE),
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
       TestCheckDownloadUrlOnPolicyAllowlistedDownload) {
  AddDomainToEnterpriseAllowlist("example.com");

  // Prepares download item that its download url (last url in url chain)
  // matches enterprise allowlist.
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(
      &item,
      {"https://landingpage.com", "http://example.com/download_page/a.exe"},
      "http://referrer.com",        // referrer
      FILE_PATH_LITERAL("a.tmp"),   // tmp_path
      FILE_PATH_LITERAL("a.exe"));  // final_path
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(),
                                                   web_contents.get());
  EXPECT_CALL(*sb_service_->mock_database_manager(), CheckDownloadUrl(_, _))
      .Times(0);
  RunLoop run_loop;
  download_service_->CheckDownloadUrl(
      &item, base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));

  // Prepares download item that other url in the url chain matches enterprise
  // allowlist.
  NiceMockDownloadItem item2;
  PrepareBasicDownloadItem(
      &item2, {"https://example.com/landing", "http://otherdomain.com/a.exe"},
      "http://referrer.com",        // referrer
      FILE_PATH_LITERAL("a.tmp"),   // tmp_path
      FILE_PATH_LITERAL("a.exe"));  // final_path
  content::DownloadItemUtils::AttachInfoForTesting(&item2, profile(),
                                                   web_contents.get());
  EXPECT_CALL(*sb_service_->mock_database_manager(), CheckDownloadUrl(_, _))
      .Times(0);
  RunLoop run_loop2;
  download_service_->CheckDownloadUrl(
      &item2, base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                             base::Unretained(this), run_loop2.QuitClosure()));
  run_loop2.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
}

TEST_F(DownloadProtectionServiceMockTimeTest, TestDownloadRequestTimeout) {
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/bla.exe"},  // url_chain
                           "http://www.google.com/",                // referrer
                           FILE_PATH_LITERAL("a.tmp"),              // tmp_path
                           FILE_PATH_LITERAL("a.exe"));  // final_path
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));

  task_environment()->FastForwardBy(
      download_service_->GetDownloadRequestTimeout());

  // The request should time out because the HTTP request hasn't returned
  // anything yet.
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
  EXPECT_TRUE(HasClientDownloadRequest());
  ClearClientDownloadRequest();
}

TEST_F(DownloadProtectionServiceTest, TestDownloadItemDestroyed) {
  base::RunLoop run_loop;
  {
    NiceMockDownloadItem item;
    PrepareBasicDownloadItem(&item,
                             {"http://www.evil.com/bla.exe"},  // url_chain
                             "http://www.google.com/",         // referrer
                             FILE_PATH_LITERAL("a.tmp"),       // tmp_path
                             FILE_PATH_LITERAL("a.exe"));      // final_path
    content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
    GURL tab_url("http://www.google.com/tab");
    EXPECT_CALL(item, GetTabUrl()).WillRepeatedly(ReturnRef(tab_url));
    EXPECT_CALL(*sb_service_->mock_database_manager(),
                MatchDownloadAllowlistUrl(_, _))
        .WillRepeatedly(
            [](const GURL& url, base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(false);
            });

    int expect_count = 1;

    // Note 'AtMost' is used because on Mac timing differences make the mocks
    // not reached.
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
        .Times(AtMost(expect_count));
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
        .Times(AtMost(expect_count));

    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    // MockDownloadItem going out of scope triggers the OnDownloadDestroyed
    // notification.
  }

  // Result won't be immediately available as it is being posted.
  EXPECT_FALSE(has_result_);
  run_loop.Run();

  // When download is destroyed, no need to check for client download request
  // result.
  EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
  EXPECT_FALSE(HasClientDownloadRequest());
}

TEST_F(DownloadProtectionServiceTest,
       TestDownloadItemDestroyedDuringAllowlistCheck) {
  std::unique_ptr<NiceMockDownloadItem> item(new NiceMockDownloadItem);
  PrepareBasicDownloadItem(item.get(),
                           {"http://www.evil.com/bla.exe"},  // url_chain
                           "http://www.google.com/",         // referrer
                           FILE_PATH_LITERAL("a.tmp"),       // tmp_path
                           FILE_PATH_LITERAL("a.exe"));      // final_path
  content::DownloadItemUtils::AttachInfoForTesting(item.get(), profile(),
                                                   nullptr);
  GURL tab_url("http://www.google.com/tab");
  EXPECT_CALL(*item, GetTabUrl()).WillRepeatedly(ReturnRef(tab_url));

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [&item](const GURL&, base::OnceCallback<void(bool)> callback) {
            item.reset();
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(0);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(0);

  base::RunLoop run_loop;
  download_service_->CheckClientDownload(
      item.get(),
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(has_result_);
  EXPECT_FALSE(HasClientDownloadRequest());
}

namespace {

class MockPageNavigator : public content::PageNavigator {
 public:
  MOCK_METHOD2(OpenURL,
               content::WebContents*(
                   const content::OpenURLParams&,
                   base::OnceCallback<void(content::NavigationHandle&)>));
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
  EXPECT_CALL(mock_page_navigator,
              OpenURL(OpenURLParamsWithContextValue("7"), _));

  download_service_->ShowDetailsForDownload(&mock_download_item,
                                            &mock_page_navigator);
}

TEST_F(DownloadProtectionServiceTest, GetAndSetDownloadProtectionData) {
  NiceMockDownloadItem item;
  EXPECT_TRUE(DownloadProtectionService::GetDownloadPingToken(&item).empty());
  EXPECT_FALSE(DownloadProtectionService::HasDownloadProtectionVerdict(&item));
  std::string token = "download_ping_token";
  ClientDownloadResponse::Verdict verdict = ClientDownloadResponse::DANGEROUS;
  ClientDownloadResponse::TailoredVerdict tailored_verdict;
  tailored_verdict.set_tailored_verdict_type(
      ClientDownloadResponse::TailoredVerdict::COOKIE_THEFT);
  DownloadProtectionService::SetDownloadProtectionData(&item, token, verdict,
                                                       tailored_verdict);
  EXPECT_EQ(token, DownloadProtectionService::GetDownloadPingToken(&item));
  EXPECT_TRUE(DownloadProtectionService::HasDownloadProtectionVerdict(&item));
  EXPECT_EQ(verdict,
            DownloadProtectionService::GetDownloadProtectionVerdict(&item));
  EXPECT_EQ(
      ClientDownloadResponse::TailoredVerdict::COOKIE_THEFT,
      DownloadProtectionService::GetDownloadProtectionTailoredVerdict(&item)
          .tailored_verdict_type());

  DownloadProtectionService::SetDownloadProtectionData(
      &item, std::string(), ClientDownloadResponse::SAFE,
      ClientDownloadResponse::TailoredVerdict());
  EXPECT_TRUE(DownloadProtectionService::GetDownloadPingToken(&item).empty());
  EXPECT_TRUE(DownloadProtectionService::HasDownloadProtectionVerdict(&item));
  EXPECT_EQ(ClientDownloadResponse::SAFE,
            DownloadProtectionService::GetDownloadProtectionVerdict(&item));
  EXPECT_EQ(
      ClientDownloadResponse::TailoredVerdict::VERDICT_TYPE_UNSPECIFIED,
      DownloadProtectionService::GetDownloadProtectionTailoredVerdict(&item)
          .tailored_verdict_type());
}

TEST_F(DownloadProtectionServiceTest,
       VerifyMaybeSendDangerousDownloadOpenedReport) {
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item,
                           std::vector<std::string>(),   // empty url_chain
                           "http://www.google.com/",     // referrer
                           FILE_PATH_LITERAL("a.tmp"),   // tmp_path
                           FILE_PATH_LITERAL("a.exe"));  // final_path
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  std::string token = "token";
  ASSERT_EQ(0, sb_service_->download_report_count());

  // No report sent if download item without token field.
  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, false);
  EXPECT_EQ(0, sb_service_->download_report_count());

  // No report sent if user is in incognito mode.
  DownloadProtectionService::SetDownloadProtectionData(
      &item, token, ClientDownloadResponse::DANGEROUS,
      ClientDownloadResponse::TailoredVerdict());
  content::DownloadItemUtils::AttachInfoForTesting(
      &item, profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      nullptr);
  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, false);
  EXPECT_EQ(0, sb_service_->download_report_count());

  // No report sent if user is not in extended reporting  group.
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  SetExtendedReportingPreference(false);
  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, false);
  EXPECT_EQ(0, sb_service_->download_report_count());

  // No report sent if the download is not considered dangerous.
  SetExtendedReportingPreference(true);
  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, false);
  EXPECT_EQ(0, sb_service_->download_report_count());

  // Report successfully sent if user opted-in extended reporting, not in
  // incognito, download item has a token stored and the download is detected to
  // be dangerous and bypassed by the user.
  EXPECT_CALL(item, IsDangerous()).WillRepeatedly(Return(false));
  EXPECT_CALL(item, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED));
  auto validate_report_contents = [this, token](bool show_download_in_folder) {
    ClientSafeBrowsingReportRequest expected_report;
    expected_report.set_url(std::string());
    expected_report.set_type(
        ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_OPENED);
    expected_report.set_download_verdict(ClientDownloadResponse::DANGEROUS);
    expected_report.set_did_proceed(true);
    expected_report.set_token(token);
    expected_report.set_show_download_in_folder(show_download_in_folder);
    std::string expected_report_serialized;
    expected_report.SerializeToString(&expected_report_serialized);

    EXPECT_EQ(expected_report_serialized, sb_service_->latest_report());
  };

  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, false);
  EXPECT_EQ(1, sb_service_->download_report_count());
  validate_report_contents(/* show_download_in_folder */ false);
  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, true);
  EXPECT_EQ(2, sb_service_->download_report_count());
  validate_report_contents(/* show_download_in_folder */ true);
}

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
TEST_F(DownloadProtectionServiceTest,
       VerifyBypassReportSentImmediatelyIfVerdictDangerous) {
  enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
      profile())
      ->SetBrowserCloudPolicyClientForTesting(client_.get());

  enterprise_connectors::test::SetOnSecurityEventReporting(
      profile()->GetPrefs(), true);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item,
                           std::vector<std::string>(),   // empty url_chain
                           "http://www.google.com/",     // referrer
                           FILE_PATH_LITERAL("a.tmp"),   // tmp_path
                           FILE_PATH_LITERAL("a.exe"));  // final_path

  // This test sets the mimetype to the empty string, so pass a valid set
  // pointer, but only put the empty string in it.
  std::set<std::string> expected_mimetypes{""};
  enterprise_connectors::test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  validator.ExpectDangerousDeepScanningResult(
      "",                          // URL, not set in this test
      "",                          // Tab URL, not set in this test
      "",                          // Source, not set in this test
      "",                          // Destination, not set in this test
      final_path_.AsUTF8Unsafe(),  // Full path, including the directory
      "68617368",                  // SHA256 of the fake download
      "DANGEROUS_FILE_TYPE",       // expected_threat_type
      enterprise_connectors::
          kFileDownloadDataTransferEventTrigger,  // expected_trigger
      &expected_mimetypes,
      0,  // expected_content_size
      enterprise_connectors::EventResultToString(
          enterprise_connectors::EventResult::BYPASSED),  // expected_result
      "",                                                 // expected_username
      profile()->GetPath().AsUTF8Unsafe(),  // expected_profile_identifier
      std::nullopt                          // scan_id
  );

  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  DownloadProtectionService::SetDownloadProtectionData(
      &item, "token", ClientDownloadResponse::SAFE,
      ClientDownloadResponse::TailoredVerdict());
  SetExtendedReportingPreference(true);
  EXPECT_CALL(item, IsDangerous()).WillRepeatedly(Return(true));
  EXPECT_CALL(item, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));

  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, false);
  EXPECT_EQ(1, sb_service_->download_report_count());
  run_loop.Run();
}

TEST_F(DownloadProtectionServiceTest,
       VerifyBypassReportSentImmediatelyIfVerdictSensitive) {
  enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
      profile())
      ->SetBrowserCloudPolicyClientForTesting(client_.get());

  enterprise_connectors::test::SetOnSecurityEventReporting(
      profile()->GetPrefs(), true);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item,
                           std::vector<std::string>(),   // empty url_chain
                           "http://www.google.com/",     // referrer
                           FILE_PATH_LITERAL("a.tmp"),   // tmp_path
                           FILE_PATH_LITERAL("a.exe"));  // final_path

  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  DownloadProtectionService::SetDownloadProtectionData(
      &item, "token", ClientDownloadResponse::SAFE,
      ClientDownloadResponse::TailoredVerdict());
  SetExtendedReportingPreference(true);
  enterprise_connectors::ContentAnalysisResponse response;
  auto* result = response.add_results();
  result->add_triggered_rules()->set_action(
      enterprise_connectors::
          ContentAnalysisResponse_Result_TriggeredRule_Action_WARN);
  result->set_tag("dlp");
  enterprise_connectors::FileMetadata file_metadata(
      final_path_.AsUTF8Unsafe(), "68617368", "fake/mimetype", 1234, response);
  auto scan_result = std::make_unique<enterprise_connectors::ScanResult>(
      std::move(file_metadata));
  item.SetUserData(enterprise_connectors::ScanResult::kKey,
                   std::move(scan_result));
  EXPECT_CALL(item, IsDangerous()).WillRepeatedly(Return(true));
  EXPECT_CALL(item, GetDangerType())
      .WillRepeatedly(
          Return(download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING));

  std::set<std::string> expected_mimetypes{"fake/mimetype"};
  enterprise_connectors::test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  validator.ExpectSensitiveDataEvent(
      "",                          // URL, not set in this test
      "",                          // Tab URL, not set in this test
      "",                          // source, not used for file downloads.
      "",                          // destination, not used for file downloads.
      final_path_.AsUTF8Unsafe(),  // Full path, including the directory
      "68617368",                  // SHA256 of the fake download
      enterprise_connectors::
          kFileDownloadDataTransferEventTrigger,  // expected_trigger
      response.results()[0], &expected_mimetypes,
      1234,  // expected_content_size
      enterprise_connectors::EventResultToString(
          enterprise_connectors::EventResult::BYPASSED),  // expected_result
      "",                                                 // expected_username
      profile()->GetPath().AsUTF8Unsafe(),  // expected_profile_identifier
      {} /* expected_scan_id */, std::nullopt /* content_transfer_reason */,
      /*user_justification*/ std::nullopt);

  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, false);
  EXPECT_EQ(1, sb_service_->download_report_count());
  run_loop.Run();
}

TEST_F(DownloadProtectionServiceTest,
       VerifyBypassReportSentAfterDangerousVerdictReceived) {
  enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
      profile())
      ->SetBrowserCloudPolicyClientForTesting(client_.get());

  enterprise_connectors::test::SetOnSecurityEventReporting(
      profile()->GetPrefs(), true);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item,
                           std::vector<std::string>(),   // empty url_chain
                           "http://www.google.com/",     // referrer
                           FILE_PATH_LITERAL("a.tmp"),   // tmp_path
                           FILE_PATH_LITERAL("a.exe"));  // final_path
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  DownloadProtectionService::SetDownloadProtectionData(
      &item, "token", ClientDownloadResponse::SAFE,
      ClientDownloadResponse::TailoredVerdict());
  SetExtendedReportingPreference(true);
  EXPECT_CALL(item, IsDangerous()).WillRepeatedly(Return(false));
  EXPECT_CALL(item, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING));

  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, false);
  EXPECT_EQ(0, sb_service_->download_report_count());

  EXPECT_CALL(item, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));

  // This test sets the mimetype to the empty string, so pass a valid set
  // pointer, but only put the empty string in it.
  std::set<std::string> expected_mimetypes{""};
  enterprise_connectors::test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  validator.ExpectDangerousDeepScanningResult(
      "",                          // URL, not set in this test
      "",                          // Tab URL, not set in this test
      "",                          // Source, not set in this test
      "",                          // Destination, not set in this test
      final_path_.AsUTF8Unsafe(),  // Full path, including the directory
      "68617368",                  // SHA256 of the fake download
      "DANGEROUS_FILE_TYPE",       // expected_threat_type
      enterprise_connectors::
          kFileDownloadDataTransferEventTrigger,  // expected_trigger
      &expected_mimetypes,
      0,  // expected_content_size
      enterprise_connectors::EventResultToString(
          enterprise_connectors::EventResult::BYPASSED),  // expected_result
      "",                                                 // expected_username
      profile()->GetPath().AsUTF8Unsafe(),  // expected_profile_identifier
      std::nullopt                          // scan_id
  );

  download_service_->ReportDelayedBypassEvent(
      &item, download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE);
  run_loop.Run();
}

TEST_F(DownloadProtectionServiceTest,
       VerifyBypassReportSentAfterDlpVerdictReceived) {
  enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
      profile())
      ->SetBrowserCloudPolicyClientForTesting(client_.get());

  enterprise_connectors::test::SetOnSecurityEventReporting(
      profile()->GetPrefs(), true);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item,
                           std::vector<std::string>(),   // empty url_chain
                           "http://www.google.com/",     // referrer
                           FILE_PATH_LITERAL("a.tmp"),   // tmp_path
                           FILE_PATH_LITERAL("a.exe"));  // final_path
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  DownloadProtectionService::SetDownloadProtectionData(
      &item, "token", ClientDownloadResponse::UNCOMMON,
      ClientDownloadResponse::TailoredVerdict());
  SetExtendedReportingPreference(true);
  EXPECT_CALL(item, IsDangerous()).WillRepeatedly(Return(false));
  EXPECT_CALL(item, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING));

  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, false);
  EXPECT_EQ(0, sb_service_->download_report_count());

  enterprise_connectors::ContentAnalysisResponse response;
  response.add_results()->add_triggered_rules()->set_action(
      enterprise_connectors::
          ContentAnalysisResponse_Result_TriggeredRule_Action_WARN);
  enterprise_connectors::FileMetadata file_metadata(
      final_path_.AsUTF8Unsafe(), "68617368", "fake/mimetype", 1234, response);
  auto scan_result = std::make_unique<enterprise_connectors::ScanResult>(
      std::move(file_metadata));
  item.SetUserData(enterprise_connectors::ScanResult::kKey,
                   std::move(scan_result));
  EXPECT_CALL(item, GetDangerType())
      .WillRepeatedly(
          Return(download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING));

  std::set<std::string> expected_mimetypes{"fake/mimetype"};
  enterprise_connectors::test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  validator.ExpectSensitiveDataEvent(
      "",                          // URL, not set in this test
      "",                          // Tab URL, not set in this test
      "",                          // source, not used for file downloads.
      "",                          // destination, not used for file downloads.
      final_path_.AsUTF8Unsafe(),  // Full path, including the directory
      "68617368",                  // SHA256 of the fake download
      enterprise_connectors::
          kFileDownloadDataTransferEventTrigger,  // expected_trigger
      response.results()[0], &expected_mimetypes,
      1234,  // expected_content_size
      enterprise_connectors::EventResultToString(
          enterprise_connectors::EventResult::BYPASSED),  // expected_result
      "",                                                 // expected_username
      profile()->GetPath().AsUTF8Unsafe(),  // expected_profile_identifier
      {} /* expected_scan_id */, std::nullopt /* content_transfer_method */,
      /*user_justification*/ std::nullopt);

  download_service_->ReportDelayedBypassEvent(
      &item, download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING);
  run_loop.Run();
}

TEST_F(DownloadProtectionServiceTest,
       VerifyBypassReportSentAfterDlpBlockVerdictReceived) {
  enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
      profile())
      ->SetBrowserCloudPolicyClientForTesting(client_.get());

  enterprise_connectors::test::SetOnSecurityEventReporting(
      profile()->GetPrefs(), true);

  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item,
                           std::vector<std::string>(),   // empty url_chain
                           "http://www.google.com/",     // referrer
                           FILE_PATH_LITERAL("a.tmp"),   // tmp_path
                           FILE_PATH_LITERAL("a.exe"));  // final_path
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  DownloadProtectionService::SetDownloadProtectionData(
      &item, "token", ClientDownloadResponse::UNCOMMON,
      ClientDownloadResponse::TailoredVerdict());
  SetExtendedReportingPreference(true);
  EXPECT_CALL(item, IsDangerous()).WillRepeatedly(Return(false));
  EXPECT_CALL(item, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING));

  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, false);
  EXPECT_EQ(0, sb_service_->download_report_count());

  enterprise_connectors::ContentAnalysisResponse response;
  response.add_results()->add_triggered_rules()->set_action(
      enterprise_connectors::
          ContentAnalysisResponse_Result_TriggeredRule_Action_BLOCK);
  enterprise_connectors::FileMetadata file_metadata(
      final_path_.AsUTF8Unsafe(), "68617368", "fake/mimetype", 1234, response);
  auto scan_result = std::make_unique<enterprise_connectors::ScanResult>(
      std::move(file_metadata));
  item.SetUserData(enterprise_connectors::ScanResult::kKey,
                   std::move(scan_result));
  EXPECT_CALL(item, GetDangerType())
      .WillRepeatedly(
          Return(download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK));

  std::set<std::string> expected_mimetypes{"fake/mimetype"};
  enterprise_connectors::test::EventReportValidator validator(client_.get());
  base::RunLoop run_loop;
  validator.SetDoneClosure(run_loop.QuitClosure());
  validator.ExpectSensitiveDataEvent(
      "",                          // URL, not set in this test
      "",                          // Tab URL, not set in this test
      "",                          // source, not used for file downloads.
      "",                          // destination, not used for file downloads.
      final_path_.AsUTF8Unsafe(),  // Full path, including the directory
      "68617368",                  // SHA256 of the fake download
      enterprise_connectors::
          kFileDownloadDataTransferEventTrigger,  // expected_trigger
      response.results()[0], &expected_mimetypes,
      1234,  // expected_content_size
      enterprise_connectors::EventResultToString(
          enterprise_connectors::EventResult::BYPASSED),  // expected_result
      "",                                                 // expected_username
      profile()->GetPath().AsUTF8Unsafe(),  // expected_profile_identifier
      {} /* expected_scan_id */, std::nullopt /* content_transfer_method */,
      /*user_justification*/ std::nullopt);

  download_service_->ReportDelayedBypassEvent(
      &item, download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK);
  run_loop.Run();
}

#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
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
  EXPECT_CALL(item, IsDangerous()).WillRepeatedly(Return(true));

  TestExtensionEventObserver event_observer(test_event_router_);
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, false);
  ASSERT_EQ(1, test_event_router_->GetEventCount(
                   OnDangerousDownloadOpened::kEventName));
  const auto captured_args =
      std::move(event_observer.PassEventArgs().GetList()[0].GetDict());
  EXPECT_EQ("http://example.com/a.exe", *captured_args.FindString("url"));
  EXPECT_EQ(base::HexEncode(hash),
            *captured_args.FindString("downloadDigestSha256"));
  EXPECT_EQ(target_path.MaybeAsASCII(), *captured_args.FindString("fileName"));

  // No event is triggered if in incognito mode..
  content::DownloadItemUtils::AttachInfoForTesting(
      &item, profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      nullptr);
  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, false);
  EXPECT_EQ(1, test_event_router_->GetEventCount(
                   OnDangerousDownloadOpened::kEventName));
}

TEST_F(DownloadProtectionServiceTest,
       VerifyNoDangerousDownloadOpenedReportSentForSensitiveDataWarning) {
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

  enterprise_connectors::ContentAnalysisResponse response;
  response.add_results()->add_triggered_rules()->set_action(
      enterprise_connectors::
          ContentAnalysisResponse_Result_TriggeredRule_Action_WARN);
  enterprise_connectors::FileMetadata file_metadata(
      final_path_.AsUTF8Unsafe(), "68617368", "fake/mimetype", 1234, response);
  auto scan_result = std::make_unique<enterprise_connectors::ScanResult>(
      std::move(file_metadata));
  item.SetUserData(enterprise_connectors::ScanResult::kKey,
                   std::move(scan_result));
  EXPECT_CALL(item, GetTargetFilePath()).WillRepeatedly(ReturnRef(target_path));
  EXPECT_CALL(item, IsDangerous()).WillRepeatedly(Return(true));
  EXPECT_CALL(item, GetDangerType())
      .WillRepeatedly(
          Return(download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING));

  TestExtensionEventObserver event_observer(test_event_router_);
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  download_service_->MaybeSendDangerousDownloadOpenedReport(&item, false);

  ASSERT_EQ(0, test_event_router_->GetEventCount(
                   OnDangerousDownloadOpened::kEventName));
}
#endif

TEST_F(DownloadProtectionServiceTest, CheckClientDownloadAllowlistedByPolicy) {
  AddDomainToEnterpriseAllowlist("example.com");
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://example.com/a.exe"},  // url_chain
                           "http://www.google.com/",             // referrer
                           FILE_PATH_LITERAL("a.tmp"),           // tmp_path
                           FILE_PATH_LITERAL("a.exe"));          // final_path
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .Times(0);
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(_, _)).Times(0);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(0);

  RunLoop run_loop;
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_FALSE(HasClientDownloadRequest());
  EXPECT_TRUE(IsResult(DownloadCheckResult::ALLOWLISTED_BY_POLICY));
}

// TODO(crbug.com/397407934): Support download feedback on Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(DownloadProtectionServiceTest, CheckOffTheRecordDoesNotSendFeedback) {
  NiceMockDownloadItem item;
  EXPECT_FALSE(download_service_->MaybeBeginFeedbackForDownload(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true), &item, "",
      ""));

  EXPECT_FALSE(download_service_->MaybeBeginFeedbackForDownload(
      profile()->GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUniqueForTesting(),
          /*create_if_needed=*/true),
      &item, "", ""));
}
#endif

// ------------ class DownloadProtectionServiceFlagTest ----------------
class DownloadProtectionServiceFlagTest
    : public DownloadProtectionServiceTest,
      public testing::WithParamInterface<bool> {
 protected:
  DownloadProtectionServiceFlagTest()
      :  // Matches unsigned.exe within zipfile_one_unsigned_binary.zip
        blocklisted_hash_hex_(
            "1e954d9ce0389e2ba7447216f21761f98d1e6540c2abecdbecff570e36c493d"
            "b") {}

  void SetUp() override {
    ASSERT_TRUE(
        base::HexStringToString(blocklisted_hash_hex_, &blocklisted_hash_) &&
        blocklisted_hash_.size() == 32);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        safe_browsing::switches::kSbManualDownloadBlocklist,
        blocklisted_hash_hex_);

    DownloadProtectionServiceTest::SetUp();
  }

  // Hex 64 chars
  const std::string blocklisted_hash_hex_;
  // Binary 32 bytes
  std::string blocklisted_hash_;
};

TEST_F(DownloadProtectionServiceFlagTest, CheckClientDownloadOverridenByFlag) {
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path

  EXPECT_CALL(item, GetHash()).WillRepeatedly(ReturnRef(blocklisted_hash_));
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
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

// Zip file analysis is not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
// Test a real .zip with a real .exe in it, where the .exe is manually
// blocklisted by hash.
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
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });

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
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(DownloadProtectionServiceTest,
       VerifyReferrerChainWithEmptyNavigationHistory) {
  // Setup a web_contents with "http://example.com" as its last committed url.
  NavigateAndCommit(GURL("http://example.com"));

  NiceMockDownloadItem item;
  std::vector<GURL> url_chain = {GURL("http://example.com/referrer"),
                                 GURL("http://example.com/evil.exe")};
  EXPECT_CALL(item, GetURL()).WillRepeatedly(ReturnRef(url_chain.back()));
  EXPECT_CALL(item, GetUrlChain()).WillRepeatedly(ReturnRef(url_chain));
  content::DownloadItemUtils::AttachInfoForTesting(&item, nullptr,
                                                   web_contents());
  std::unique_ptr<ReferrerChainData> referrer_chain_data =
      IdentifyReferrerChain(
          item,
          DownloadProtectionService::GetDownloadAttributionUserGestureLimit(
              &item));
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
  SafeBrowsingNavigationObserver::MaybeCreateForWebContents(
      web_contents(), HostContentSettingsMapFactory::GetForProfile(profile()),
      SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
          profile()),
      profile()->GetPrefs(), g_browser_process->safe_browsing_service());

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

  content::DownloadItemUtils::AttachInfoForTesting(&item, nullptr,
                                                   web_contents());

  SetExtendedReportingPrefForTests(profile()->GetPrefs(), true);
  std::unique_ptr<ReferrerChainData> referrer_chain_data =
      IdentifyReferrerChain(
          item,
          DownloadProtectionService::GetDownloadAttributionUserGestureLimit(
              &item));
  // 6 entries means 5 interactions between entries.
  EXPECT_EQ(referrer_chain_data->referrer_chain_length(), 6u);

  SetExtendedReportingPrefForTests(profile()->GetPrefs(), false);
  referrer_chain_data = IdentifyReferrerChain(
      item,
      DownloadProtectionService::GetDownloadAttributionUserGestureLimit(&item));
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
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
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

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
TEST_F(DeepScanningDownloadTest, PasswordProtectedArchivesBlockedByPreference) {
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
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  TestBinaryUploadService* test_upload_service =
      static_cast<TestBinaryUploadService*>(
          CloudBinaryUploadServiceFactory::GetForProfile(profile()));
  test_upload_service->SetResponse(
      enterprise_connectors::ScanRequestUploadResult::FILE_ENCRYPTED,
      enterprise_connectors::ContentAnalysisResponse());

  {
    enterprise_connectors::test::SetAnalysisConnector(
        profile()->GetPrefs(), enterprise_connectors::FILE_DOWNLOADED, R"(
                         {
                           "service_provider": "google",
                           "enable": [
                             {"url_list": ["*"], "tags": ["malware"]}
                           ],
                           "block_until_verdict": 1,
                           "block_password_protected": true
                         })");
    PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

    EXPECT_CALL(*sb_service_->mock_database_manager(),
                MatchDownloadAllowlistUrl(_, _))
        .WillRepeatedly(
            [](const GURL& url, base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(false);
            });

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::BLOCKED_PASSWORD_PROTECTED));
    EXPECT_TRUE(HasClientDownloadRequest());
  }

  {
    enterprise_connectors::test::SetAnalysisConnector(
        profile()->GetPrefs(), enterprise_connectors::FILE_DOWNLOADED, R"(
                         {
                           "service_provider": "google",
                           "enable": [
                             {"url_list": ["*"], "tags": ["malware"]}
                           ],
                           "block_until_verdict": 1,
                           "block_password_protected": false
                         })");
    PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

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
}

TEST_F(DeepScanningDownloadTest, LargeFileBlockedByPreference) {
  constexpr int64_t kLargeSize = 51 * 1024 * 1024;
  std::string file_contents = std::string(kLargeSize, 'a');
  base::FilePath file_path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &file_path));
  file_path = temp_dir_.GetPath().AppendASCII("foo.doc");

  // Create the file.
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  file.WriteAtCurrentPos(base::as_byte_span(file_contents));

  NiceMockDownloadItem item;
  PrepareBasicDownloadItemWithFullPaths(
      &item, {"http://www.evil.com/foo.doc"},                     // url_chain
      "http://www.google.com/",                                   // referrer
      file_path,                                                  // tmp_path
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("foo.doc")));  // final_path
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });

  TestBinaryUploadService* test_upload_service =
      static_cast<TestBinaryUploadService*>(
          CloudBinaryUploadServiceFactory::GetForProfile(profile()));
  test_upload_service->SetResponse(
      enterprise_connectors::ScanRequestUploadResult::FILE_TOO_LARGE,
      enterprise_connectors::ContentAnalysisResponse());

  {
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));
    enterprise_connectors::test::SetAnalysisConnector(
        profile()->GetPrefs(), enterprise_connectors::FILE_DOWNLOADED, R"(
                         {
                           "service_provider": "google",
                           "enable": [
                             {"url_list": ["*"], "tags": ["malware"]}
                           ],
                           "block_until_verdict": 1,
                           "block_large_files": true
                         })");
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

  {
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));
    enterprise_connectors::test::SetAnalysisConnector(
        profile()->GetPrefs(), enterprise_connectors::FILE_DOWNLOADED, R"(
                         {
                           "service_provider": "google",
                           "enable": [
                             {"url_list": ["*"], "tags": ["malware"]}
                           ],
                           "block_until_verdict": 1,
                           "block_large_files": false
                         })");
    PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

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
}
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

// Android enforces a feature state before checking a File System Access Write.
#if BUILDFLAG(IS_ANDROID)
TEST_F(DownloadProtectionServiceTest, FileSystemAccessWriteRequest_NotEnabled) {
  DisableFeatures({kMaliciousApkDownloadCheck});

  auto item = PrepareBasicFileSystemAccessWriteItem(
      /*tmp_path_literal=*/FILE_PATH_LITERAL("a.txt.crswap"),
      /*final_path_literal=*/FILE_PATH_LITERAL(kEligibleFilename));

  RunLoop run_loop;
  download_service_->CheckFileSystemAccessWrite(
      std::move(item),
      base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
  EXPECT_FALSE(HasClientDownloadRequest());
}
#endif

TEST_F(DownloadProtectionServiceTest, FileSystemAccessWriteRequest_NotABinary) {
  auto item = PrepareBasicFileSystemAccessWriteItem(
      /*tmp_path_literal=*/FILE_PATH_LITERAL("a.txt.crswap"),
      /*final_path_literal=*/FILE_PATH_LITERAL("a.txt"));

  RunLoop run_loop;
  download_service_->CheckFileSystemAccessWrite(
      std::move(item),
      base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
  EXPECT_FALSE(HasClientDownloadRequest());
}

// "Light" pings for sampled files are not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(DownloadProtectionServiceTest,
       FileSystemAccessWriteRequest_SampledFile) {
  // Server response will be discarded.
  PrepareResponse(ClientDownloadResponse::DANGEROUS, net::HTTP_OK, net::OK);

  auto item = PrepareBasicFileSystemAccessWriteItem(
      /*tmp_path_literal=*/FILE_PATH_LITERAL("a.txt.crswap"),
      /*final_path_literal=*/FILE_PATH_LITERAL("a.txt"));

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
    item->browser_context =
        profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

    RunLoop run_loop;
    download_service_->CheckFileSystemAccessWrite(
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
    item = PrepareBasicFileSystemAccessWriteItem(
        /*tmp_path_literal=*/FILE_PATH_LITERAL("a.txt.crswap"),
        /*final_path_literal=*/FILE_PATH_LITERAL("a.txt"));
    RunLoop run_loop;
    download_service_->CheckFileSystemAccessWrite(
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
    EXPECT_EQ(GURL(req->url()).DeprecatedGetOriginAsURL().spec(), req->url());
    for (auto resource : req->resources()) {
      EXPECT_EQ(GURL(resource.url()).DeprecatedGetOriginAsURL().spec(),
                resource.url());
      EXPECT_EQ(GURL(resource.referrer()).DeprecatedGetOriginAsURL().spec(),
                resource.referrer());
    }
    ClearClientDownloadRequest();
  }
  {
    // Case (3): !is_extended_reporting && is_incognito.
    //           ClientDownloadRequest should NOT be sent.
    SetExtendedReportingPreference(false);
    item = PrepareBasicFileSystemAccessWriteItem(
        /*tmp_path_literal=*/FILE_PATH_LITERAL("a.txt.crswap"),
        /*final_path_literal=*/FILE_PATH_LITERAL("a.txt"));
    item->browser_context =
        profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

    RunLoop run_loop;
    download_service_->CheckFileSystemAccessWrite(
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
    item = PrepareBasicFileSystemAccessWriteItem(
        /*tmp_path_literal=*/FILE_PATH_LITERAL("a.txt.crswap"),
        /*final_path_literal=*/FILE_PATH_LITERAL("a.txt"));
    RunLoop run_loop;
    download_service_->CheckFileSystemAccessWrite(
        std::move(item),
        base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
    EXPECT_FALSE(HasClientDownloadRequest());
  }
}
#endif

TEST_F(DownloadProtectionServiceTest,
       FileSystemAccessWriteRequest_FetchFailed) {
  base::HistogramTester histogram_tester;
  // HTTP request will fail.
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_INTERNAL_SERVER_ERROR,
                  net::ERR_FAILED);

  auto item = PrepareBasicFileSystemAccessWriteItem(
      /*tmp_path_literal=*/FILE_PATH_LITERAL("a.exe.crswap"),
      /*final_path_literal=*/kEligibleFilename);

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));
  RunLoop run_loop;
  download_service_->CheckFileSystemAccessWrite(
      std::move(item),
      base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::UNKNOWN));
  histogram_tester.ExpectUniqueSample(
      /*name=*/"SBClientDownload.DownloadRequestNetworkResult",
      /*sample=*/net::ERR_FAILED,
      /*expected_bucket_count=*/1);
}

TEST_F(DownloadProtectionServiceTest, FileSystemAccessWriteRequest_Success) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  auto item = PrepareBasicFileSystemAccessWriteItem(
      /*tmp_path_literal=*/FILE_PATH_LITERAL("a.exe.crswap"),
      /*final_path_literal=*/kEligibleFilename);

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(9);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(9);
  ClientDownloadResponse expected_response;

  {
    base::HistogramTester histogram_tester;
    RunLoop run_loop;
    download_service_->CheckFileSystemAccessWrite(
        CloneFileSystemAccessWriteItem(item.get()),
        base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    EXPECT_TRUE(HasClientDownloadRequest());
    histogram_tester.ExpectUniqueSample(
        /*name=*/"SBClientDownload.DownloadRequestNetworkResult",
        /*sample=*/200,
        /*expected_bucket_count=*/1);
    ClearClientDownloadRequest();
  }
  {
    // Invalid response should result in SAFE (default value in proto).
    ClientDownloadResponse invalid_response;
    sb_service_->GetTestURLLoaderFactory(profile())->AddResponse(
        download_service_->GetDownloadRequestUrl().spec(),
        invalid_response.SerializePartialAsString());
    RunLoop run_loop;
    download_service_->CheckFileSystemAccessWrite(
        CloneFileSystemAccessWriteItem(item.get()),
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
      {ClientDownloadResponse::DANGEROUS_ACCOUNT_COMPROMISE,
       DownloadCheckResult::DANGEROUS_ACCOUNT_COMPROMISE},
  };
  for (const auto& test_case : kExpectedResults) {
    PrepareResponse(test_case.verdict, net::HTTP_OK, net::OK);
    RunLoop run_loop;
    download_service_->CheckFileSystemAccessWrite(
        CloneFileSystemAccessWriteItem(item.get()),
        base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_TRUE(IsResult(test_case.expected_result));
    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
  }
}

TEST_F(DownloadProtectionServiceTest,
       FileSystemAccessWriteRequest_WebContentsNull) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  auto item = PrepareBasicFileSystemAccessWriteItem(
      /*tmp_path_literal=*/FILE_PATH_LITERAL("a.exe.crswap"),
      /*final_path_literal=*/kEligibleFilename);
  item->web_contents = nullptr;

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(1);

  RunLoop run_loop;
  download_service_->CheckFileSystemAccessWrite(
      CloneFileSystemAccessWriteItem(item.get()),
      base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
  EXPECT_TRUE(HasClientDownloadRequest());
  ClearClientDownloadRequest();
}

TEST_F(DownloadProtectionServiceTest,
       FileSystemAccessWriteRequest_ProfileDestroyed) {
  Profile* profile1 =
      testing_profile_manager_.CreateTestingProfile("profile 1");
  auto item = PrepareBasicFileSystemAccessWriteItem(
      /*tmp_path_literal=*/FILE_PATH_LITERAL("a.exe.crswap"),
      /*final_path_literal=*/kEligibleFilename);
  item->browser_context = profile1;

  // Note 'AtMost' is used below because on Mac timing differences make the
  // mocks not reached.
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(AtMost(1));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(AtMost(1));

  RunLoop run_loop;
  download_service_->CheckFileSystemAccessWrite(
      CloneFileSystemAccessWriteItem(item.get()),
      base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  // RemovePendingDownloadRequests is called when profile is destroyed.
  download_service_->RemovePendingDownloadRequests(profile1);
  testing_profile_manager_.DeleteTestingProfile("profile 1");
}

TEST_F(DownloadProtectionServiceTest,
       FileSystemAccessWriteRequest_AllowlistedByPolicy) {
  AddDomainToEnterpriseAllowlist("example.com");

  base::RunLoop run_loop;
  auto item = PrepareBasicFileSystemAccessWriteItem(
      /*tmp_path_literal=*/FILE_PATH_LITERAL("a.txt.crswap"),
      /*final_path_literal=*/kEligibleFilename);
  item->frame_url = GURL("https://example.com/foo");
  download_service_->CheckFileSystemAccessWrite(
      std::move(item),
      base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  // Result won't be immediately available, wait for the response to
  // be posted.
  EXPECT_FALSE(has_result_);
  run_loop.Run();
  ASSERT_TRUE(IsResult(DownloadCheckResult::ALLOWLISTED_BY_POLICY));
}

TEST_F(DownloadProtectionServiceTest,
       FileSystemAccessWriteRequest_CheckRequest) {
  auto item = PrepareBasicFileSystemAccessWriteItem(
      /*tmp_path_literal=*/FILE_PATH_LITERAL("a.exe.crswap"),
      /*final_path_literal=*/kEligibleFilename);
  item->frame_url = GURL("http://www.google.com/");

  GURL tab_url("http://tab.com/final");
  GURL tab_referrer("http://tab.com/referrer");

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      tab_url, web_contents());
  navigation->SetReferrer(blink::mojom::Referrer::New(
      tab_referrer, network::mojom::ReferrerPolicy::kDefault));
  navigation->Commit();

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
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
    sb_service_->GetTestURLLoaderFactory(profile())->SetInterceptor(
        base::BindLambdaForTesting(
            [&](const network::ResourceRequest& request) {
              upload_data = network::GetUploadData(request);
              if (!upload_data.empty()) {
                interceptor_run_loop.Quit();
              }
            }));

    PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

    RunLoop run_loop;
    download_service_->CheckFileSystemAccessWrite(
        CloneFileSystemAccessWriteItem(item.get()),
        base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    interceptor_run_loop.Run();

    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();

    ClientDownloadRequest request;
    EXPECT_TRUE(request.ParseFromString(upload_data));
    EXPECT_EQ("blob:http://www.google.com/file-system-access-write",
              request.url());
    EXPECT_EQ(hash_, request.digests().sha256());
    EXPECT_EQ(item->size, request.length());
    EXPECT_EQ(item->has_user_gesture, request.user_initiated());
    EXPECT_EQ(2, request.resources_size());
    EXPECT_TRUE(RequestContainsResource(
        request, ClientDownloadRequest::DOWNLOAD_URL,
        "blob:http://www.google.com/file-system-access-write",
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

    sb_service_->GetTestURLLoaderFactory(profile())->SetInterceptor(
        network::TestURLLoaderFactory::Interceptor());

    // Simulate the request finishing.
    run_loop.Run();
  }

  // Now try with a history match.
  {
    RunLoop interceptor_run_loop;

    std::string upload_data;
    sb_service_->GetTestURLLoaderFactory(profile())->SetInterceptor(
        base::BindLambdaForTesting(
            [&](const network::ResourceRequest& request) {
              EXPECT_TRUE(request.headers
                              .GetHeader(net::HttpRequestHeaders::kContentType)
                              .has_value());
              upload_data = network::GetUploadData(request);
              if (!upload_data.empty()) {
                interceptor_run_loop.Quit();
              }
            }));

    history::RedirectList redirects;
    redirects.emplace_back("http://tab.com/ref1");
    redirects.emplace_back("http://tab.com/ref2");
    redirects.push_back(tab_url);
    HistoryServiceFactory::GetForProfile(profile(),
                                         ServiceAccessType::EXPLICIT_ACCESS)
        ->AddPage(tab_url, base::Time::Now(), 1, 0, GURL(), redirects,
                  ui::PAGE_TRANSITION_TYPED, history::SOURCE_BROWSED,
                  history::VisitResponseCodeCategory::kNot404, false);

    PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

    RunLoop run_loop;
    download_service_->CheckFileSystemAccessWrite(
        CloneFileSystemAccessWriteItem(item.get()),
        base::BindOnce(&DownloadProtectionServiceTest::CheckDoneCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    interceptor_run_loop.Run();

    EXPECT_TRUE(HasClientDownloadRequest());
    ClearClientDownloadRequest();
    ClientDownloadRequest request;
    EXPECT_TRUE(request.ParseFromString(upload_data));
    EXPECT_EQ("blob:http://www.google.com/file-system-access-write",
              request.url());
    EXPECT_EQ(hash_, request.digests().sha256());
    EXPECT_EQ(item->size, request.length());
    EXPECT_EQ(item->has_user_gesture, request.user_initiated());
    EXPECT_EQ(4, request.resources_size());
    EXPECT_TRUE(RequestContainsResource(
        request, ClientDownloadRequest::DOWNLOAD_URL,
        "blob:http://www.google.com/file-system-access-write",
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

// No access tokens on Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(EnhancedProtectionDownloadTest, AccessTokenForEnhancedProtectionUsers) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  identity_test_env_adaptor_->identity_test_env()->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSync);
  identity_test_env_adaptor_->identity_test_env()
      ->SetAutomaticIssueOfAccessTokens(/*grant=*/true);

  WebUIContentInfoSingleton::GetInstance()->AddListenerForTesting();

  {
    SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);
    NiceMockDownloadItem item;
    content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
    PrepareBasicDownloadItem(&item,
                             {"http://www.evil.com/bla.exe"},  // url_chain
                             "",                               // referrer
                             FILE_PATH_LITERAL("a.tmp"),       // tmp_path
                             FILE_PATH_LITERAL("a.exe"));      // final_path

    EXPECT_CALL(
        *sb_service_->mock_database_manager(),
        MatchDownloadAllowlistUrl(GURL("http://www.evil.com/bla.exe"), _))
        .WillRepeatedly(
            [](const GURL& url, base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(false);
            });
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
        .Times(1);
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
        .Times(1);
    sb_service_->GetTestURLLoaderFactory(profile())->SetInterceptor(
        base::BindLambdaForTesting(
            [&](const network::ResourceRequest& request) {
              EXPECT_EQ(*request.headers.GetHeader(
                            net::HttpRequestHeaders::kAuthorization),
                        "Bearer access_token");
              // Cookies should be included even when token is set.
              EXPECT_EQ(request.credentials_mode,
                        network::mojom::CredentialsMode::kInclude);
            }));

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
        WebUIContentInfoSingleton::GetInstance()
            ->client_download_requests_sent();
    ASSERT_EQ(requests.size(), 1u);
  }

  {
    SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), false);
    NiceMockDownloadItem item;
    content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
    PrepareBasicDownloadItem(&item,
                             {"http://www.evil.com/bla.exe"},  // url_chain
                             "",                               // referrer
                             FILE_PATH_LITERAL("a.tmp"),       // tmp_path
                             FILE_PATH_LITERAL("a.exe"));      // final_path

    EXPECT_CALL(
        *sb_service_->mock_database_manager(),
        MatchDownloadAllowlistUrl(GURL("http://www.evil.com/bla.exe"), _))
        .WillRepeatedly(
            [](const GURL& url, base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(false);
            });
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
        .Times(1);
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
        .Times(1);
    sb_service_->GetTestURLLoaderFactory(profile())->SetInterceptor(
        base::BindLambdaForTesting(
            [&](const network::ResourceRequest& request) {
              EXPECT_FALSE(
                  request.headers
                      .GetHeader(net::HttpRequestHeaders::kAuthorization)
                      .has_value());
              // Cookies should be attached when token is empty.
              EXPECT_EQ(request.credentials_mode,
                        network::mojom::CredentialsMode::kInclude);
            }));

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
        WebUIContentInfoSingleton::GetInstance()
            ->client_download_requests_sent();
    ASSERT_EQ(requests.size(), 2u);
  }

  WebUIContentInfoSingleton::GetInstance()->ClearListenerForTesting();
}

TEST_F(EnhancedProtectionDownloadTest, AccessTokenOnlyWhenSignedIn) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  identity_test_env_adaptor_->identity_test_env()
      ->SetAutomaticIssueOfAccessTokens(/*grant=*/true);
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  WebUIContentInfoSingleton::GetInstance()->AddListenerForTesting();

  {
    NiceMockDownloadItem item;
    content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
    PrepareBasicDownloadItem(&item,
                             {"http://www.evil.com/bla.exe"},  // url_chain
                             "",                               // referrer
                             FILE_PATH_LITERAL("a.tmp"),       // tmp_path
                             FILE_PATH_LITERAL("a.exe"));      // final_path

    EXPECT_CALL(
        *sb_service_->mock_database_manager(),
        MatchDownloadAllowlistUrl(GURL("http://www.evil.com/bla.exe"), _))
        .WillRepeatedly(
            [](const GURL& url, base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(false);
            });
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
        .Times(1);
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
        .Times(1);
    sb_service_->GetTestURLLoaderFactory(profile())->SetInterceptor(
        base::BindLambdaForTesting(
            [&](const network::ResourceRequest& request) {
              EXPECT_FALSE(
                  request.headers
                      .GetHeader(net::HttpRequestHeaders::kAuthorization)
                      .has_value());
              // Cookies should be attached when token is empty.
              EXPECT_EQ(request.credentials_mode,
                        network::mojom::CredentialsMode::kInclude);
            }));

    // Confirm that we don't try to request fetching the token
    base::MockCallback<base::OnceClosure> access_token_requested;
    EXPECT_CALL(access_token_requested, Run()).Times(0);
    identity_test_env_adaptor_->identity_test_env()
        ->SetCallbackForNextAccessTokenRequest(access_token_requested.Get());

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
        WebUIContentInfoSingleton::GetInstance()
            ->client_download_requests_sent();
    ASSERT_EQ(requests.size(), 1u);
    identity_test_env_adaptor_->identity_test_env()
        ->SetCallbackForNextAccessTokenRequest(base::NullCallback());
  }

  identity_test_env_adaptor_->identity_test_env()->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSync);

  {
    NiceMockDownloadItem item;
    content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
    PrepareBasicDownloadItem(&item,
                             {"http://www.evil.com/bla.exe"},  // url_chain
                             "",                               // referrer
                             FILE_PATH_LITERAL("a.tmp"),       // tmp_path
                             FILE_PATH_LITERAL("a.exe"));      // final_path

    EXPECT_CALL(
        *sb_service_->mock_database_manager(),
        MatchDownloadAllowlistUrl(GURL("http://www.evil.com/bla.exe"), _))
        .WillRepeatedly(
            [](const GURL& url, base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(false);
            });
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
        .Times(1);
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
        .Times(1);
    sb_service_->GetTestURLLoaderFactory(profile())->SetInterceptor(
        base::BindLambdaForTesting(
            [&](const network::ResourceRequest& request) {
              EXPECT_EQ(*request.headers.GetHeader(
                            net::HttpRequestHeaders::kAuthorization),
                        "Bearer access_token");
              // Cookies should be included even when token is set.
              EXPECT_EQ(request.credentials_mode,
                        network::mojom::CredentialsMode::kInclude);
            }));

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
        WebUIContentInfoSingleton::GetInstance()
            ->client_download_requests_sent();
    ASSERT_EQ(requests.size(), 2u);
  }

  WebUIContentInfoSingleton::GetInstance()->ClearListenerForTesting();
}
#endif

TEST_F(EnhancedProtectionDownloadTest, NoAccessTokenWhileIncognito) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);

  WebUIContentInfoSingleton::GetInstance()->AddListenerForTesting();

  sb_service_->CreateTestURLLoaderFactoryForProfile(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  {
    SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);
    NiceMockDownloadItem item;
    content::DownloadItemUtils::AttachInfoForTesting(
        &item, profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
        nullptr);
    PrepareBasicDownloadItem(&item,
                             {"http://www.evil.com/bla.exe"},  // url_chain
                             "",                               // referrer
                             FILE_PATH_LITERAL("a.tmp"),       // tmp_path
                             FILE_PATH_LITERAL("a.exe"));      // final_path

    EXPECT_CALL(
        *sb_service_->mock_database_manager(),
        MatchDownloadAllowlistUrl(GURL("http://www.evil.com/bla.exe"), _))
        .WillRepeatedly(
            [](const GURL& url, base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(false);
            });
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
        .Times(1);
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
        .Times(1);
    sb_service_->GetTestURLLoaderFactory(profile())->SetInterceptor(
        base::BindLambdaForTesting(
            [&](const network::ResourceRequest& request) {
              EXPECT_FALSE(
                  request.headers
                      .GetHeader(net::HttpRequestHeaders::kAuthorization)
                      .has_value());
              // Cookies should be attached when token is empty.
              EXPECT_EQ(request.credentials_mode,
                        network::mojom::CredentialsMode::kInclude);
            }));

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    const std::vector<std::unique_ptr<ClientDownloadRequest>>& requests =
        WebUIContentInfoSingleton::GetInstance()
            ->client_download_requests_sent();
    ASSERT_EQ(requests.size(), 1u);
  }

  WebUIContentInfoSingleton::GetInstance()->ClearListenerForTesting();
}

TEST_F(DownloadProtectionServiceTest,
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
    content::DownloadItemUtils::AttachInfoForTesting(&item1, profile1, nullptr);

    ClientDownloadResponse response;
    response.set_verdict(ClientDownloadResponse::SAFE);
    sb_service_->GetTestURLLoaderFactory(profile1)->AddResponse(
        download_service_->GetDownloadRequestUrl().spec(),
        response.SerializeAsString());

    EXPECT_CALL(*sb_service_->mock_database_manager(),
                MatchDownloadAllowlistUrl(_, _))
        .WillRepeatedly(
            [](const GURL& url, base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(false);
            });
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

    download_service_->CheckClientDownload(
        &item1,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
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
    content::DownloadItemUtils::AttachInfoForTesting(&item2, profile2, nullptr);

    ClientDownloadResponse response;
    response.set_verdict(ClientDownloadResponse::SAFE);
    sb_service_->GetTestURLLoaderFactory(profile2)->AddResponse(
        download_service_->GetDownloadRequestUrl().spec(),
        response.SerializeAsString());

    EXPECT_CALL(*sb_service_->mock_database_manager(),
                MatchDownloadAllowlistUrl(_, _))
        .WillRepeatedly(
            [](const GURL& url, base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(false);
            });
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));
    download_service_->CheckClientDownload(
        &item2,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(sb_service_->GetTestURLLoaderFactory(profile1)->NumPending(), 0);

  testing_profile_manager_.DeleteTestingProfile("profile 1");
  testing_profile_manager_.DeleteTestingProfile("profile 2");
}

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
TEST_F(DeepScanningDownloadTest, PolicyEnabled) {
  std::string file_contents = "Normal file contents";
  base::FilePath file_path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &file_path));
  file_path = temp_dir_.GetPath().AppendASCII("foo.doc");

  // Create the file.
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  file.WriteAtCurrentPos(base::as_byte_span(file_contents));

  NiceMockDownloadItem item;
  PrepareBasicDownloadItemWithFullPaths(
      &item, {"http://www.evil.com/foo.doc"},                     // url_chain
      "http://www.google.com/",                                   // referrer
      file_path,                                                  // tmp_path
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("foo.doc")));  // final_path
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

  enterprise_connectors::test::SetAnalysisConnector(
      profile()->GetPrefs(), enterprise_connectors::FILE_DOWNLOADED,
      R"({
                            "service_provider": "google",
                            "enable": [
                              {
                                "url_list": ["*"],
                                "tags": ["malware"]
                              }
                            ],
                            "block_until_verdict": 1
                          })");

  TestBinaryUploadService* test_upload_service =
      static_cast<TestBinaryUploadService*>(
          CloudBinaryUploadServiceFactory::GetForProfile(profile()));

  {
    PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);
    test_upload_service->SetResponse(
        enterprise_connectors::ScanRequestUploadResult::UPLOAD_FAILURE,
        enterprise_connectors::ContentAnalysisResponse());

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
    EXPECT_TRUE(HasClientDownloadRequest());
    EXPECT_TRUE(test_upload_service->was_called());
  }
}

TEST_F(DeepScanningDownloadTest, PolicyDisabled) {
  std::string file_contents = "Normal file contents";
  base::FilePath file_path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &file_path));
  file_path = temp_dir_.GetPath().AppendASCII("foo.doc");

  // Create the file.
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  file.WriteAtCurrentPos(base::as_byte_span(file_contents));

  NiceMockDownloadItem item;
  PrepareBasicDownloadItemWithFullPaths(
      &item, {"http://www.evil.com/foo.doc"},                     // url_chain
      "http://www.google.com/",                                   // referrer
      file_path,                                                  // tmp_path
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("foo.doc")));  // final_path
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

  enterprise_connectors::test::ClearAnalysisConnector(
      profile()->GetPrefs(), enterprise_connectors::FILE_DOWNLOADED);

  TestBinaryUploadService* test_upload_service =
      static_cast<TestBinaryUploadService*>(
          CloudBinaryUploadServiceFactory::GetForProfile(profile()));

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

TEST_F(DeepScanningDownloadTest, SafeVerdictPrecedence) {
  // These responses have precedence over safe deep scanning results.
  std::vector<std::pair<ClientDownloadResponse::Verdict, DownloadCheckResult>>
      responses = {
          {ClientDownloadResponse::DANGEROUS, DownloadCheckResult::DANGEROUS},
          {ClientDownloadResponse::DANGEROUS_HOST,
           DownloadCheckResult::DANGEROUS_HOST},
          {ClientDownloadResponse::POTENTIALLY_UNWANTED,
           DownloadCheckResult::POTENTIALLY_UNWANTED},
          {ClientDownloadResponse::UNCOMMON, DownloadCheckResult::UNCOMMON},
          {ClientDownloadResponse::DANGEROUS_ACCOUNT_COMPROMISE,
           DownloadCheckResult::DANGEROUS_ACCOUNT_COMPROMISE},
      };
  for (const auto& response : responses) {
    std::string file_contents = "Normal file contents";
    base::ScopedTempDir temporary_directory;
    ASSERT_TRUE(temporary_directory.CreateUniqueTempDir());
    base::FilePath file_path;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &file_path));
    file_path = temporary_directory.GetPath().AppendASCII("foo.doc");

    // Create the file.
    base::File file(file_path,
                    base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    file.WriteAtCurrentPos(base::as_byte_span(file_contents));

    NiceMockDownloadItem item;
    PrepareBasicDownloadItemWithFullPaths(
        &item, {"http://www.evil.com/foo.doc"},  // url_chain
        "http://www.google.com/",                // referrer
        file_path,                               // tmp_path
        temporary_directory.GetPath().Append(
            FILE_PATH_LITERAL("foo.doc")));  // final_path
    content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
    EXPECT_CALL(*sb_service_->mock_database_manager(),
                MatchDownloadAllowlistUrl(_, _))
        .WillRepeatedly(
            [](const GURL& url, base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(false);
            });
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

    enterprise_connectors::test::SetAnalysisConnector(
        profile()->GetPrefs(), enterprise_connectors::FILE_DOWNLOADED, R"(
                         {
                           "service_provider": "google",
                           "enable": [
                             {"url_list": ["*"], "tags": ["malware"]},
                             {"url_list": ["evil.com"], "tags": ["dlp"]}
                           ],
                           "block_until_verdict": 1,
                           "block_password_protected": true
                         })");

    TestBinaryUploadService* test_upload_service =
        static_cast<TestBinaryUploadService*>(
            CloudBinaryUploadServiceFactory::GetForProfile(profile()));

    PrepareResponse(response.first, net::HTTP_OK, net::OK);
    test_upload_service->SetResponse(
        enterprise_connectors::ScanRequestUploadResult::SUCCESS,
        enterprise_connectors::ContentAnalysisResponse());

    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_TRUE(IsResult(response.second));
    EXPECT_TRUE(HasClientDownloadRequest());

    bool dangerous_response =
        response.first == ClientDownloadResponse::DANGEROUS ||
        response.first == ClientDownloadResponse::DANGEROUS_HOST ||
        response.first == ClientDownloadResponse::DANGEROUS_ACCOUNT_COMPROMISE;
    EXPECT_EQ(test_upload_service->was_called(), !dangerous_response);
    test_upload_service->ClearWasCalled();
  }
}
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

// Advanced Protection deep scans are not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(DownloadProtectionServiceTest, AdvancedProtectionRequestScan) {
  PrepareResponse(ClientDownloadResponse::UNCOMMON, net::HTTP_OK, net::OK,
                  /*upload_requested=*/true, /*request_deep_scan=*/true);
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

  // Testing Scenario with request_deep_scan response is true and
  // user is enrolled in the Advanced Protection Program
  AdvancedProtectionStatusManagerFactory::GetForProfile(profile())
      ->SetAdvancedProtectionStatusForTesting(/*enrolled=*/true);
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::PROMPT_FOR_SCANNING));
  EXPECT_EQ("response_token",
            DownloadProtectionService::GetDownloadPingToken(&item));
}

TEST_F(DownloadProtectionServiceTest,
       AdvancedProtectionRequestScanWithSafeResponse) {
  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK,
                  /*upload_requested=*/true, /*request_deep_scan=*/true);
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

  // Testing Scenario with request_deep_scan response is true and
  // user is enrolled in the Advanced Protection Program, but the response
  // verdict is SAFE.
  AdvancedProtectionStatusManagerFactory::GetForProfile(profile())
      ->SetAdvancedProtectionStatusForTesting(/*enrolled=*/true);
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::PROMPT_FOR_SCANNING));
  EXPECT_EQ("response_token",
            DownloadProtectionService::GetDownloadPingToken(&item));
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(DownloadProtectionServiceTest, AdvancedProtectionRequestScanFalse) {
  PrepareResponse(ClientDownloadResponse::UNCOMMON, net::HTTP_OK, net::OK,
                  /*upload_requested=*/false,
                  /*request_deep_scan=*/false);
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

  // Testing Scenario with request_deep_scan response is true and
  // user is enrolled in the Advanced Protection Program
  AdvancedProtectionStatusManagerFactory::GetForProfile(profile())
      ->SetAdvancedProtectionStatusForTesting(/*enrolled=*/true);
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::UNCOMMON));
}

TEST_F(DownloadProtectionServiceTest, ESBRequestScanFalse) {
  PrepareResponse(ClientDownloadResponse::UNCOMMON, net::HTTP_OK, net::OK,
                  /*upload_requested=*/true, /*request_deep_scan=*/false);
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

  // Testing Scenario with request_deep_scan response is false and
  // user is enrolled in the Enhanced Protection Program.
  safe_browsing::SetEnhancedProtectionPrefForTests(profile()->GetPrefs(),
                                                   /*value*/ true);

  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::UNCOMMON));
}

TEST_F(DownloadProtectionServiceTest, ESBRequestScanFalseWhenTooLarge) {
  PrepareResponse(ClientDownloadResponse::UNCOMMON, net::HTTP_OK, net::OK,
                  /*upload_requested=*/true, /*request_deep_scan=*/true);
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path

  EXPECT_CALL(item, GetTotalBytes()).WillRepeatedly(Return(51 * 1024 * 1024));

  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

  // Testing Scenario with request_deep_scan response is true and
  // user is enrolled in the Enhanced Protection Program.
  safe_browsing::SetEnhancedProtectionPrefForTests(profile()->GetPrefs(),
                                                   /*value*/ true);

  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::UNCOMMON));
}

TEST_F(DownloadProtectionServiceTest, ESBRequestScanFalseWhenIncognito) {
  Profile* incognito_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  sb_service_->CreateTestURLLoaderFactoryForProfile(incognito_profile);
  PrepareSuccessResponseForProfile(
      incognito_profile, ClientDownloadResponse::UNCOMMON,
      /*upload_requested=*/true, /*request_deep_scan=*/true);

  NiceMockDownloadItem item;
  content::DownloadItemUtils::AttachInfoForTesting(&item, incognito_profile,
                                                   nullptr);
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

  // Testing Scenario with request_deep_scan response is true and
  // user is enrolled in the Enhanced Protection Program, but the download
  // happens in the incognito profile.
  safe_browsing::SetEnhancedProtectionPrefForTests(profile()->GetPrefs(),
                                                   /*value*/ true);

  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  // Not PROMPT_FOR_SCANNING because it is from incognito profile.
  EXPECT_TRUE(IsResult(DownloadCheckResult::UNCOMMON));
}

// Deep scanning is not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(DownloadProtectionServiceTest, ESBRequestScanPolicyEnabled) {
  PrepareResponse(ClientDownloadResponse::UNCOMMON, net::HTTP_OK, net::OK,
                  /*upload_requested=*/true, /*request_deep_scan=*/true);
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

  // Testing Scenario with request_deep_scan response is true and
  // user is enrolled in the Enhanced Protection Program.
  safe_browsing::SetEnhancedProtectionPrefForTests(profile()->GetPrefs(),
                                                   /*value*/ true);

  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingDeepScanningEnabled,
                                    true);
  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::IMMEDIATE_DEEP_SCAN));
}
#endif

TEST_F(DownloadProtectionServiceTest, ESBRequestScanPolicyDisabled) {
  PrepareResponse(ClientDownloadResponse::UNCOMMON, net::HTTP_OK, net::OK,
                  /*upload_requested=*/true, /*request_deep_scan=*/true);
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

  // Testing Scenario with request_deep_scan response is true and
  // user is enrolled in the Enhanced Protection Program.
  safe_browsing::SetEnhancedProtectionPrefForTests(profile()->GetPrefs(),
                                                   /*value*/ true);

  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingDeepScanningEnabled,
                                    false);
  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::UNCOMMON));
}

// TODO(crbug.com/397407934): Support download feedback on Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(DownloadProtectionServiceTest, DownloadFeedbackOnDangerous) {
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  SetEnhancedProtectionPrefForTests(profile()->GetPrefs(), true);

  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(4);
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
      .Times(4);

  {
    PrepareResponse(ClientDownloadResponse::DANGEROUS, net::HTTP_OK, net::OK,
                    true /* upload_requested */);
    EXPECT_CALL(*feedback_service_, BeginFeedbackForDownload(_, _, _, _));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    Mock::VerifyAndClearExpectations(feedback_service_);
  }
  {
    PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK,
                    true /* upload_requested */);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    Mock::VerifyAndClearExpectations(feedback_service_);
  }
  {
    PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK,
                    false /* upload_requested */);
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    Mock::VerifyAndClearExpectations(feedback_service_);
  }
  {
    PrepareResponse(ClientDownloadResponse::DANGEROUS, net::HTTP_OK, net::OK,
                    /*upload_requested=*/true);
    EXPECT_CALL(item, GetReceivedBytes())
        .WillRepeatedly(Return(DownloadFeedback::kMaxUploadSize + 1));
    RunLoop run_loop;
    download_service_->CheckClientDownload(
        &item,
        base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    Mock::VerifyAndClearExpectations(feedback_service_);
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
using EnterpriseCsdDownloadTest = DownloadProtectionServiceTest;

TEST_F(EnterpriseCsdDownloadTest, SkipsConsumerCsdWhenEnabled) {
  std::string file_contents = "Normal file contents";
  base::FilePath file_path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &file_path));
  file_path = temp_dir_.GetPath().AppendASCII("foo.doc");

  // Create the file.
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  file.WriteAtCurrentPos(base::as_byte_span(file_contents));

  NiceMockDownloadItem item;
  PrepareBasicDownloadItemWithFullPaths(
      &item, {"http://www.evil.com/foo.doc"},                     // url_chain
      "http://www.google.com/",                                   // referrer
      file_path,                                                  // tmp_path
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("foo.doc")));  // final_path
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  enterprise_connectors::test::SetAnalysisConnector(
      profile()->GetPrefs(), enterprise_connectors::FILE_DOWNLOADED, R"(
                         {
                           "service_provider": "google",
                           "enable": [
                             {"url_list": ["*"], "tags": ["malware"]}
                           ],
                           "block_until_verdict": 1
                         })");

  TestBinaryUploadService* test_upload_service =
      static_cast<TestBinaryUploadService*>(
          CloudBinaryUploadServiceFactory::GetForProfile(profile()));

  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);
  test_upload_service->SetResponse(
      enterprise_connectors::ScanRequestUploadResult::SUCCESS,
      enterprise_connectors::ContentAnalysisResponse());

  RunLoop run_loop;
  download_service_->MaybeCheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(HasClientDownloadRequest(), false);
  EXPECT_TRUE(test_upload_service->was_called());
}

TEST_F(EnterpriseCsdDownloadTest, PopulatesCsdFieldWhenEnabled) {
  std::string file_contents = "Normal file contents";
  base::FilePath file_path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &file_path));
  file_path = temp_dir_.GetPath().AppendASCII("foo.doc");

  // Create the file.
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  file.WriteAtCurrentPos(base::as_byte_span(file_contents));

  NiceMockDownloadItem item;
  PrepareBasicDownloadItemWithFullPaths(
      &item, {"http://www.evil.com/foo.doc"},                     // url_chain
      "http://www.google.com/",                                   // referrer
      file_path,                                                  // tmp_path
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("foo.doc")));  // final_path
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  enterprise_connectors::test::SetAnalysisConnector(
      profile()->GetPrefs(), enterprise_connectors::FILE_DOWNLOADED,
      R"({
                           "service_provider": "google",
                           "enable": [
                             {"url_list": ["*"], "tags": ["malware"]}
                           ],
                           "block_until_verdict": 1
                         })");

  TestBinaryUploadService* test_upload_service =
      static_cast<TestBinaryUploadService*>(
          CloudBinaryUploadServiceFactory::GetForProfile(profile()));

  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);
  test_upload_service->SetResponse(
      enterprise_connectors::ScanRequestUploadResult::SUCCESS,
      enterprise_connectors::ContentAnalysisResponse());

  RunLoop run_loop;
  download_service_->MaybeCheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(test_upload_service->was_called());
  EXPECT_EQ(test_upload_service->last_request().request_data().has_csd(), true);
}

TEST_F(EnterpriseCsdDownloadTest, StillDoesMetadataCheckForLargeFile) {
  std::string file_contents = "Normal file contents";
  base::FilePath file_path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &file_path));
  file_path = temp_dir_.GetPath().AppendASCII("foo.doc");

  // Create the file.
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  file.WriteAtCurrentPos(base::as_byte_span(file_contents));

  NiceMockDownloadItem item;
  PrepareBasicDownloadItemWithFullPaths(
      &item, {"http://www.evil.com/foo.doc"},                     // url_chain
      "http://www.google.com/",                                   // referrer
      file_path,                                                  // tmp_path
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("foo.doc")));  // final_path
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _));
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

  enterprise_connectors::test::SetAnalysisConnector(
      profile()->GetPrefs(), enterprise_connectors::FILE_DOWNLOADED,
      R"({
                           "service_provider": "google",
                           "enable": [
                             {"url_list": ["*"], "tags": ["malware"]}
                           ],
                           "block_until_verdict": 1
                         })");

  TestBinaryUploadService* test_upload_service =
      static_cast<TestBinaryUploadService*>(
          CloudBinaryUploadServiceFactory::GetForProfile(profile()));

  PrepareResponse(ClientDownloadResponse::SAFE, net::HTTP_OK, net::OK);
  test_upload_service->SetResponse(
      enterprise_connectors::ScanRequestUploadResult::FILE_TOO_LARGE,
      enterprise_connectors::ContentAnalysisResponse());

  RunLoop run_loop;
  download_service_->MaybeCheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(test_upload_service->was_called());
  EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
}
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

// Deep scans, Advanced Protection, and encrypted archives are not supported on
// Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(DownloadProtectionServiceTest, ESBRequestScan) {
  PrepareResponse(ClientDownloadResponse::UNCOMMON, net::HTTP_OK, net::OK,
                  /*upload_requested=*/true, /*request_deep_scan=*/true);
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

  // Testing Scenario with request_deep_scan response is true and
  // user is enrolled in the Enhanced Protection Program.
  safe_browsing::SetEnhancedProtectionPrefForTests(profile()->GetPrefs(),
                                                   /*value*/ true);

  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::IMMEDIATE_DEEP_SCAN));
}

TEST_F(DownloadProtectionServiceTest, APPRequestScan) {
  PrepareResponse(ClientDownloadResponse::UNCOMMON, net::HTTP_OK, net::OK,
                  /*upload_requested=*/true, /*request_deep_scan=*/true);
  NiceMockDownloadItem item;
  PrepareBasicDownloadItem(&item, {"http://www.evil.com/a.exe"},  // url_chain
                           "http://www.google.com/",              // referrer
                           FILE_PATH_LITERAL("a.tmp"),            // tmp_path
                           FILE_PATH_LITERAL("a.exe"));           // final_path
  EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
      .Times(1);
  EXPECT_CALL(*sb_service_->mock_database_manager(),
              MatchDownloadAllowlistUrl(_, _))
      .WillRepeatedly(
          [](const GURL& url, base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          });
  EXPECT_CALL(*binary_feature_extractor_.get(),
              ExtractImageFeatures(
                  tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _));

  // Testing Scenario with request_deep_scan response is true and
  // user is enrolled in the the Advanced Protection Program
  AdvancedProtectionStatusManagerFactory::GetForProfile(profile())
      ->SetAdvancedProtectionStatusForTesting(/*enrolled=*/true);

  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  // Advanced Protection users do not immediately deep scan.
  EXPECT_TRUE(IsResult(DownloadCheckResult::PROMPT_FOR_SCANNING));
}

TEST_F(DownloadProtectionServiceTest, EncryptedArchive) {
  PrepareResponse(ClientDownloadResponse::UNCOMMON, net::HTTP_OK, net::OK,
                  /*upload_requested=*/true, /*request_deep_scan=*/true);

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
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  // Testing Scenario with request_deep_scan response is true and
  // user is enrolled in the Enhanced Protection Program.
  safe_browsing::SetEnhancedProtectionPrefForTests(profile()->GetPrefs(),
                                                   /*value*/ true);

  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  RunLoop run_loop;
  download_service_->CheckClientDownload(
      &item,
      base::BindRepeating(&DownloadProtectionServiceTest::CheckDoneCallback,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  // Downloads of encrypted archives cannot immediately deep scan
  EXPECT_TRUE(IsResult(DownloadCheckResult::PROMPT_FOR_SCANNING));
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
struct UrlOverrideTestCase {
  // If null, no override is specified.
  const char* override_string = nullptr;
  // Specified only if different from `override_string`.
  const char* expected_url = nullptr;
};

// Tests for Android download protection logic, parameterized by whether Android
// download protection is in telemetry-only mode, and the Safe Browsing state,
// and the service override URL to use.
// These tests use the real RemoteDatabaseManager rather than a mock.
class AndroidDownloadProtectionTest
    : public DownloadProtectionServiceTestBase</*ShouldSetDbManager=*/false>,
      public testing::WithParamInterface<
          std::tuple<bool, SafeBrowsingState, UrlOverrideTestCase>> {
 public:
  using Outcome =
      DownloadProtectionMetricsData::AndroidDownloadProtectionOutcome;

  AndroidDownloadProtectionTest() {
    base::FieldTrialParams params = {
        {std::string(kMaliciousApkDownloadCheckTelemetryOnly.name),
         IsTelemetryOnlyMode() ? "true" : "false"}};
    if (GetUrlOverrideString()) {
      params.insert(
          {std::string(kMaliciousApkDownloadCheckServiceUrlOverride.name),
           std::string(GetUrlOverrideString())});
    }
    feature_list_.InitAndEnableFeatureWithParameters(kMaliciousApkDownloadCheck,
                                                     params);
  }

  bool IsTelemetryOnlyMode() const { return std::get<0>(GetParam()); }

  SafeBrowsingState GetSafeBrowsingState() const { return get<1>(GetParam()); }

  const char* GetUrlOverrideString() const {
    return get<2>(GetParam()).override_string;
  }

  // Calling this function requires that the test has an override URL.
  GURL GetExpectedDownloadRequestUrl() const {
    CHECK(GetUrlOverrideString());
    if (const char* expected = get<2>(GetParam()).expected_url;
        expected != nullptr) {
      return GURL{expected};
    }
    return GURL{GetUrlOverrideString()};
  }

  void SetUp() override {
    DownloadProtectionServiceTestBase<false>::SetUp();
    SetSafeBrowsingState(profile()->GetPrefs(), GetSafeBrowsingState());
    // Stop and restart the RemoteDatabaseManager with a test URLLoaderFactory.
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            sb_service_->GetTestURLLoaderFactory(profile()));
    sb_service_->database_manager()->StopOnUIThread(/*shutdown=*/false);
    sb_service_->database_manager()->StartOnUIThread(
        test_shared_loader_factory_, GetTestV4ProtocolConfig());
  }

  bool ShouldAndroidDownloadProtectionBeActive() const {
    switch (GetSafeBrowsingState()) {
      case SafeBrowsingState::NO_SAFE_BROWSING:
        return false;
      case SafeBrowsingState::STANDARD_PROTECTION:
        return !IsTelemetryOnlyMode();
      case SafeBrowsingState::ENHANCED_PROTECTION:
        return true;
    }
  }

  void ResetHistogramTester() {
    histograms_ = std::make_unique<base::HistogramTester>();
  }

  void ExpectHistogramUniqueSample(Outcome outcome) {
    histograms_->ExpectUniqueSample(kAndroidDownloadProtectionOutcomeHistogram,
                                    outcome, 1);
  }

  void OverrideNextShouldSample(bool should_sample) {
    static_cast<DownloadProtectionDelegateAndroid*>(
        download_service_->delegate())
        ->SetNextShouldSampleForTesting(should_sample);
  }

  // Expects MaybeCheckClientDownload to return false, and therefore no ping to
  // be sent.
  void ExpectNoCheckClientDownload(download::DownloadItem* item) {
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(_, _))
        .Times(0);
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(_, _, _, _))
        .Times(0);

    EXPECT_FALSE(
        download_service_->MaybeCheckClientDownload(item, base::DoNothing()));
    EXPECT_FALSE(HasClientDownloadRequest());

    Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
  }

  // Expects MaybeCheckClientDownload to return true, and to send a ping over
  // the network.
  void ExpectCheckClientDownload(
      download::DownloadItem* item,
      base::optional_ref<GURL> download_request_url = std::nullopt) {
    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(tmp_path_, _))
        .Times(1);
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(
                    tmp_path_, BinaryFeatureExtractor::kDefaultOptions, _, _))
        .Times(1);
    sb_service_->GetTestURLLoaderFactory(profile())->SetInterceptor(
        base::BindLambdaForTesting(
            [&](const network::ResourceRequest& request) {
              EXPECT_EQ(*request.headers.GetHeader(
                            net::HttpRequestHeaders::kContentType),
                        "application/x-protobuf");
              EXPECT_TRUE(
                  request.headers.GetHeader("X-Goog-Api-Key").has_value());
              EXPECT_EQ(
                  request.url,
                  // Check for the correct service URL, if provided.
                  download_request_url
                      ? *download_request_url
                      : download_service_->delegate()->GetDownloadRequestUrl());
            }));

    RunLoop run_loop;
    EXPECT_TRUE(download_service_->MaybeCheckClientDownload(
        item,
        base::BindRepeating(&AndroidDownloadProtectionTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure())));
    run_loop.Run();

    EXPECT_TRUE(HasClientDownloadRequest());

    Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
  }

  // Common test code for CheckClientDownload().
  void TestCheckClientDownload(
      base::optional_ref<GURL> download_request_url = std::nullopt,
      base::OnceCallback<void(download::DownloadItem*)> verify_item =
          base::DoNothing()) {
    // Response to any requests will be DANGEROUS.
    PrepareResponse(ClientDownloadResponse::DANGEROUS, net::HTTP_OK, net::OK);
    // Override the random sampling.
    OverrideNextShouldSample(true);

    ResetHistogramTester();
    {
      NiceMockDownloadItem item;
      content::DownloadItemUtils::AttachInfoForTesting(&item, profile(),
                                                       nullptr);
      PrepareBasicDownloadItemWithContentUri(
          &item, /*url_chain_items=*/{"http://www.evil.com/bla.apk"},
          /*referrer_url=*/"",
          /*display_name=*/base::FilePath(kApkFilename));

      std::move(verify_item).Run(&item);

      if (ShouldAndroidDownloadProtectionBeActive()) {
        ExpectCheckClientDownload(&item, download_request_url);
      } else {
        ExpectNoCheckClientDownload(&item);
      }
    }
    // The histogram is logged when the item goes out of scope.

    if (ShouldAndroidDownloadProtectionBeActive()) {
      EXPECT_TRUE(IsResult(IsTelemetryOnlyMode()
                               ? DownloadCheckResult::UNKNOWN
                               : DownloadCheckResult::DANGEROUS));
      EXPECT_TRUE(GetClientDownloadRequest()->has_download_type());
      EXPECT_EQ(GetClientDownloadRequest()->download_type(),
                ClientDownloadRequest::ANDROID_APK);
      EXPECT_EQ(GetClientDownloadRequest()->file_basename(), "a.apk");
      ExpectHistogramUniqueSample(Outcome::kClientDownloadRequestSent);
    } else {
      ExpectHistogramUniqueSample(Outcome::kDownloadProtectionDisabled);
    }
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<base::HistogramTester> histograms_;
};

const SafeBrowsingState kSafeBrowsingStates[] = {
    SafeBrowsingState::NO_SAFE_BROWSING,
    SafeBrowsingState::STANDARD_PROTECTION,
    SafeBrowsingState::ENHANCED_PROTECTION,
};

// The parameter indicates: whether Android download protection is in
// telemetry-only mode; the Safe Browsing state of the profile; the service URL
// override.
INSTANTIATE_TEST_SUITE_P(
    /* No label */,
    AndroidDownloadProtectionTest,
    testing::Combine(testing::Bool(),
                     testing::ValuesIn(kSafeBrowsingStates),
                     testing::Values(UrlOverrideTestCase{})));

const char kDefaultAndroidDownloadRequestUrl[] =
    "https://androidchromeprotect.pa.googleapis.com/v1/download";

using AndroidDownloadProtectionTestWithOverrideUrl =
    AndroidDownloadProtectionTest;
INSTANTIATE_TEST_SUITE_P(
    WithOverrideUrl,
    AndroidDownloadProtectionTestWithOverrideUrl,
    testing::Combine(testing::Bool(),
                     testing::ValuesIn(kSafeBrowsingStates),
                     testing::ValuesIn<UrlOverrideTestCase>({
                         // Valid URL overrides.
                         {"https://foo.googleapis.com/path"},
                         {"https://foo.google.com/path"},
                         {"https://google.com/path"},
                         {kDefaultAndroidDownloadRequestUrl},
                         // Invalid cases should fall back to the default URL.
                         {"", kDefaultAndroidDownloadRequestUrl},
                         {"bogus.com", kDefaultAndroidDownloadRequestUrl},
                         // Not a Google associated domain.
                         {"https://foo.notgoogle.com/path",
                          kDefaultAndroidDownloadRequestUrl},
                         // Not HTTPS.
                         {"http://foo.googleapis.com/path",
                          kDefaultAndroidDownloadRequestUrl},
                         {"file:///foo/bar", kDefaultAndroidDownloadRequestUrl},
                     })));

// On Android, the RemoteDatabaseManager doesn't check any blocklist for
// downloads.
TEST_P(AndroidDownloadProtectionTest, CheckDownloadUrl) {
  NiceMockDownloadItem item;
  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
  PrepareBasicDownloadItemWithContentUri(
      &item, /*url_chain_items=*/
      {"https://www.example.test/", "http://www.evil.com/bla.apk"},
      /*referrer_url=*/"https://www.google.com",
      /*display_name=*/base::FilePath(kApkFilename));

  content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);

  EXPECT_EQ(download_service_->delegate()->ShouldCheckDownloadUrl(&item),
            ShouldAndroidDownloadProtectionBeActive());

  // On Android, CheckDownloadURL always returns immediately which means the
  // client object callback will never be called. Nevertheless the callback
  // provided to CheckClientDownload must still be called.
  // Note: This calls CheckDownloadUrl() regardless of the value of
  // ShouldCheckDownloadUrl(). Normally the caller is responsible for checking
  // ShouldCheckDownloadUrl(). CheckDownloadUrl() itself should behave the same
  // in either case.
  RunLoop run_loop;
  download_service_->CheckDownloadUrl(
      &item, base::BindOnce(&AndroidDownloadProtectionTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(IsResult(DownloadCheckResult::SAFE));
}

// Tests that CheckClientDownload is called if Android download protection is
// active.
TEST_P(AndroidDownloadProtectionTest, CheckClientDownload) {
  TestCheckClientDownload();
}

TEST_P(AndroidDownloadProtectionTestWithOverrideUrl, CheckClientDownload) {
  SCOPED_TRACE(GetUrlOverrideString());

  // Test that the correct service override URL is used.
  GURL expected_url = GetExpectedDownloadRequestUrl();
  EXPECT_EQ(download_service_->delegate()->GetDownloadRequestUrl(),
            expected_url);

  TestCheckClientDownload(expected_url);
}

TEST_P(AndroidDownloadProtectionTest,
       NoCheckClientDownloadForNonSupportedType) {
  if (!ShouldAndroidDownloadProtectionBeActive()) {
    return;
  }

  // Response to any requests will be DANGEROUS.
  PrepareResponse(ClientDownloadResponse::DANGEROUS, net::HTTP_OK, net::OK);
  // Override the random sampling.
  OverrideNextShouldSample(true);

  ResetHistogramTester();
  {
    NiceMockDownloadItem item;
    content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
    PrepareBasicDownloadItemWithContentUri(
        &item, /*url_chain_items=*/{"http://www.evil.com/bla.apk"},
        /*referrer_url=*/"",
        /*display_name=*/
        base::FilePath(FILE_PATH_LITERAL("not_supported_filetype.dex")));

    EXPECT_EQ(
        download_service_->delegate()->IsSupportedDownload(item, display_name_),
        MayCheckDownloadResult::kMaySendSampledPingOnly);

    EXPECT_CALL(*binary_feature_extractor_.get(), CheckSignature(_, _))
        .Times(0);
    EXPECT_CALL(*binary_feature_extractor_.get(),
                ExtractImageFeatures(_, _, _, _))
        .Times(0);

    // This returns true because there may be a ping sent, as far as
    // DownloadProtectionService knows at this point...
    RunLoop run_loop;
    EXPECT_TRUE(download_service_->MaybeCheckClientDownload(
        &item,
        base::BindRepeating(&AndroidDownloadProtectionTest::CheckDoneCallback,
                            base::Unretained(this), run_loop.QuitClosure())));
    run_loop.Run();

    // ... except that we are guaranteed not to send a ping because the sample
    // rate for unsupported file types is 0.
    EXPECT_EQ(download_service_->delegate()->GetUnsupportedFileSampleRate(
                  display_name_),
              0.0);
    EXPECT_FALSE(HasClientDownloadRequest());

    Mock::VerifyAndClearExpectations(binary_feature_extractor_.get());
  }
  // The histogram is logged when the item goes out of scope.
  ExpectHistogramUniqueSample(Outcome::kDownloadNotSupportedType);
}

TEST_P(AndroidDownloadProtectionTest, NoCheckClientDownloadNotSampled) {
  if (!ShouldAndroidDownloadProtectionBeActive()) {
    return;
  }

  // Response to any requests will be DANGEROUS.
  PrepareResponse(ClientDownloadResponse::DANGEROUS, net::HTTP_OK, net::OK);
  // Override the random sampling to guarantee we won't sample.
  OverrideNextShouldSample(false);

  ResetHistogramTester();
  {
    NiceMockDownloadItem item;
    content::DownloadItemUtils::AttachInfoForTesting(&item, profile(), nullptr);
    PrepareBasicDownloadItemWithContentUri(
        &item, /*url_chain_items=*/{"http://www.evil.com/bla.apk"},
        /*referrer_url=*/"",
        /*display_name=*/
        base::FilePath(kApkFilename));

    // The sampling from the delegate results in MaybeCheckClientDownload
    // returning false.
    ExpectNoCheckClientDownload(&item);
  }
  // The histogram is logged when the item goes out of scope.
  ExpectHistogramUniqueSample(Outcome::kNotSampled);
}

// Tests the various false outcomes of IsSupportedDownload().
TEST_P(AndroidDownloadProtectionTest, IsSupportedDownloadFalse) {
  ResetHistogramTester();
  {
    NiceMockDownloadItem item_empty_url_chain;
    content::DownloadItemUtils::AttachInfoForTesting(&item_empty_url_chain,
                                                     profile(), nullptr);
    PrepareBasicDownloadItemWithContentUri(
        &item_empty_url_chain, /*url_chain_items=*/{},
        /*referrer_url=*/"",
        /*display_name=*/base::FilePath(kApkFilename));
    std::vector<GURL> empty_url_chain;
    EXPECT_CALL(item_empty_url_chain, GetUrlChain())
        .WillRepeatedly(ReturnRef(empty_url_chain));

    EXPECT_EQ(download_service_->delegate()->IsSupportedDownload(
                  item_empty_url_chain, final_path_),
              MayCheckDownloadResult::kMayNotCheckDownload);
  }
  ExpectHistogramUniqueSample(Outcome::kEmptyUrlChain);

  ResetHistogramTester();
  {
    NiceMockDownloadItem item_invalid_url;
    content::DownloadItemUtils::AttachInfoForTesting(&item_invalid_url,
                                                     profile(), nullptr);
    PrepareBasicDownloadItemWithContentUri(
        &item_invalid_url, /*url_chain_items=*/{"bogus_url"},
        /*referrer_url=*/"",
        /*display_name=*/base::FilePath(kApkFilename));

    EXPECT_EQ(download_service_->delegate()->IsSupportedDownload(
                  item_invalid_url, final_path_),
              MayCheckDownloadResult::kMayNotCheckDownload);
  }
  ExpectHistogramUniqueSample(Outcome::kInvalidUrl);

  ResetHistogramTester();
  {
    NiceMockDownloadItem item_unsupported_url_scheme;
    content::DownloadItemUtils::AttachInfoForTesting(
        &item_unsupported_url_scheme, profile(), nullptr);
    PrepareBasicDownloadItemWithContentUri(
        &item_unsupported_url_scheme,
        /*url_chain_items=*/{"unsupported://blah"},
        /*referrer_url=*/"",
        /*display_name=*/base::FilePath(kApkFilename));

    EXPECT_EQ(download_service_->delegate()->IsSupportedDownload(
                  item_unsupported_url_scheme, final_path_),
              MayCheckDownloadResult::kMayNotCheckDownload);
  }
  ExpectHistogramUniqueSample(Outcome::kUnsupportedUrlScheme);

  ResetHistogramTester();
  {
    NiceMockDownloadItem item_remote_file_url;
    content::DownloadItemUtils::AttachInfoForTesting(&item_remote_file_url,
                                                     profile(), nullptr);
    PrepareBasicDownloadItemWithContentUri(
        &item_remote_file_url,
        /*url_chain_items=*/{"file://drive.test/download"},
        /*referrer_url=*/"",
        /*display_name=*/base::FilePath(kApkFilename));

    EXPECT_EQ(download_service_->delegate()->IsSupportedDownload(
                  item_remote_file_url, final_path_),
              MayCheckDownloadResult::kMayNotCheckDownload);
  }
  ExpectHistogramUniqueSample(Outcome::kRemoteFile);

  ResetHistogramTester();
  {
    NiceMockDownloadItem item_local_file_url;
    content::DownloadItemUtils::AttachInfoForTesting(&item_local_file_url,
                                                     profile(), nullptr);
    PrepareBasicDownloadItemWithContentUri(
        &item_local_file_url, /*url_chain_items=*/{"file:///download"},
        /*referrer_url=*/"",
        /*display_name=*/base::FilePath(kApkFilename));

    EXPECT_EQ(download_service_->delegate()->IsSupportedDownload(
                  item_local_file_url, final_path_),
              MayCheckDownloadResult::kMayNotCheckDownload);
  }
  ExpectHistogramUniqueSample(Outcome::kLocalFile);

  ResetHistogramTester();
  {
    NiceMockDownloadItem item_display_name_not_apk;
    content::DownloadItemUtils::AttachInfoForTesting(&item_display_name_not_apk,
                                                     profile(), nullptr);
    PrepareBasicDownloadItemWithContentUri(
        &item_display_name_not_apk,
        /*url_chain_items=*/{"https://evil.com/bla.apk"},
        /*referrer_url=*/"",
        /*display_name=*/base::FilePath(FILE_PATH_LITERAL("a.dex")));

    EXPECT_EQ(download_service_->delegate()->IsSupportedDownload(
                  item_display_name_not_apk, final_path_),
              MayCheckDownloadResult::kMaySendSampledPingOnly);
  }
  ExpectHistogramUniqueSample(Outcome::kDownloadNotSupportedType);
}

// Tests that CheckClientDownload is called even if the download is not a
// FULL_PING file type according to FileTypePolicies, but is otherwise supported
// for Android download protection. This essentially tests that Android download
// protection bypasses FileTypePolicies and applies its own hardcoded logic to
// always ping only for APK files.
// TODO(chlily): Refactor/fix FileTypePolicies to consolidate platform-specific
// logic, then use FileTypePolicies on Android instead of bypassing it.
TEST_P(AndroidDownloadProtectionTest,
       CheckClientDownloadIgnoresFileTypePolicies) {
  FileTypePoliciesTestOverlay file_type_policies;
  auto no_ping_config = std::make_unique<DownloadFileTypeConfig>();
  no_ping_config->mutable_default_file_type()->set_ping_setting(
      DownloadFileType::NO_PING);
  file_type_policies.SwapConfig(no_ping_config);

  TestCheckClientDownload(
      /*download_request_url=*/std::nullopt,
      /*verify_item=*/base::BindLambdaForTesting(
          [](download::DownloadItem* item) {
            // Check that setup worked properly.
            EXPECT_FALSE(FileTypePolicies::GetInstance()->IsCheckedBinaryFile(
                item->GetFileNameToReportUser()));
          }));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace safe_browsing
