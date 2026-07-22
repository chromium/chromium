// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"
#include "chrome/browser/enterprise/signals/signals_aggregator_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/enterprise/browser/reporting/chrome_profile_request_generator.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/report_uploader.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace enterprise_reporting {
namespace em = enterprise_management;

namespace {

constexpr char kFakeEnrollmentDomain[] = "fake.domain.google.com";
constexpr char kFakeAffiliationID[] = "fake_affiliation_id";
constexpr char kFakeSerialNumber[] = "fake_serial_number";
constexpr char kCookieHeaderName[] = "Cookie";
constexpr char kSetCookiePath[] = "/set-cookie";

struct CapturedProfileReportRequest {
  std::optional<em::DeviceManagementRequest> request;
  std::optional<std::string> cookie;
};

std::string CreateFakeSerializedAuthCookie(std::string_view cookie_value) {
  return base::StrCat(
      {cookie_value, ";secure;Domain=.google.com;max-age=1000"});
}

std::string GetSetCookiesPath(std::string_view cookie_value) {
  return base::StrCat(
      {kSetCookiePath, "?", CreateFakeSerializedAuthCookie(cookie_value)});
}

std::string CreateFakeAuthCookieValue() {
  return base::StrCat({GaiaConstants::kGaiaSigninCookieName, "=foo"});
}

}  // namespace

class SecurityReportingCrosBrowserTest
    : public policy::DevicePolicyCrosBrowserTest {
 public:
  SecurityReportingCrosBrowserTest() {
    device_state_.set_skip_initial_policy_setup(true);
  }
  ~SecurityReportingCrosBrowserTest() override = default;

  void SetUp() override {
    embedded_https_test_server().SetCertHostnames(
        {"m.google.com", "accounts.google.com", "google.com"});
    net::test_server::RegisterDefaultHandlers(&embedded_https_test_server());

    CHECK(embedded_https_test_server().InitializeAndListen());
    policy::DevicePolicyCrosBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        policy::switches::kDeviceManagementUrl,
        embedded_https_test_server()
            .GetURL("m.google.com", "/devicemanagement/data/api")
            .spec());
    policy::DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
        &SecurityReportingCrosBrowserTest::HandleRequest, base::Unretained(this)));

    embedded_https_test_server().StartAcceptingConnections();

    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    const auto& request_url = request.GetURL();
    if (request_url.path() != "/devicemanagement/data/api") {
      return nullptr;
    }

    std::string action_name;
    if (!net::GetValueForKeyInQuery(
            request_url, policy::dm_protocol::kParamRequest, &action_name)) {
      return nullptr;
    }

    if (action_name == policy::dm_protocol::kValueRequestPolicy) {
      return std::make_unique<net::test_server::BasicHttpResponse>();
    }

    if (action_name != policy::dm_protocol::kValueRequestChromeProfileReport) {
      return nullptr;
    }

    if (!pending_capture_.is_null()) {
      CapturedProfileReportRequest captured_request{};
      auto cookie_it = request.headers.find(kCookieHeaderName);
      if (cookie_it != request.headers.end()) {
        captured_request.cookie = cookie_it->second;
      }

      em::DeviceManagementRequest request_proto;
      if (request_proto.ParseFromString(request.content)) {
        captured_request.request = std::move(request_proto);
      }

      std::move(pending_capture_).Run(std::move(captured_request));
    }

    return std::make_unique<net::test_server::BasicHttpResponse>();
  }

 protected:
  Profile* profile() {
    return g_browser_process->profile_manager()->GetPrimaryUserProfile();
  }

  void SetupManagedDevice() {
    device_policy()->policy_data().set_managed_by(kFakeEnrollmentDomain);
    device_policy()->policy_data().add_device_affiliation_ids(
        kFakeAffiliationID);
    fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                  kFakeSerialNumber);
    fake_statistics_provider_.SetLoadingState(
        ash::system::StatisticsProvider::LoadingState::kFinished);
    policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();

    const user_manager::User* user =
        ash::ProfileHelper::Get()->GetUserByProfile(profile());
    user_manager::UserManager::Get()->SetUserPolicyStatus(
        user->GetAccountId(),
        /*is_managed=*/true,
        /*is_affiliated=*/true);
  }

  void SetFakeCookieValue(std::string_view cookie_value) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_https_test_server().GetURL("accounts.google.com",
                                            GetSetCookiesPath(cookie_value))));
  }

  base::OnceCallback<void(CapturedProfileReportRequest)> pending_capture_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

