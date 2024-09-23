// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/cloud/chrome_browser_cloud_management_browsertest_delegate.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/browser/enterprise_switches.h"
#include "components/policy/core/common/cloud/chrome_browser_cloud_management_metrics.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/policy_storage.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/policy/cloud/chrome_browser_cloud_management_browsertest_delegate_android.h"
#else
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/policy/cloud/chrome_browser_cloud_management_browsertest_delegate_desktop.h"
#include "chrome/browser/ui/browser_finder.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/policy/cloud/chrome_browser_cloud_management_browsertest_mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace em = enterprise_management;

namespace policy {
namespace {

constexpr char kEnrollmentToken[] = "enrollment_token";
constexpr char kMachineName[] = "foo";
constexpr char kClientID[] = "fake_client_id";
constexpr char kDMToken[] = "fake_dm_token";
constexpr char kInvalidDMToken[] = "invalid_dm_token";
constexpr char kDeletionDMToken[] = "deletion_dm_token";
constexpr char kEnrollmentResultMetrics[] =
    "Enterprise.MachineLevelUserCloudPolicyEnrollment.Result";
const char kUnenrollmentSuccessMetrics[] =
    "Enterprise.MachineLevelUserCloudPolicyEnrollment.UnenrollSuccess";
const char kDmTokenDeletionMetrics[] =
    "Enterprise.MachineLevelUserCloudPolicyEnrollment.DMTokenDeletion";

#if BUILDFLAG(IS_ANDROID)
typedef ChromeBrowserCloudManagementBrowserTestDelegateAndroid
    ChromeBrowserCloudManagementBrowserTestDelegateType;
#else
typedef ChromeBrowserCloudManagementBrowserTestDelegateDesktop
    ChromeBrowserCloudManagementBrowserTestDelegateType;
#endif  // BUILDFLAG(IS_ANDROID)

void UpdatePolicyStorage(PolicyStorage* policy_storage) {
  em::CloudPolicySettings settings;
  em::BooleanPolicyProto* saving_browser_history_disabled =
      settings.mutable_savingbrowserhistorydisabled();
  saving_browser_history_disabled->mutable_policy_options()->set_mode(
      em::PolicyOptions::MANDATORY);
  saving_browser_history_disabled->set_value(true);

  policy_storage->SetPolicyPayload(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType,
      settings.SerializeAsString());
  policy_storage->set_robot_api_auth_code("fake_auth_code");
  policy_storage->set_service_account_identity("foo@bar.com");
}

ClientStorage::ClientInfo CreateTestClientInfo() {
  ClientStorage::ClientInfo client_info;
  client_info.device_id = kClientID;
  client_info.device_token = kDMToken;
  client_info.allowed_policy_types.insert(
      {dm_protocol::kChromeMachineLevelUserCloudPolicyType,
       dm_protocol::kChromeMachineLevelExtensionCloudPolicyType});
  return client_info;
}

class ChromeBrowserCloudManagementControllerObserver
    : public ChromeBrowserCloudManagementController::Observer {
 public:
  ChromeBrowserCloudManagementControllerObserver(
      ChromeBrowserCloudManagementBrowserTestDelegate* delegate)
      : delegate_(delegate) {}
  ~ChromeBrowserCloudManagementControllerObserver() override = default;

  void OnPolicyRegisterFinished(bool succeeded) override {
    delegate_->MaybeCheckDialogClosingAfterPolicyRegistration(
        !succeeded && should_display_error_message_);
    EXPECT_EQ(should_succeed_, succeeded);
    is_finished_ = true;
    g_browser_process->browser_policy_connector()
        ->chrome_browser_cloud_management_controller()
        ->RemoveObserver(this);
    EXPECT_EQ(
        delegate_->ExpectManagerImmediatelyInitialized(succeeded),
        g_browser_process->browser_policy_connector()
            ->machine_level_user_cloud_policy_manager()
            ->IsInitializationComplete(PolicyDomain::POLICY_DOMAIN_CHROME));
  }

  void SetShouldSucceed(bool should_succeed) {
    should_succeed_ = should_succeed;
  }

  void SetShouldDisplayErrorMessage(bool should_display) {
    should_display_error_message_ = should_display;
  }

  bool IsFinished() { return is_finished_; }

 private:
  raw_ptr<ChromeBrowserCloudManagementBrowserTestDelegate> delegate_;

  bool is_finished_ = false;
  bool should_succeed_ = false;
  bool should_display_error_message_ = false;
};

class ChromeBrowserExtraSetUp : public ChromeBrowserMainExtraParts {
 public:
  explicit ChromeBrowserExtraSetUp(
      ChromeBrowserCloudManagementControllerObserver* observer)
      : observer_(observer) {}
  ChromeBrowserExtraSetUp(const ChromeBrowserExtraSetUp&) = delete;
  ChromeBrowserExtraSetUp& operator=(const ChromeBrowserExtraSetUp&) = delete;
  void PreCreateMainMessageLoop() override {
    g_browser_process->browser_policy_connector()
        ->chrome_browser_cloud_management_controller()
        ->AddObserver(observer_);
  }

