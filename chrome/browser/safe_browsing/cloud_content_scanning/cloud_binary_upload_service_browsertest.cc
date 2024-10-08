// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service.h"

#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/test/management_context_mixin.h"
#include "chrome/browser/enterprise/connectors/test/test_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/test/browser_test.h"

namespace safe_browsing {
namespace {

constexpr char kData[] = "data";
constexpr char kTestUrl[] = "https://example.com";
constexpr char kTestAccessToken[] = "test_access_token";
constexpr char kTestInstanceId[] = "test_instance_id";

struct ManagementContextDeviceRequest {
  enterprise_connectors::test::ManagementContext context;
  bool profile_request;
};

constexpr ManagementContextDeviceRequest kTestCases[] = {
    // Cases where only the device is managed.
    {{.is_cloud_user_managed = false, .is_cloud_machine_managed = true}, false},

    // Cases where only the user is managed.
    {{.is_cloud_user_managed = true, .is_cloud_machine_managed = false}, true},

    // Cases where both the user and device are managed
    {{.is_cloud_user_managed = true,
      .is_cloud_machine_managed = true,
      .affiliated = true},
     true},
    {{.is_cloud_user_managed = true,
      .is_cloud_machine_managed = true,
      .affiliated = false},
     true},
    {{.is_cloud_user_managed = true,
      .is_cloud_machine_managed = true,
      .affiliated = true},
     false},
    {{.is_cloud_user_managed = true,
      .is_cloud_machine_managed = true,
      .affiliated = false},
     false},
};

class TestSafeBrowsingTokenFetcher : public SafeBrowsingTokenFetcher {
 public:
  void Start(Callback callback) override {
    std::move(callback).Run(kTestAccessToken);
  }

  void OnInvalidAccessToken(const std::string& invalid_access_token) override {
    NOTREACHED_IN_MIGRATION();
  }
};

class TestBinaryFCMService : public BinaryFCMService {
 public:
  explicit TestBinaryFCMService(bool connected) : connected_(connected) {}

  void GetInstanceID(GetInstanceIDCallback callback) override {
    if (connected_) {
      std::move(callback).Run(kTestInstanceId);
    } else {
      std::move(callback).Run(BinaryFCMService::kInvalidId);
    }
  }

  void UnregisterInstanceID(const std::string& token,
                            UnregisterInstanceIDCallback callback) override {
    // Always successfully unregister.
    std::move(callback).Run(true);
  }

  bool Connected() override { return connected_; }

 private:
  bool connected_ = false;
};

class TestCloudBinaryUploadService : public CloudBinaryUploadService {
 public:
  TestCloudBinaryUploadService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile* profile,
      std::unique_ptr<BinaryFCMService> binary_fcm_service,
      enterprise_connectors::test::ManagementContext management_context,
      enterprise_connectors::AnalysisConnector connector,
      bool profile_request)
      : CloudBinaryUploadService(url_loader_factory,
                                 profile,
                                 std::move(binary_fcm_service)),
        management_context_(management_context),
        connector_(connector),
        profile_request_(profile_request) {
    SetTokenFetcherForTesting(std::make_unique<TestSafeBrowsingTokenFetcher>());
  }

  void OnGetRequestData(Request::Id request_id,
                        Result result,
                        Request::Data data) override {
    auto* request = GetRequest(request_id);

    ASSERT_TRUE(request);
    ASSERT_EQ(result, Result::SUCCESS);

    // The `data` obtained here should be identical to the one mocked in
    // `TestRequest`.
    ASSERT_EQ(data.contents, kData);
    ASSERT_EQ(data.contents.size(), data.size);
    ASSERT_EQ(request->per_profile_request(), profile_request_);
    // Since paste requests only receive sync responses, they don't use FCM and
    // shouldn't have a notification token.
    if (connector_ ==
        enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY) {
      ASSERT_TRUE(request->fcm_notification_token().empty());
    } else {
      ASSERT_EQ(request->fcm_notification_token(), kTestInstanceId);
    }

    // There is no case where neither user nor machine is managed.
    // We upload an access token when we have a:
    // 1. Profile policy on an unmanaged device,
    // 2. Profile policy on an unaffiliated device (no device-level BCE
    // policies),
    // 3. Affiliated profile.
    if (!management_context_.is_cloud_machine_managed ||
        management_context_.affiliated || request->per_profile_request()) {
      ASSERT_EQ(request->access_token(), kTestAccessToken);
    } else {
      ASSERT_TRUE(request->access_token().empty());
    }

    FinishRequest(GetRequest(request_id), BinaryUploadService::Result::SUCCESS,
                  enterprise_connectors::ContentAnalysisResponse());
  }

