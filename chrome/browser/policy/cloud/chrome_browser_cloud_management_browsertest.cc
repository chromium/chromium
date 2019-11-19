// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/chrome_browser_cloud_management_controller.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/fake_browser_dm_token_storage.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/cloud/chrome_browser_cloud_management_metrics.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/test_support/local_policy_test_server.h"
#include "content/public/browser/network_service_instance.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"

#if defined(OS_MACOSX)
#include "chrome/browser/policy/cloud/chrome_browser_cloud_management_browsertest_mac_util.h"
#endif

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace em = enterprise_management;

namespace policy {
namespace {

const char kEnrollmentToken[] = "enrollment_token";
const char kInvalidEnrollmentToken[] = "invalid_enrollment_token";
const char kMachineName[] = "foo";
const char kClientID[] = "fake_client_id";
const char kDMToken[] = "fake_dm_token";
const char kInvalidDMToken[] = "invalid_dm_token";
const char kEnrollmentResultMetrics[] =
    "Enterprise.MachineLevelUserCloudPolicyEnrollment.Result";
const char kTestPolicyConfig[] = R"(
{
  "google/chrome/machine-level-user" : {
    "mandatory": {
      "ShowHomeButton": true
    }
  }
}
)";

class ChromeBrowserCloudManagementControllerObserver
    : public ChromeBrowserCloudManagementController::Observer {
 public:
  void OnPolicyRegisterFinished(bool succeeded) override {
    if (!succeeded && should_display_error_message_) {
      EXPECT_EQ(0u, chrome::GetTotalBrowserCount());
#if defined(OS_MACOSX)
      PostAppControllerNSNotifications();
#endif
      // Close the error dialog.
      ASSERT_EQ(1u, views::test::WidgetTest::GetAllWidgets().size());
      (*views::test::WidgetTest::GetAllWidgets().begin())->Close();
    }
    EXPECT_EQ(should_succeed_, succeeded);
    is_finished_ = true;
    g_browser_process->browser_policy_connector()
        ->chrome_browser_cloud_management_controller()
        ->RemoveObserver(this);
    // If enrollment fails, the manager should be marked as initialized
    // immediately. Otherwise, this will be done after the policy data is
    // downloaded.
    EXPECT_EQ(!succeeded, g_browser_process->browser_policy_connector()
                              ->machine_level_user_cloud_policy_manager()
                              ->IsInitializationComplete(
                                  PolicyDomain::POLICY_DOMAIN_CHROME));
  }

  void SetShouldSucceed(bool should_succeed) {
    should_succeed_ = should_succeed;
  }

  void SetShouldDisplayErrorMessage(bool should_display) {
    should_display_error_message_ = should_display;
  }

  bool IsFinished() { return is_finished_; }

 private:
  bool is_finished_ = false;
  bool should_succeed_ = false;
  bool should_display_error_message_ = false;
};

class ChromeBrowserExtraSetUp : public ChromeBrowserMainExtraParts {
 public:
  explicit ChromeBrowserExtraSetUp(
      ChromeBrowserCloudManagementControllerObserver* observer)
      : observer_(observer) {}
  void PreMainMessageLoopStart() override {
    g_browser_process->browser_policy_connector()
        ->chrome_browser_cloud_management_controller()
        ->AddObserver(observer_);
  }

 private:
  ChromeBrowserCloudManagementControllerObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserExtraSetUp);
};

// Two observers that quit run_loop when policy is fetched and stored or in case
// there is any error.
class PolicyFetchStoreObserver : public CloudPolicyStore::Observer {
 public:
  PolicyFetchStoreObserver(CloudPolicyStore* store,
                           base::OnceClosure quit_closure)
      : store_(store), quit_closure_(std::move(quit_closure)) {
    store_->AddObserver(this);
  }
  ~PolicyFetchStoreObserver() override { store_->RemoveObserver(this); }