 private:
  raw_ptr<ChromeBrowserCloudManagementControllerObserver> observer_;
};

// Two observers that quit run_loop when policy is fetched and stored or in case
// the core is disconnected in case of error.
class PolicyFetchStoreObserver : public CloudPolicyStore::Observer {
 public:
  PolicyFetchStoreObserver(CloudPolicyStore* store,
                           base::OnceClosure quit_closure)
      : store_(store), quit_closure_(std::move(quit_closure)) {
    store_->AddObserver(this);
  }
  PolicyFetchStoreObserver(const PolicyFetchStoreObserver&) = delete;
  PolicyFetchStoreObserver& operator=(const PolicyFetchStoreObserver&) = delete;
  ~PolicyFetchStoreObserver() override { store_->RemoveObserver(this); }

  void OnStoreLoaded(CloudPolicyStore* store) override {
    std::move(quit_closure_).Run();
    is_succesfully_loaded_ = true;
  }
  void OnStoreError(CloudPolicyStore* store) override {
    std::move(quit_closure_).Run();
    is_succesfully_loaded_ = false;
  }

  bool is_successfully_loaded() const { return is_succesfully_loaded_; }

 private:
  raw_ptr<CloudPolicyStore> store_;
  base::OnceClosure quit_closure_;
  bool is_succesfully_loaded_;
};

class PolicyFetchCoreObserver : public CloudPolicyCore::Observer {
 public:
  PolicyFetchCoreObserver(CloudPolicyCore* core, base::OnceClosure quit_closure)
      : core_(core), quit_closure_(std::move(quit_closure)) {
    core_->AddObserver(this);
  }
  ~PolicyFetchCoreObserver() override { core_->RemoveObserver(this); }

  void OnCoreConnected(CloudPolicyCore* core) override {}

  void OnRefreshSchedulerStarted(CloudPolicyCore* core) override {}

  void OnCoreDisconnecting(CloudPolicyCore* core) override {
    // This is called when policy fetching fails and is used in
    // ChromeBrowserCloudManagementController to unenroll the browser. The
    // status must be either `DM_STATUS_SERVICE_DEVICE_NOT_FOUND` or
    // `DM_STATUS_SERVICE_DEVICE_NEEDS_RESET` for this to happen.
    EXPECT_THAT((std::array{DM_STATUS_SERVICE_DEVICE_NOT_FOUND,
                            DM_STATUS_SERVICE_DEVICE_NEEDS_RESET}),
                testing::Contains(core->client()->last_dm_status()));
    std::move(quit_closure_).Run();
  }

  void OnRemoteCommandsServiceStarted(CloudPolicyCore* core) override {}

 private:
  raw_ptr<CloudPolicyCore> core_;
  base::OnceClosure quit_closure_;
};

}  // namespace

class ChromeBrowserCloudManagementServiceIntegrationTest
    : public PlatformBrowserTest,
      public testing::WithParamInterface<std::string (  // NOLINT
          ChromeBrowserCloudManagementServiceIntegrationTest::*)(void)> {
 public:
  MOCK_METHOD4(OnJobDone,
               void(DeviceManagementService::Job*,
                    DeviceManagementStatus,
                    int,
                    const std::string&));

  std::string InitTestServer() {
    StartTestServer();
    return test_server_->GetServiceURL().spec();
  }

 protected:
  void PerformRegistration(const std::string& enrollment_token,
                           const std::string& machine_name,
                           bool expect_success) {
    base::RunLoop run_loop;
    if (expect_success) {
      EXPECT_CALL(*this, OnJobDone(_, testing::Eq(DM_STATUS_SUCCESS), _, _))
          .WillOnce(DoAll(
              Invoke(this, &ChromeBrowserCloudManagementServiceIntegrationTest::
                               RecordToken),
              InvokeWithoutArgs(&run_loop, &base::RunLoop::QuitWhenIdle)));
    } else {
      EXPECT_CALL(*this, OnJobDone(_, testing::Ne(DM_STATUS_SUCCESS), _, _))
          .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::QuitWhenIdle));
    }

    auto params = DMServerJobConfiguration::CreateParams::WithoutClient(
        DeviceManagementService::JobConfiguration::TYPE_BROWSER_REGISTRATION,
        service_.get(), kClientID,
        g_browser_process->system_network_context_manager()
            ->GetSharedURLLoaderFactory());
    params.auth_data = !enrollment_token.empty()
                           ? DMAuth::FromEnrollmentToken(enrollment_token)
                           : DMAuth::NoAuth();

    std::unique_ptr<FakeJobConfiguration> config =
        std::make_unique<FakeJobConfiguration>(
            std::move(params),
            base::BindOnce(
                &ChromeBrowserCloudManagementServiceIntegrationTest::OnJobDone,
                base::Unretained(this)),
            base::DoNothing(), base::DoNothing());