 private:
  enterprise_connectors::test::ManagementContext management_context_;
  enterprise_connectors::AnalysisConnector connector_;
  bool profile_request_;
};

class TestRequest : public CloudBinaryUploadService::Request {
 public:
  TestRequest(BinaryUploadService::ContentAnalysisCallback callback,
              enterprise_connectors::CloudOrLocalAnalysisSettings settings)
      : CloudBinaryUploadService::Request(std::move(callback),
                                          std::move(settings)) {}

  void GetRequestData(DataCallback callback) override {
    CloudBinaryUploadService::Request::Data data;
    data.contents = kData;
    data.size = data.contents.size();

    std::move(callback).Run(BinaryUploadService::Result::SUCCESS,
                            std::move(data));
  }
};

class CloudBinaryUploadServiceRequestValidationBrowserTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<ManagementContextDeviceRequest> {
 public:
  CloudBinaryUploadServiceRequestValidationBrowserTest()
      : management_mixin_(
            enterprise_connectors::test::ManagementContextMixin::Create(
                &mixin_host_,
                this,
                management_context())) {}

  enterprise_connectors::test::ManagementContext management_context() const {
    return GetParam().context;
  }

  bool profile_request() const { return GetParam().profile_request; }

  void SetUpOnMainThread() override {
    CloudBinaryUploadServiceFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(
            &CloudBinaryUploadServiceRequestValidationBrowserTest::
                CreateCloudBinaryUploadService,
            base::Unretained(this)));
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<KeyedService> CreateCloudBinaryUploadService(
      content::BrowserContext* browser_context) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    return std::make_unique<safe_browsing::TestCloudBinaryUploadService>(
        g_browser_process->safe_browsing_service()->GetURLLoaderFactory(
            profile),
        profile, std::make_unique<TestBinaryFCMService>(valid_fcm_),
        management_context(), connector_, profile_request());
  }

  CloudBinaryUploadService* service() {
    return static_cast<safe_browsing::CloudBinaryUploadService*>(
        CloudBinaryUploadServiceFactory::GetForProfile(browser()->profile()));
  }

  std::string dm_token() {
    if (profile_request()) {
      return enterprise_connectors::test::kProfileDmToken;
    } else {
      return enterprise_connectors::test::kDeviceDmToken;
    }
  }

  void set_connector(enterprise_connectors::AnalysisConnector connector) {
    connector_ = connector;
  }

  void set_valid_fcm(bool valid_fcm) { valid_fcm_ = valid_fcm; }