  void OnStoreLoaded(CloudPolicyStore* store) override {
    std::move(quit_closure_).Run();
  }
  void OnStoreError(CloudPolicyStore* store) override {
    std::move(quit_closure_).Run();
  }

 private:
  CloudPolicyStore* store_;
  base::OnceClosure quit_closure_;
  DISALLOW_COPY_AND_ASSIGN(PolicyFetchStoreObserver);
};

class PolicyFetchClientObserver : public CloudPolicyClient::Observer {
 public:
  PolicyFetchClientObserver(CloudPolicyClient* client,
                            base::OnceClosure quit_closure)
      : client_(client), quit_closure_(std::move(quit_closure)) {
    client_->AddObserver(this);
  }
  ~PolicyFetchClientObserver() override { client_->RemoveObserver(this); }

  void OnPolicyFetched(CloudPolicyClient* client) override {
    std::move(quit_closure_).Run();
  }

  void OnRegistrationStateChanged(CloudPolicyClient* client) override {}

  void OnClientError(CloudPolicyClient* client) override {
    std::move(quit_closure_).Run();
  }

 private:
  CloudPolicyClient* client_;
  base::OnceClosure quit_closure_;
  std::unique_ptr<PolicyFetchStoreObserver> store_observer_;
  DISALLOW_COPY_AND_ASSIGN(PolicyFetchClientObserver);
};

}  // namespace

class ChromeBrowserCloudManagementServiceIntegrationTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<std::string(
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

    std::unique_ptr<FakeJobConfiguration> config =
        std::make_unique<FakeJobConfiguration>(
            service_.get(),
            DeviceManagementService::JobConfiguration::TYPE_TOKEN_ENROLLMENT,
            kClientID,
            /*critical=*/false,
            !enrollment_token.empty()
                ? DMAuth::FromEnrollmentToken(enrollment_token)
                : DMAuth::NoAuth(),
            /*oauth_token=*/base::nullopt,
            g_browser_process->system_network_context_manager()
                ->GetSharedURLLoaderFactory(),
            base::BindOnce(
                &ChromeBrowserCloudManagementServiceIntegrationTest::OnJobDone,
                base::Unretained(this)),
            base::DoNothing());

    em::DeviceManagementRequest request;
    request.mutable_register_browser_request();
    if (!machine_name.empty()) {
      request.mutable_register_browser_request()->set_machine_name(
          machine_name);
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

    std::unique_ptr<FakeJobConfiguration> config = std::make_unique<
        FakeJobConfiguration>(
        service_.get(),
        DeviceManagementService::JobConfiguration::TYPE_CHROME_DESKTOP_REPORT,
        kClientID,
        /*critical=*/false, DMAuth::FromEnrollmentToken(kDMToken),
        /*oauth_token=*/std::string(),
        g_browser_process->system_network_context_manager()
            ->GetSharedURLLoaderFactory(),
        base::BindOnce(
            &ChromeBrowserCloudManagementServiceIntegrationTest::OnJobDone,
            base::Unretained(this)),
        base::DoNothing());

    std::unique_ptr<DeviceManagementService::Job> job =
        service_->CreateJob(std::move(config));

    run_loop.Run();
  }

  void SetUpOnMainThread() override {
    std::string service_url((this->*(GetParam()))());
    service_.reset(new DeviceManagementService(
        std::unique_ptr<DeviceManagementService::Configuration>(
            new MockDeviceManagementServiceConfiguration(service_url))));
    service_->ScheduleInitialization(0);
    base::RunLoop().RunUntilIdle();
  }

  void TearDownOnMainThread() override {
    service_.reset();
    test_server_.reset();
  }

  void StartTestServer() {
    test_server_.reset(new LocalPolicyTestServer(
        "chrome/test/data/policy/"
        "policy_machine_level_user_cloud_policy_service_browsertest.json"));
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

  std::string token_;
  std::unique_ptr<DeviceManagementService> service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<LocalPolicyTestServer> test_server_;
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
  PerformRegistration(kEnrollmentToken, std::string(),
                      /*expect_success=*/false);
  EXPECT_TRUE(token_.empty());
}

IN_PROC_BROWSER_TEST_P(ChromeBrowserCloudManagementServiceIntegrationTest,
                       ChromeDesktopReport) {
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
  CloudPolicyStoreObserverStub() {}

  bool was_called() const { return on_loaded_ || on_error_; }

 private:
  // CloudPolicyStore::Observer
  void OnStoreLoaded(CloudPolicyStore* store) override { on_loaded_ = true; }
  void OnStoreError(CloudPolicyStore* store) override { on_error_ = true; }

  bool on_loaded_ = false;
  bool on_error_ = false;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyStoreObserverStub);
};