    em::DeviceManagementRequest request;
    em::RegisterBrowserRequest* register_browser_request =
        request.mutable_register_browser_request();
    register_browser_request->set_os_platform(GetOSPlatform());
    if (!machine_name.empty()) {
      register_browser_request->set_machine_name(machine_name);
    }
    std::string payload;
    ASSERT_TRUE(request.SerializeToString(&payload));
    config->SetRequestPayload(payload);

    std::unique_ptr<DeviceManagementService::Job> job =
        service_->CreateJob(std::move(config));

    run_loop.Run();
  }

  void UploadChromeDesktopReport(
      const em::ChromeDesktopReportRequest* chrome_desktop_report) {
    base::RunLoop run_loop;

    EXPECT_CALL(*this, OnJobDone(_, testing::Eq(DM_STATUS_SUCCESS), _, _))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::QuitWhenIdle));

    auto params = DMServerJobConfiguration::CreateParams::WithoutClient(
        DeviceManagementService::JobConfiguration::TYPE_CHROME_DESKTOP_REPORT,
        service_.get(), kClientID,
        g_browser_process->system_network_context_manager()
            ->GetSharedURLLoaderFactory());
    params.auth_data = DMAuth::FromEnrollmentToken(kDMToken);

    std::unique_ptr<FakeJobConfiguration> config =
        std::make_unique<FakeJobConfiguration>(
            std::move(params),
            base::BindOnce(
                &ChromeBrowserCloudManagementServiceIntegrationTest::OnJobDone,
                base::Unretained(this)),
            /* retry_callback */ base::DoNothing(),
            /* should_retry_callback */ base::DoNothing());

    std::unique_ptr<DeviceManagementService::Job> job =
        service_->CreateJob(std::move(config));

    run_loop.Run();
  }

  void SetUpOnMainThread() override {
    std::string service_url((this->*(GetParam()))());
    service_ = std::make_unique<DeviceManagementService>(
        std::unique_ptr<DeviceManagementService::Configuration>(
            new MockDeviceManagementServiceConfiguration(service_url)));
    service_->ScheduleInitialization(0);
    base::RunLoop().RunUntilIdle();
  }

  void TearDownOnMainThread() override {
    service_.reset();
    test_server_.reset();
  }

  void StartTestServer() {
    test_server_ = std::make_unique<EmbeddedPolicyTestServer>();
    ASSERT_TRUE(test_server_->Start());
  }

  void RecordToken(DeviceManagementService::Job* job,
                   DeviceManagementStatus code,
                   int net_error,
                   const std::string& response_body) {
    em::DeviceManagementResponse response;
    ASSERT_TRUE(response.ParseFromString(response_body));
    token_ = response.register_response().device_management_token();
  }

  ChromeBrowserCloudManagementBrowserTestDelegateType delegate_;

  std::string token_;
  std::unique_ptr<DeviceManagementService> service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<EmbeddedPolicyTestServer> test_server_;
};

IN_PROC_BROWSER_TEST_P(ChromeBrowserCloudManagementServiceIntegrationTest,
                       Registration) {
  ASSERT_TRUE(token_.empty());
  PerformRegistration(kEnrollmentToken, kMachineName, /*expect_success=*/true);
  EXPECT_FALSE(token_.empty());
}

IN_PROC_BROWSER_TEST_P(ChromeBrowserCloudManagementServiceIntegrationTest,
                       RegistrationNoEnrollmentToken) {
  ASSERT_TRUE(token_.empty());
  PerformRegistration(std::string(), kMachineName, /*expect_success=*/false);
  EXPECT_TRUE(token_.empty());
}

IN_PROC_BROWSER_TEST_P(ChromeBrowserCloudManagementServiceIntegrationTest,
                       RegistrationNoMachineName) {
  ASSERT_TRUE(token_.empty());
  bool expect_success = delegate_.AcceptEmptyMachineNameOnBrowserRegistration();
  PerformRegistration(kEnrollmentToken, std::string(), expect_success);
  EXPECT_NE(token_.empty(), expect_success);
}

#if BUILDFLAG(IS_ANDROID)
// TODO(http://crbug.com/1091438): Enable this test on Android once reporting is
// implemented.
#define MAYBE_ChromeDesktopReport DISABLED_ChromeDesktopReport
#else
#define MAYBE_ChromeDesktopReport ChromeDesktopReport
#endif  // BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(ChromeBrowserCloudManagementServiceIntegrationTest,
                       MAYBE_ChromeDesktopReport) {
  em::ChromeDesktopReportRequest chrome_desktop_report;
  UploadChromeDesktopReport(&chrome_desktop_report);
}

INSTANTIATE_TEST_SUITE_P(
    ChromeBrowserCloudManagementServiceIntegrationTestInstance,
    ChromeBrowserCloudManagementServiceIntegrationTest,
    testing::Values(
        &ChromeBrowserCloudManagementServiceIntegrationTest::InitTestServer));