IN_PROC_BROWSER_TEST_F(SecurityReportingCrosBrowserTest,
                       GenerateReportPayload) {
  SetupManagedDevice();

  profile()->GetPrefs()->SetBoolean(kUserSecuritySignalsReporting, true);

  ReportingDelegateFactoryDesktop delegate_factory;
  ChromeProfileRequestGenerator generator(
      profile()->GetPath(), &delegate_factory,
      enterprise_signals::SignalsAggregatorFactory::GetForProfile(profile()));

  base::test::TestFuture<
      base::expected<ReportRequestQueue, ReportGenerationError>>
      future;
  generator.Generate(
      ReportGenerationConfig(ReportTrigger::kTriggerManual,
                             ReportType::kProfileReport,
                             SecuritySignalsMode::kSignalsAttached, false),
      future.GetCallback());

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  auto& requests = result.value();
  ASSERT_EQ(requests.size(), 1u);

  auto& profile_report_request =
      requests.front()->GetChromeProfileReportRequest();
  EXPECT_TRUE(profile_report_request.has_os_report());
  EXPECT_TRUE(profile_report_request.has_browser_report());
}

IN_PROC_BROWSER_TEST_F(SecurityReportingCrosBrowserTest, UploadWithAuth) {
  SetupManagedDevice();

  // 1. Navigate to accounts.google.com and set the cookie.
  SetFakeCookieValue(CreateFakeAuthCookieValue());

  // 2. Setup the test future to capture the request on the server.
  base::test::TestFuture<CapturedProfileReportRequest> test_future;
  pending_capture_ =
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         test_future.GetCallback());

  profile()->GetPrefs()->SetBoolean(kUserSecuritySignalsReporting, true);
  profile()->GetPrefs()->SetBoolean(kUserSecurityAuthenticatedReporting, true);

  // 3. Generate the report payload.
  ReportingDelegateFactoryDesktop delegate_factory;
  ChromeProfileRequestGenerator generator(
      profile()->GetPath(), &delegate_factory,
      enterprise_signals::SignalsAggregatorFactory::GetForProfile(profile()));

  base::test::TestFuture<
      base::expected<ReportRequestQueue, ReportGenerationError>>
      generate_future;
  generator.Generate(
      ReportGenerationConfig(ReportTrigger::kTriggerManual,
                             ReportType::kProfileReport,
                             SecuritySignalsMode::kSignalsAttached,
                             /*use_cookies=*/true),
      generate_future.GetCallback());

  auto result = generate_future.Take();
  ASSERT_TRUE(result.has_value());

  // 4. Manually trigger the upload via CloudPolicyClient since ReportScheduler
  // isn't created automatically in this testing environment.
  auto client = std::make_unique<policy::CloudPolicyClient>(
      /*machine_id=*/"",
      g_browser_process->browser_policy_connector()
          ->device_management_service(),
      profile()->GetURLLoaderFactory(),
      policy::CloudPolicyClient::DeviceDMTokenCallback());
  client->SetupRegistration("fake_dm_token", "fake_client_id", {});

  base::test::TestFuture<ReportUploader::ReportStatus> upload_future;
  ReportUploader uploader(client.get(), 1);
  uploader.SetRequestAndUpload(
      ReportGenerationConfig(ReportTrigger::kTriggerManual,
                             ReportType::kProfileReport,
                             SecuritySignalsMode::kSignalsAttached,
                             /*use_cookies=*/true),
      std::move(result.value()), upload_future.GetCallback());

  // 5. Verify the captured request on the server.
  auto captured = test_future.Get();
  ASSERT_TRUE(captured.request);
  EXPECT_EQ(captured.cookie, CreateFakeAuthCookieValue());
}

}  // namespace enterprise_reporting