class MachineLevelUserCloudPolicyManagerTest : public InProcessBrowserTest {
 protected:
  bool CreateAndInitManager(const std::string& dm_token) {
    base::ScopedAllowBlockingForTesting scope_for_testing;
    std::string client_id("client_id");
    base::FilePath user_data_dir;
    CombinedSchemaRegistry schema_registry;
    CloudPolicyStoreObserverStub observer;

    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

    std::unique_ptr<MachineLevelUserCloudPolicyStore> policy_store =
        MachineLevelUserCloudPolicyStore::Create(
            dm_token, client_id, user_data_dir,
            /*cloud_policy_overrides=*/false,
            base::CreateSequencedTaskRunner({base::ThreadPool(),
                                             base::MayBlock(),
                                             base::TaskPriority::BEST_EFFORT}));
    policy_store->AddObserver(&observer);

    base::FilePath policy_dir = user_data_dir.Append(
        ChromeBrowserCloudManagementController::kPolicyDir);

    std::unique_ptr<MachineLevelUserCloudPolicyManager> manager =
        std::make_unique<MachineLevelUserCloudPolicyManager>(
            std::move(policy_store), nullptr, policy_dir,
            base::ThreadTaskRunnerHandle::Get(),
            base::BindRepeating(&content::GetNetworkConnectionTracker));
    manager->Init(&schema_registry);

    manager->store()->RemoveObserver(&observer);
    manager->Shutdown();
    return observer.was_called();
  }
};

IN_PROC_BROWSER_TEST_F(MachineLevelUserCloudPolicyManagerTest, NoDmToken) {
  EXPECT_FALSE(CreateAndInitManager(std::string()));
}

IN_PROC_BROWSER_TEST_F(MachineLevelUserCloudPolicyManagerTest, WithDmToken) {
  EXPECT_TRUE(CreateAndInitManager("dummy_dm_token"));
}

class ChromeBrowserCloudManagementEnrollmentTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  ChromeBrowserCloudManagementEnrollmentTest() {
    BrowserDMTokenStorage::SetForTesting(&storage_);
    storage_.SetEnrollmentToken(is_enrollment_token_valid()
                                    ? kEnrollmentToken
                                    : kInvalidEnrollmentToken);
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

  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(test_server_.Start());

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kDeviceManagementUrl,
                                    test_server_.GetServiceURL().spec());

    histogram_tester_.ExpectTotalCount(kEnrollmentResultMetrics, 0);
  }

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
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
    static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
        new ChromeBrowserExtraSetUp(&observer_));
  }

  void VerifyEnrollmentResult() {
    auto dm_token = BrowserDMTokenStorage::Get()->RetrieveBrowserDMToken();
    if (is_enrollment_token_valid()) {
      EXPECT_TRUE(dm_token.is_valid());
      EXPECT_EQ("fake_device_management_token", dm_token.value());
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

  base::HistogramTester histogram_tester_;

 private:
  LocalPolicyTestServer test_server_;
  FakeBrowserDMTokenStorage storage_;
  ChromeBrowserCloudManagementControllerObserver observer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserCloudManagementEnrollmentTest);
};