class CloudPolicyStoreObserverStub : public CloudPolicyStore::Observer {
 public:
  CloudPolicyStoreObserverStub() = default;
  CloudPolicyStoreObserverStub(const CloudPolicyStoreObserverStub&) = delete;
  CloudPolicyStoreObserverStub& operator=(const CloudPolicyStoreObserverStub&) =
      delete;

  bool was_called() const { return on_loaded_ || on_error_; }

 private:
  // CloudPolicyStore::Observer
  void OnStoreLoaded(CloudPolicyStore* store) override { on_loaded_ = true; }
  void OnStoreError(CloudPolicyStore* store) override { on_error_ = true; }

  bool on_loaded_ = false;
  bool on_error_ = false;
};

class MachineLevelUserCloudPolicyManagerTest : public PlatformBrowserTest {
 protected:
  int CreateAndInitManager(const std::string& dm_token) {
    base::ScopedAllowBlockingForTesting scope_for_testing;
    std::string client_id("client_id");
    base::FilePath user_data_dir;
    CombinedSchemaRegistry schema_registry;
    CloudPolicyStoreObserverStub observer;

    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    DMToken browser_dm_token = dm_token.empty()
                                   ? DMToken::CreateEmptyToken()
                                   : DMToken::CreateValidToken(dm_token);
    std::unique_ptr<MachineLevelUserCloudPolicyStore> policy_store =
        MachineLevelUserCloudPolicyStore::Create(
            browser_dm_token, client_id, base::FilePath(), user_data_dir,
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
    policy_store->AddObserver(&observer);

    base::FilePath policy_dir = user_data_dir.Append(
        ChromeBrowserCloudManagementController::kPolicyDir);

    std::unique_ptr<MachineLevelUserCloudPolicyManager> manager =
        std::make_unique<MachineLevelUserCloudPolicyManager>(
            std::move(policy_store), nullptr, policy_dir,
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            base::BindRepeating(&content::GetNetworkConnectionTracker));
    manager->Init(&schema_registry);

    manager->store()->RemoveObserver(&observer);
    manager->Shutdown();
    return observer.was_called();
  }

  ChromeBrowserCloudManagementBrowserTestDelegateType delegate_;
};

IN_PROC_BROWSER_TEST_F(MachineLevelUserCloudPolicyManagerTest, NoDmToken) {
  EXPECT_EQ(CreateAndInitManager(std::string()),
            delegate_.ExpectOnStoreEventFired());
}

IN_PROC_BROWSER_TEST_F(MachineLevelUserCloudPolicyManagerTest, WithDmToken) {
  EXPECT_TRUE(CreateAndInitManager("dummy_dm_token"));
}

class ChromeBrowserCloudManagementEnrollmentTest
    : public PlatformBrowserTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  ChromeBrowserCloudManagementEnrollmentTest() : observer_(&delegate_) {
    BrowserDMTokenStorage::SetForTesting(&storage_);
    storage_.SetEnrollmentToken(enrollment_token());
    storage_.SetClientId("client_id");
    storage_.EnableStorage(storage_enabled());
    storage_.SetEnrollmentErrorOption(should_display_error_message());

    observer_.SetShouldSucceed(is_enrollment_token_valid());
    observer_.SetShouldDisplayErrorMessage(should_display_error_message());

    if (!is_enrollment_token_valid() && should_display_error_message()) {
      set_expected_exit_code(
          chrome::RESULT_CODE_CLOUD_POLICY_ENROLLMENT_FAILED);
    }
  }

  ChromeBrowserCloudManagementEnrollmentTest(
      const ChromeBrowserCloudManagementEnrollmentTest&) = delete;
  ChromeBrowserCloudManagementEnrollmentTest& operator=(
      const ChromeBrowserCloudManagementEnrollmentTest&) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(test_server_.Start());

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kDeviceManagementUrl,
                                    test_server_.GetServiceURL().spec());
    ChromeBrowserPolicyConnector::EnableCommandLineSupportForTesting();

    histogram_tester_.ExpectTotalCount(kEnrollmentResultMetrics, 0);
  }

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    PlatformBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->AppendSwitch(::switches::kEnableChromeBrowserCloudManagement);
  }