 protected:
  bool valid_fcm_ = true;
  enterprise_connectors::AnalysisConnector connector_ =
      enterprise_connectors::AnalysisConnector::ANALYSIS_CONNECTOR_UNSPECIFIED;
  std::unique_ptr<enterprise_connectors::test::ManagementContextMixin>
      management_mixin_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_P(CloudBinaryUploadServiceRequestValidationBrowserTest,
                       Paste) {
  set_connector(enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY);
  // Having an invalid FCM connection shouldn't prevent paste requests from
  // working since they don't use FCM.
  set_valid_fcm(false);

  enterprise_connectors::CloudAnalysisSettings cloud_settings;
  cloud_settings.analysis_url = GURL(kTestUrl);
  cloud_settings.dm_token = dm_token();

  base::test::TestFuture<BinaryUploadService::Result,
                         enterprise_connectors::ContentAnalysisResponse>
      future;
  std::unique_ptr<TestRequest> request = std::make_unique<TestRequest>(
      future.GetCallback(), enterprise_connectors::CloudOrLocalAnalysisSettings(
                                std::move(cloud_settings)));
  request->set_analysis_connector(
      enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY);
  request->set_device_token(dm_token());
  request->set_per_profile_request(profile_request());

  service()->SetAuthForTesting(dm_token(),
                               BinaryUploadService::Result::SUCCESS);
  service()->MaybeUploadForDeepScanning(std::move(request));

  ASSERT_EQ(future.Get<0>(), BinaryUploadService::Result::SUCCESS);
}

IN_PROC_BROWSER_TEST_P(CloudBinaryUploadServiceRequestValidationBrowserTest,
                       FileAttach) {
  set_connector(enterprise_connectors::AnalysisConnector::FILE_ATTACHED);
  set_valid_fcm(true);

  enterprise_connectors::CloudAnalysisSettings cloud_settings;
  cloud_settings.analysis_url = GURL(kTestUrl);
  cloud_settings.dm_token = dm_token();

  base::test::TestFuture<BinaryUploadService::Result,
                         enterprise_connectors::ContentAnalysisResponse>
      future;
  std::unique_ptr<TestRequest> request = std::make_unique<TestRequest>(
      future.GetCallback(), enterprise_connectors::CloudOrLocalAnalysisSettings(
                                std::move(cloud_settings)));
  request->set_analysis_connector(
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED);
  request->set_device_token(dm_token());
  request->set_per_profile_request(profile_request());

  service()->SetAuthForTesting(dm_token(),
                               BinaryUploadService::Result::SUCCESS);
  service()->MaybeUploadForDeepScanning(std::move(request));

  ASSERT_EQ(future.Get<0>(), BinaryUploadService::Result::SUCCESS);
}

IN_PROC_BROWSER_TEST_P(CloudBinaryUploadServiceRequestValidationBrowserTest,
                       FileDownload) {
  set_connector(enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED);
  set_valid_fcm(true);

  enterprise_connectors::CloudAnalysisSettings cloud_settings;
  cloud_settings.analysis_url = GURL(kTestUrl);
  cloud_settings.dm_token = dm_token();

  base::test::TestFuture<BinaryUploadService::Result,
                         enterprise_connectors::ContentAnalysisResponse>
      future;
  std::unique_ptr<TestRequest> request = std::make_unique<TestRequest>(
      future.GetCallback(), enterprise_connectors::CloudOrLocalAnalysisSettings(
                                std::move(cloud_settings)));
  request->set_analysis_connector(
      enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED);
  request->set_device_token(dm_token());
  request->set_per_profile_request(profile_request());

  service()->SetAuthForTesting(dm_token(),
                               BinaryUploadService::Result::SUCCESS);
  service()->MaybeUploadForDeepScanning(std::move(request));

  ASSERT_EQ(future.Get<0>(), BinaryUploadService::Result::SUCCESS);
}

IN_PROC_BROWSER_TEST_P(CloudBinaryUploadServiceRequestValidationBrowserTest,
                       Print) {
  set_connector(enterprise_connectors::AnalysisConnector::PRINT);
  set_valid_fcm(true);

  enterprise_connectors::CloudAnalysisSettings cloud_settings;
  cloud_settings.analysis_url = GURL(kTestUrl);
  cloud_settings.dm_token = dm_token();

  base::test::TestFuture<BinaryUploadService::Result,
                         enterprise_connectors::ContentAnalysisResponse>
      future;
  std::unique_ptr<TestRequest> request = std::make_unique<TestRequest>(
      future.GetCallback(), enterprise_connectors::CloudOrLocalAnalysisSettings(
                                std::move(cloud_settings)));
  request->set_analysis_connector(
      enterprise_connectors::AnalysisConnector::PRINT);
  request->set_device_token(dm_token());
  request->set_per_profile_request(profile_request());

  service()->SetAuthForTesting(dm_token(),
                               BinaryUploadService::Result::SUCCESS);
  service()->MaybeUploadForDeepScanning(std::move(request));

  ASSERT_EQ(future.Get<0>(), BinaryUploadService::Result::SUCCESS);
}

INSTANTIATE_TEST_SUITE_P(All,
                         CloudBinaryUploadServiceRequestValidationBrowserTest,
                         testing::ValuesIn(kTestCases));

}  // namespace safe_browsing