// Consistently timing out on Windows. http://crbug.com/1025220
#if defined(OS_WIN)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
IN_PROC_BROWSER_TEST_P(ChromeBrowserCloudManagementEnrollmentTest, MAYBE_Test) {
  // Test body is run only if enrollment is succeeded or failed without error
  // message.
  EXPECT_TRUE(is_enrollment_token_valid() || !should_display_error_message());

  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  VerifyEnrollmentResult();
#if defined(OS_MACOSX)
  // Verify the last mericis of launch is recorded in
  // applicationDidFinishNotification.
  EXPECT_EQ(1u, histogram_tester_
                    .GetAllSamples("Startup.OSX.DockIconWillFinishBouncing")
                    .size());
#endif
}

INSTANTIATE_TEST_SUITE_P(,
                         ChromeBrowserCloudManagementEnrollmentTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

class MachineLevelUserCloudPolicyPolicyFetchTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  MachineLevelUserCloudPolicyPolicyFetchTest() {
    BrowserDMTokenStorage::SetForTesting(&storage_);
    storage_.SetEnrollmentToken(kEnrollmentToken);
    storage_.SetClientId(kClientID);
    if (!dm_token().empty())
      storage_.SetDMToken(dm_token());
  }

  void SetUpInProcessBrowserTestFixture() override {
    SetUpTestServer();
    ASSERT_TRUE(test_server_->Start());

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kDeviceManagementUrl,
                                    test_server_->GetServiceURL().spec());
  }

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->AppendSwitch(::switches::kEnableChromeBrowserCloudManagement);
  }
#endif

  void SetUpTestServer() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath config_path = temp_dir_.GetPath().AppendASCII("config.json");
    base::WriteFile(config_path, kTestPolicyConfig, strlen(kTestPolicyConfig));
    test_server_ = std::make_unique<LocalPolicyTestServer>(config_path);
    test_server_->RegisterClient(kDMToken, kClientID, {} /* state_keys */);
  }

  const std::string dm_token() const { return GetParam(); }

 private:
  std::unique_ptr<LocalPolicyTestServer> test_server_;
  FakeBrowserDMTokenStorage storage_;
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(MachineLevelUserCloudPolicyPolicyFetchTest);
};

IN_PROC_BROWSER_TEST_P(MachineLevelUserCloudPolicyPolicyFetchTest, Test) {
  MachineLevelUserCloudPolicyManager* manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();
  ASSERT_TRUE(manager);
  // If the policy hasn't been updated, wait for it.
  if (manager->core()->client()->last_policy_timestamp().is_null()) {
    base::RunLoop run_loop;
    // Listen to store event which is fired after policy validation if token is
    // valid. Otherwise listen to the client event because there is no store
    // event.
    std::unique_ptr<PolicyFetchClientObserver> client_observer;
    std::unique_ptr<PolicyFetchStoreObserver> store_observer;
    if (dm_token() == kInvalidDMToken) {
      client_observer = std::make_unique<PolicyFetchClientObserver>(
          manager->core()->client(), run_loop.QuitClosure());
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
  if (dm_token() != kInvalidDMToken) {
    EXPECT_EQ(1u, policy_map.size());
    EXPECT_EQ(base::Value(true), *(policy_map.Get("ShowHomeButton")->value));
  } else {
    EXPECT_EQ(0u, policy_map.size());
  }
}

// The tests here cover three cases:
//  1) Start Chrome with a valid DM token but no policy cache. Chrome will
//  load the policy from the DM server.
//  2) Start Chrome with an invalid DM token. Chrome will hit the DM server and
//  get an error. There should be no more cloud policy applied.
//  3) Start Chrome without DM token. Chrome will register itself and fetch
//  policy after it.
INSTANTIATE_TEST_SUITE_P(,
                         MachineLevelUserCloudPolicyPolicyFetchTest,
                         ::testing::Values(kDMToken, kInvalidDMToken, ""));

}  // namespace policy