#endif

  void TearDownInProcessBrowserTestFixture() override {
    // Test body is skipped if enrollment failed as Chrome quit early.
    // Verify the enrollment result in the tear down instead.
    if (!is_enrollment_token_valid()) {
      VerifyEnrollmentResult();
    }
  }

  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    PlatformBrowserTest::CreatedBrowserMainParts(parts);
    static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
        std::make_unique<ChromeBrowserExtraSetUp>(&observer_));
  }

  void VerifyEnrollmentResult() {
    DMToken dm_token = BrowserDMTokenStorage::Get()->RetrieveDMToken();
    if (is_enrollment_token_valid()) {
      EXPECT_TRUE(dm_token.is_valid());
      EXPECT_EQ(kFakeDeviceToken, dm_token.value());
    } else {
      EXPECT_TRUE(dm_token.is_empty());
    }

    // Verify the enrollment result.
    ChromeBrowserCloudManagementEnrollmentResult expected_result;
    if (is_enrollment_token_valid() && storage_enabled()) {
      expected_result = ChromeBrowserCloudManagementEnrollmentResult::kSuccess;
    } else if (is_enrollment_token_valid() && !storage_enabled()) {
      expected_result =
          ChromeBrowserCloudManagementEnrollmentResult::kFailedToStore;
    } else {
      expected_result =
          ChromeBrowserCloudManagementEnrollmentResult::kFailedToFetch;
    }

    // Verify the metrics.
    histogram_tester_.ExpectBucketCount(kEnrollmentResultMetrics,
                                        expected_result, 1);
    histogram_tester_.ExpectTotalCount(kEnrollmentResultMetrics, 1);
  }

 protected:
  bool is_enrollment_token_valid() const { return std::get<0>(GetParam()); }
  bool storage_enabled() const { return std::get<1>(GetParam()); }
  bool should_display_error_message() const { return std::get<2>(GetParam()); }
  std::string enrollment_token() const {
    return is_enrollment_token_valid() ? kEnrollmentToken
                                       : kInvalidEnrollmentToken;
  }

  ChromeBrowserCloudManagementBrowserTestDelegateType delegate_;

  base::HistogramTester histogram_tester_;

 private:
  EmbeddedPolicyTestServer test_server_;
  FakeBrowserDMTokenStorage storage_;
  ChromeBrowserCloudManagementControllerObserver observer_;
};

// Consistently timing out on Windows. http://crbug.com/1025220
#if BUILDFLAG(IS_WIN)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
IN_PROC_BROWSER_TEST_P(ChromeBrowserCloudManagementEnrollmentTest, MAYBE_Test) {
#undef MAYBE_Test
  // Test body is run only if enrollment is succeeded or failed without error
  // message.
  EXPECT_TRUE(is_enrollment_token_valid() || !should_display_error_message());

  delegate_.MaybeWaitForEnrollmentConfirmation(enrollment_token());

  delegate_.MaybeCheckTotalBrowserCount(1u);

  VerifyEnrollmentResult();
}

#if BUILDFLAG(IS_ANDROID)
// No need to run this test with |should_display_error_message| equals true on
// Android.
INSTANTIATE_TEST_SUITE_P(
    ChromeBrowserCloudManagementEnrollmentTest,
    ChromeBrowserCloudManagementEnrollmentTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Bool(),
        /*should_display_error_message=*/::testing::Values(false)));
#else
INSTANTIATE_TEST_SUITE_P(ChromeBrowserCloudManagementEnrollmentTest,
                         ChromeBrowserCloudManagementEnrollmentTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));
#endif  // BUILDFLAG(IS_ANDROID)

class MachineLevelUserCloudPolicyPolicyFetchObserver
    : public ChromeBrowserCloudManagementControllerObserver {
 public:
  MachineLevelUserCloudPolicyPolicyFetchObserver(
      ChromeBrowserCloudManagementBrowserTestDelegate* delegate)
      : ChromeBrowserCloudManagementControllerObserver(delegate) {}
  ~MachineLevelUserCloudPolicyPolicyFetchObserver() override = default;

  void QuitOnUnenroll(base::RepeatingClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

  void OnBrowserUnenrolled(bool succeeded) override {
    if (!quit_closure_.is_null()) {
      EXPECT_FALSE(succeeded);
      std::move(quit_closure_).Run();
    }
  }

 private:
  base::RepeatingClosure quit_closure_;
};

class MachineLevelUserCloudPolicyPolicyFetchTest
    : public PlatformBrowserTest,
      public ::testing::WithParamInterface<std::tuple<
          /*dm_token=*/std::string,
          /*storage_enabled=*/bool,
          /*is_policy_fetch_with_sha256_enabled=*/bool>> {
 public:
  MachineLevelUserCloudPolicyPolicyFetchTest() : observer_(&delegate_) {
    BrowserDMTokenStorage::SetForTesting(&storage_);
    storage_.SetEnrollmentToken(kEnrollmentToken);
    storage_.SetClientId(kClientID);
    storage_.EnableStorage(storage_enabled());
    if (!dm_token().empty()) {
      storage_.SetDMToken(dm_token());
    }

    if (is_policy_fetch_with_sha256_enabled()) {
      scoped_feature_list_.InitAndEnableFeature(policy::kPolicyFetchWithSha256);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          policy::kPolicyFetchWithSha256);
    }
  }
  MachineLevelUserCloudPolicyPolicyFetchTest(
      const MachineLevelUserCloudPolicyPolicyFetchTest&) = delete;
  MachineLevelUserCloudPolicyPolicyFetchTest& operator=(
      const MachineLevelUserCloudPolicyPolicyFetchTest&) = delete;

  void SetUpOnMainThread() override {
    g_browser_process->browser_policy_connector()
        ->chrome_browser_cloud_management_controller()
        ->AddObserver(&observer_);
  }

  void TearDownOnMainThread() override {
    g_browser_process->browser_policy_connector()
        ->chrome_browser_cloud_management_controller()
        ->RemoveObserver(&observer_);
  }

  void SetUpInProcessBrowserTestFixture() override {
    SetUpTestServer();
    ASSERT_TRUE(test_server_->Start());

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kDeviceManagementUrl,
                                    test_server_->GetServiceURL().spec());
    ChromeBrowserPolicyConnector::EnableCommandLineSupportForTesting();
  }

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    PlatformBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->AppendSwitch(::switches::kEnableChromeBrowserCloudManagement);
  }
#endif

  void SetUpTestServer() {
    test_server_ = std::make_unique<EmbeddedPolicyTestServer>();
    UpdatePolicyStorage(test_server_->policy_storage());
    // Configure the policy server to signal that DMToken deletion has been
    // requested via the DMServer response.
    if (dm_token() == kDeletionDMToken) {
      test_server_->policy_storage()->set_error_detail(
          em::CBCM_DELETION_POLICY_PREFERENCE_DELETE_TOKEN);
    }
    test_server_->client_storage()->RegisterClient(CreateTestClientInfo());
  }

  DMToken retrieve_dm_token() { return storage_.RetrieveDMToken(); }

  const std::string dm_token() const { return std::get<0>(GetParam()); }
  bool storage_enabled() const { return std::get<1>(GetParam()); }
  bool is_policy_fetch_with_sha256_enabled() const {
    return std::get<2>(GetParam());
  }

 protected:
  ChromeBrowserCloudManagementBrowserTestDelegateType delegate_;
  MachineLevelUserCloudPolicyPolicyFetchObserver observer_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<EmbeddedPolicyTestServer> test_server_;
  FakeBrowserDMTokenStorage storage_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
// TODO(crbug.com/40782028): Test is flaky.
IN_PROC_BROWSER_TEST_P(MachineLevelUserCloudPolicyPolicyFetchTest,
                       DISABLED_Test) {
#else
IN_PROC_BROWSER_TEST_P(MachineLevelUserCloudPolicyPolicyFetchTest, Test) {
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  MachineLevelUserCloudPolicyManager* manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();
  ASSERT_TRUE(manager);
  // If the policy hasn't been updated, wait for it.
  if (manager->core()->client()->last_policy_timestamp().is_null()) {
    base::RunLoop run_loop;
    // Listen to store event which is fired after policy validation if token is
    // valid. Otherwise listen to the core since it gets disconnected by
    // unenrollment.
    std::unique_ptr<PolicyFetchCoreObserver> core_observer;
    std::unique_ptr<PolicyFetchStoreObserver> store_observer;
    if (dm_token() == kInvalidDMToken || dm_token() == kDeletionDMToken) {
      if (storage_enabled()) {
        // |run_loop|'s QuitClosure will be called after the core is
        // disconnected following unenrollment.
        core_observer = std::make_unique<PolicyFetchCoreObserver>(
            manager->core(), run_loop.QuitClosure());
      } else {
        // |run_loop|'s QuitClosure will be called after the browser attempts to
        // unenroll from CBCM. This is necessary to quit the loop in the case
        // the storage fails since the core is not disconnected.
        observer_.QuitOnUnenroll(run_loop.QuitClosure());
      }
    } else {
      store_observer = std::make_unique<PolicyFetchStoreObserver>(
          manager->store(), run_loop.QuitClosure());
    }
    g_browser_process->browser_policy_connector()
        ->device_management_service()
        ->ScheduleInitialization(0);
    run_loop.Run();
  }
  EXPECT_TRUE(
      manager->IsInitializationComplete(PolicyDomain::POLICY_DOMAIN_CHROME));

  const PolicyMap& policy_map = manager->store()->policy_map();
  DMToken token = retrieve_dm_token();

  if (dm_token() == kInvalidDMToken) {
    EXPECT_EQ(0u, policy_map.size());
    // The token in storage should be invalid.
    EXPECT_TRUE(token.is_invalid());
    histogram_tester_.ExpectUniqueSample(kUnenrollmentSuccessMetrics,
                                         storage_enabled(), 1);
    histogram_tester_.ExpectTotalCount(kDmTokenDeletionMetrics, 0);
  } else if (dm_token() == kDeletionDMToken) {
    EXPECT_EQ(0u, policy_map.size());
    // The token in storage should be empty.
    EXPECT_TRUE(token.is_empty());
    histogram_tester_.ExpectTotalCount(kUnenrollmentSuccessMetrics, 0);
    histogram_tester_.ExpectUniqueSample(kDmTokenDeletionMetrics,
                                         storage_enabled(), 1);
  } else {
    EXPECT_EQ(1u, policy_map.size());
    EXPECT_EQ(base::Value(true),
              *(policy_map.Get(key::kSavingBrowserHistoryDisabled)
                    ->value(base::Value::Type::BOOLEAN)));
    // The token in storage should be valid.
    EXPECT_TRUE(token.is_valid());

    // The test server will register with kFakeDeviceToken if
    // Chrome is started without a DM token.
    if (dm_token().empty()) {
      EXPECT_EQ(token.value(), kFakeDeviceToken);
    } else {
      EXPECT_EQ(token.value(), kDMToken);
    }

    histogram_tester_.ExpectTotalCount(kUnenrollmentSuccessMetrics, 0);
    histogram_tester_.ExpectTotalCount(kDmTokenDeletionMetrics, 0);
  }
}

// The tests here cover three DM token cases combined with the storage
// succeeding or failing:
//  1) Start Chrome with a valid DM token but no policy cache. Chrome will
//  load the policy from the DM server.
//  2) Start Chrome with an invalid DM token. Chrome will hit the DM server and
//  get an error. There should be no more cloud policy applied.
//  3) Start Chrome without DM token. Chrome will register itself and fetch
//  policy after it.
// The tests also cover the migration of the policy stack to SHA256_RSA
// signature algorithm.
INSTANTIATE_TEST_SUITE_P(
    MachineLevelUserCloudPolicyPolicyFetchTest,
    MachineLevelUserCloudPolicyPolicyFetchTest,
    ::testing::Combine(
        /*dm_token=*/::testing::Values(kDMToken,
                                       kInvalidDMToken,
                                       kDeletionDMToken,
                                       ""),
        /*storage_enabled=*/::testing::Bool(),
        /*is_policy_fetch_with_sha256_enabled=*/::testing::Bool()));

#if !BUILDFLAG(IS_ANDROID)
class MachineLevelUserCloudPolicyPolicyFetchKeyRotationTest
    : public MachineLevelUserCloudPolicyPolicyFetchTest {
 public:
  MachineLevelUserCloudPolicyPolicyFetchKeyRotationTest() = default;
  ~MachineLevelUserCloudPolicyPolicyFetchKeyRotationTest() override = default;
  MachineLevelUserCloudPolicyPolicyFetchKeyRotationTest(
      const MachineLevelUserCloudPolicyPolicyFetchKeyRotationTest&) = delete;
  MachineLevelUserCloudPolicyPolicyFetchKeyRotationTest& operator=(
      const MachineLevelUserCloudPolicyPolicyFetchKeyRotationTest&) = delete;
};

IN_PROC_BROWSER_TEST_P(MachineLevelUserCloudPolicyPolicyFetchKeyRotationTest,
                       KeyRotationTest) {
  MachineLevelUserCloudPolicyManager* manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();
  ASSERT_TRUE(manager);
  test_server_->policy_storage()->signature_provider()->set_rotate_keys(true);
  // If the policy hasn't been updated, force the initialization which will
  // force policy fetch.
  if (manager->core()->client()->last_policy_timestamp().is_null()) {
    base::RunLoop run_loop;
    std::unique_ptr<PolicyFetchStoreObserver> store_observer =
        std::make_unique<PolicyFetchStoreObserver>(manager->store(),
                                                   run_loop.QuitClosure());
    g_browser_process->browser_policy_connector()
        ->device_management_service()
        ->ScheduleInitialization(0);
    run_loop.Run();
  }
  ASSERT_TRUE(
      manager->IsInitializationComplete(PolicyDomain::POLICY_DOMAIN_CHROME));
  const PolicyMap& policy_map = manager->store()->policy_map();
  EXPECT_EQ(base::Value(true),
            *(policy_map.Get(key::kSavingBrowserHistoryDisabled)
                  ->value(base::Value::Type::BOOLEAN)));
  int current_public_key_version =
      manager->store()->policy()->public_key_version();

  // Configure new policies on the server and refresh policies on the client.
  // This will force the key rotation.
  {
    em::CloudPolicySettings settings;
    em::BooleanPolicyProto* saving_browser_history_disabled =
        settings.mutable_savingbrowserhistorydisabled();
    saving_browser_history_disabled->mutable_policy_options()->set_mode(
        em::PolicyOptions::MANDATORY);
    saving_browser_history_disabled->set_value(false);
    test_server_->policy_storage()->SetPolicyPayload(
        dm_protocol::kChromeMachineLevelUserCloudPolicyType,
        settings.SerializeAsString());
    base::RunLoop run_loop;
    std::unique_ptr<PolicyFetchStoreObserver> store_observer =
        std::make_unique<PolicyFetchStoreObserver>(manager->store(),
                                                   run_loop.QuitClosure());
    manager->RefreshPolicies(PolicyFetchReason::kTest);
    run_loop.Run();
  }

  // Verify new policy and verify that key has been rotated.
  EXPECT_EQ(base::Value(false),
            *(policy_map.Get(key::kSavingBrowserHistoryDisabled)
                  ->value(base::Value::Type::BOOLEAN)));
  EXPECT_EQ(current_public_key_version + 1,
            manager->store()->policy()->public_key_version());
  current_public_key_version = manager->store()->policy()->public_key_version();
  // Verify that policies are reloaded correctly with the new key.
  {
    manager->store()->Load();
    base::RunLoop run_loop;
    PolicyFetchStoreObserver store_observer(manager->store(),
                                            run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_TRUE(store_observer.is_successfully_loaded());
    EXPECT_EQ(base::Value(false),
              *(policy_map.Get(key::kSavingBrowserHistoryDisabled)
                    ->value(base::Value::Type::BOOLEAN)));
    EXPECT_EQ(current_public_key_version,
              manager->store()->policy()->public_key_version());
  }
}

INSTANTIATE_TEST_SUITE_P(
    MachineLevelUserCloudPolicyPolicyFetchKeyRotationTest,
    MachineLevelUserCloudPolicyPolicyFetchKeyRotationTest,
    ::testing::Combine(
        /*dm_token=*/::testing::Values(kDMToken),
        /*storage_enabled=*/::testing::Values(true),
        /*is_policy_fetch_with_sha256_enabled=*/::testing::Bool()));
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
class MachineLevelUserCloudPolicyRobotAuthTest : public PlatformBrowserTest {
 public:
  MachineLevelUserCloudPolicyRobotAuthTest() : observer_(&delegate_) {
    BrowserDMTokenStorage::SetForTesting(&storage_);
    storage_.SetEnrollmentToken(kEnrollmentToken);
    storage_.SetClientId(kClientID);
    storage_.EnableStorage(true);
    storage_.SetDMToken(kDMToken);
  }

  void SetUpOnMainThread() override {
    g_browser_process->browser_policy_connector()
        ->chrome_browser_cloud_management_controller()
        ->AddObserver(&observer_);
    test_url_loader_factory_.AddResponse(
        GaiaUrls::GetInstance()->oauth2_token_url().spec(),
        R"P({
          "access_token":"at",
          "refresh_token":"rt",
          "expires_in":9999
  })P");
    g_browser_process->browser_policy_connector()
        ->chrome_browser_cloud_management_controller()
        ->SetGaiaURLLoaderFactory(
            test_url_loader_factory_.GetSafeWeakWrapper());
  }

  void TearDownOnMainThread() override {
    g_browser_process->browser_policy_connector()
        ->chrome_browser_cloud_management_controller()
        ->RemoveObserver(&observer_);
  }

  void SetUpInProcessBrowserTestFixture() override {
    SetUpTestServer();
    ASSERT_TRUE(test_server_->Start());

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kDeviceManagementUrl,
                                    test_server_->GetServiceURL().spec());
    ChromeBrowserPolicyConnector::EnableCommandLineSupportForTesting();
  }

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    PlatformBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->AppendSwitch(::switches::kEnableChromeBrowserCloudManagement);
  }
#endif

  void SetUpTestServer() {
    test_server_ = std::make_unique<EmbeddedPolicyTestServer>();
    UpdatePolicyStorage(test_server_->policy_storage());
    test_server_->client_storage()->RegisterClient(CreateTestClientInfo());
  }

  DMToken retrieve_dm_token() { return storage_.RetrieveDMToken(); }

 private:
  ChromeBrowserCloudManagementBrowserTestDelegateType delegate_;
  std::unique_ptr<EmbeddedPolicyTestServer> test_server_;
  FakeBrowserDMTokenStorage storage_;
  base::ScopedTempDir temp_dir_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  ChromeBrowserCloudManagementControllerObserver observer_;
};  // namespace policy

// Flaky on linux & win: https://crbug.com/1105167
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
IN_PROC_BROWSER_TEST_F(MachineLevelUserCloudPolicyRobotAuthTest, MAYBE_Test) {
  MachineLevelUserCloudPolicyManager* manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();
  ASSERT_TRUE(manager);
  // If the policy hasn't been updated, wait for it.
  if (manager->core()->client()->last_policy_timestamp().is_null()) {
    base::RunLoop run_loop;
    // Listen to store event which is fired after policy validation if token is
    // valid.
    auto store_observer = std::make_unique<PolicyFetchStoreObserver>(
        manager->store(), run_loop.QuitClosure());

    g_browser_process->browser_policy_connector()
        ->device_management_service()
        ->ScheduleInitialization(0);
    run_loop.Run();
  }
  EXPECT_TRUE(
      manager->IsInitializationComplete(PolicyDomain::POLICY_DOMAIN_CHROME));

  const PolicyMap& policy_map = manager->store()->policy_map();

  EXPECT_EQ(1u, policy_map.size());
  EXPECT_EQ(base::Value(true),
            *(policy_map.Get(key::kSavingBrowserHistoryDisabled)
                  ->value(base::Value::Type::BOOLEAN)));

  // The token in storage should be valid.
  DMToken token = retrieve_dm_token();
  EXPECT_TRUE(token.is_valid());

  // The test server will register with kFakeDeviceToken if
  // Chrome is started without a DM token.
  EXPECT_EQ(token.value(), kDMToken);

  base::RunLoop run_loop;
  DeviceOAuth2TokenServiceFactory::Get()->SetRefreshTokenAvailableCallback(
      run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(
      DeviceOAuth2TokenServiceFactory::Get()->RefreshTokenIsAvailable());
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace policy
