// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/client_certificates/certificate_provisioning_service_factory.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/enterprise/test/management_context_mixin.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/platform_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/device_signals/core/browser/browser_utils.h"
#include "components/device_signals/core/common/platform_utils.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/browser/reporting/report_util.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ManagementContextMixin = enterprise::test::ManagementContextMixin;
using ManagementContext = enterprise::test::ManagementContext;

namespace em = enterprise_management;

namespace enterprise_reporting {

namespace {

const char* kCookieHeaderName = "Cookie";
const char* kSetCookiePath = "/set-cookie";

struct CapturedProfileReportRequest {
  std::optional<em::DeviceManagementRequest> request{std::nullopt};
  std::optional<std::string> cookie{std::nullopt};
};

std::string CreateFakeAuthCookieValue() {
  // Set the SAPISID cookie.
  return base::StrCat({GaiaConstants::kGaiaSigninCookieName, "=foo"});
}

std::string CreateOtherFakeAuthCookieValue() {
  // Set the SAPISID cookie.
  return base::StrCat({GaiaConstants::kGaiaSigninCookieName, "=bar"});
}

std::string CreateFakeSerializedAuthCookie(std::string_view cookie_value) {
  // Make sure there are no spaces in this string, as the URL encoding may
  // drop some of the cookie parameters.
  return base::StrCat(
      {cookie_value, ";secure;Domain=.google.com;max-age=1000"});
}

std::string GetSetCookiesPath(std::string_view cookie_value) {
  return base::StrCat(
      {kSetCookiePath, "?", CreateFakeSerializedAuthCookie(cookie_value)});
}

// Helper function to deal with when a signal with std::nullopt gets converted
// into an empty string in the report.
void CheckReportMatchSignal(std::string report_value,
                            std::optional<std::string> signal_value) {
  if (signal_value == std::nullopt) {
    ASSERT_TRUE(report_value.empty());
    return;
  }

  EXPECT_EQ(signal_value.value(), report_value);
}

}  // namespace

class SecurityReportingBrowserTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<testing::tuple<bool, bool>> {
 protected:
  SecurityReportingBrowserTest() {
    management_mixin_ = ManagementContextMixin::Create(
        &mixin_host_, this,
        {
            .is_cloud_user_managed = true,
            .is_cloud_machine_managed = is_device_managed(),
            .affiliated = is_affiliated(),
        });
  }

  void SetUp() override {
    embedded_https_test_server().SetCertHostnames(
        {"m.google.com", "accounts.google.com", "google.com"});
    net::test_server::RegisterDefaultHandlers(&embedded_https_test_server());

    CHECK(embedded_https_test_server().InitializeAndListen());
    MixinBasedInProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        policy::switches::kDeviceManagementUrl,
        embedded_https_test_server()
            .GetURL("m.google.com", "/devicemanagement/data/api")
            .spec());
  }

  void SetUpInProcessBrowserTestFixture() override {
    policy::ChromeBrowserPolicyConnector::EnableCommandLineSupportForTesting();
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
        &SecurityReportingBrowserTest::HandleRequest, base::Unretained(this)));

    embedded_https_test_server().StartAcceptingConnections();

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    const auto& request_url = request.GetURL();
    if (request_url.path_piece() != "/devicemanagement/data/api") {
      SCOPED_TRACE("Not a DM server request.");
      return nullptr;
    }

    std::string action_name;
    if (!net::GetValueForKeyInQuery(
            request_url, policy::dm_protocol::kParamRequest, &action_name)) {
      SCOPED_TRACE("No action name.");
      return nullptr;
    }

    if (action_name != policy::dm_protocol::kValueRequestChromeProfileReport) {
      SCOPED_TRACE("Not a Profile report.");
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

  void SetFakeCookieValue() { SetFakeCookieValue(CreateFakeAuthCookieValue()); }

  void SetOtherFakeCookieValue() {
    SetFakeCookieValue(CreateOtherFakeAuthCookieValue());
  }

  // Verifies the request's content and auth values.
  void VerifyRequest(const CapturedProfileReportRequest& request,
                     em::ChromeProfileReportRequest::ReportType profile_type,
                     std::optional<std::string> cookie_value = std::nullopt) {
    ASSERT_TRUE(request.request);
    ASSERT_TRUE(request.request->has_chrome_profile_report_request());
    EXPECT_EQ(request.cookie, cookie_value);

    const auto& profile_report_request =
        request.request->chrome_profile_report_request();
    EXPECT_EQ(profile_report_request.report_type(), profile_type);

    bool expect_signals_override_value =
        profile_type != em::ChromeProfileReportRequest::PROFILE_REPORT;

    EXPECT_EQ(profile_report_request.has_browser_device_identifier(),
              expect_signals_override_value);

    if (expect_signals_override_value) {
      auto browser_device_identifier =
          profile_report_request.browser_device_identifier();
      EXPECT_EQ(
          browser_device_identifier.computer_name(),
          can_collect_pii_signals() ? policy::GetDeviceName() : std::string());
      EXPECT_EQ(browser_device_identifier.host_name(),
                can_collect_pii_signals() ? device_signals::GetHostName()
                                          : std::string());
      base::RunLoop run_loop;

      base::ThreadPool::PostTask(
          FROM_HERE, {base::MayBlock()},
          base::BindOnce(
              &SecurityReportingBrowserTest::VerifyDeviceIdentifierAsyncSignal,
              base::Unretained(this), std::ref(browser_device_identifier),
              run_loop.QuitClosure()));
      run_loop.Run();
    }

    ASSERT_TRUE(profile_report_request.has_os_report());
    VerifyOsReport(profile_report_request.os_report(),
                   expect_signals_override_value);

    ASSERT_TRUE(profile_report_request.has_browser_report());
    auto browser_report = profile_report_request.browser_report();
    EXPECT_EQ(browser_report.browser_version(),
              version_info::GetVersionNumber());

    EXPECT_EQ(1, browser_report.chrome_user_profile_infos_size());
    auto chrome_user_profile_info = browser_report.chrome_user_profile_infos(0);

    // `profile_signals_report` is a signals report only sub-proto.
    EXPECT_EQ(chrome_user_profile_info.has_profile_signals_report(),
              expect_signals_override_value);

    if (!expect_signals_override_value) {
      return;
    }

    VerifyProfileSignalsReport(
        chrome_user_profile_info.profile_signals_report());

    ASSERT_FALSE(chrome_user_profile_info.profile_id().empty());
    EXPECT_EQ(
        chrome_user_profile_info.profile_id(),
        enterprise::ProfileIdServiceFactory::GetForProfile(browser()->profile())
            ->GetProfileId()
            .value());
  }

  void VerifyDeviceIdentifierAsyncSignal(
      em::BrowserDeviceIdentifier& browser_device_identifier,
      base::OnceCallback<void()> done_closure) {
    EXPECT_EQ(browser_device_identifier.serial_number(),
              can_collect_pii_signals() ? device_signals::GetSerialNumber()
                                        : std::string());
    std::move(done_closure).Run();
  }

  void VerifyOsReport(const em::OSReport& os_report,
                      bool expect_signals_override_value) {
    EXPECT_EQ(os_report.name(), policy::GetOSPlatform());
    EXPECT_EQ(os_report.arch(), policy::GetOSArchitecture());
    if (expect_signals_override_value) {
      EXPECT_EQ(os_report.version(), device_signals::GetOsVersion());
      EXPECT_EQ(os_report.screen_lock_secured(),
                TranslateSettingValue(device_signals::GetScreenlockSecured()));
#if BUILDFLAG(IS_WIN)
      EXPECT_EQ(os_report.secure_boot_mode(),
                TranslateSettingValue(device_signals::GetSecureBootEnabled()));
      CheckReportMatchSignal(os_report.windows_machine_domain(),
                             device_signals::GetWindowsMachineDomain());
      CheckReportMatchSignal(os_report.windows_user_domain(),
                             device_signals::GetWindowsUserDomain());
      CheckReportMatchSignal(os_report.machine_guid(),
                             can_collect_pii_signals()
                                 ? device_signals::GetMachineGuid()
                                 : std::nullopt);
#endif  // BUILDFLAG(IS_WIN)
      base::RunLoop run_loop;
      base::ThreadPool::PostTask(
          FROM_HERE, {base::MayBlock()},
          base::BindOnce(
              &SecurityReportingBrowserTest::VerifyOsReportAsyncSignal,
              base::Unretained(this), os_report, run_loop.QuitClosure()));
      run_loop.Run();
    } else {
      EXPECT_EQ(os_report.version(), policy::GetOSVersion());

      // Signals report only fields should not be written
      ASSERT_FALSE(os_report.has_device_enrollment_domain());
      ASSERT_FALSE(os_report.has_screen_lock_secured());

      EXPECT_EQ(0, os_report.mac_addresses_size());
#if BUILDFLAG(IS_WIN)
      EXPECT_EQ(0, os_report.antivirus_info_size());
      EXPECT_EQ(0, os_report.hotfixes_size());
#endif  // BUILDFLAG(IS_WIN)
    }
  }

  void VerifyOsReportAsyncSignal(const em::OSReport& os_report,
                                 base::OnceCallback<void()> done_closure) {
    EXPECT_EQ(os_report.disk_encryption(),
              TranslateSettingValue(device_signals::GetDiskEncrypted()));
    EXPECT_EQ(os_report.os_firewall(),
              TranslateSettingValue(device_signals::GetOSFirewall()));
    std::move(done_closure).Run();
  }

  void VerifyProfileSignalsReport(
      const em::ProfileSignalsReport& profile_signals_report) {
    EXPECT_EQ(profile_signals_report.built_in_dns_client_enabled(),
              g_browser_process->local_state()->GetBoolean(
                  prefs::kBuiltInDnsClientEnabled));
    EXPECT_EQ(profile_signals_report.chrome_remote_desktop_app_blocked(),
              device_signals::GetChromeRemoteDesktopAppBlocked(
                  PolicyBlocklistFactory::GetForBrowserContext(
                      browser()->profile())));
    EXPECT_EQ(profile_signals_report.password_protection_warning_trigger(),
              TranslatePasswordProtectionTrigger(
                  device_signals::GetPasswordProtectionWarningTrigger(
                      browser()->profile()->GetPrefs())));
    CheckReportMatchSignal(profile_signals_report.profile_enrollment_domain(),
                           device_signals::TryGetEnrollmentDomain(
                               browser()->profile()->GetCloudPolicyManager()));
    EXPECT_EQ(profile_signals_report.safe_browsing_protection_level(),
              TranslateSafeBrowsingLevel(
                  device_signals::GetSafeBrowsingProtectionLevel(
                      browser()->profile()->GetPrefs())));
    EXPECT_EQ(profile_signals_report.site_isolation_enabled(),
              device_signals::GetSiteIsolationEnabled());
  }

  bool is_device_managed() { return testing::get<0>(GetParam()); }
  bool is_affiliated() { return testing::get<1>(GetParam()); }

  bool can_collect_pii_signals() {
    return is_device_managed() && is_affiliated();
  }

  ManagementContextMixin* management_mixin() { return management_mixin_.get(); }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  base::OnceCallback<void(CapturedProfileReportRequest)> pending_capture_;

  void SetFakeCookieValue(std::string_view cookie_value) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_https_test_server().GetURL("accounts.google.com",
                                            GetSetCookiesPath(cookie_value))));
  }

 private:
  base::HistogramTester histogram_tester_;
  std::unique_ptr<ManagementContextMixin> management_mixin_;
};

// Tests that a security-only report is sent when only the security reports
// user policy is enabled. It should also not include the cookie, as the
// authenticated reporting policy is not set.
IN_PROC_BROWSER_TEST_P(SecurityReportingBrowserTest, SecurityReportOnly) {
  SetFakeCookieValue();

  base::test::TestFuture<CapturedProfileReportRequest> test_future;
  pending_capture_ =
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         test_future.GetCallback());

  base::flat_map<std::string, std::optional<base::Value>> policy_values;
  policy_values.insert(
      {policy::key::kUserSecuritySignalsReporting, base::Value(true)});
  management_mixin()->SetCloudUserPolicies(std::move(policy_values));

  VerifyRequest(test_future.Get(),
                em::ChromeProfileReportRequest::PROFILE_SECURITY_SIGNALS);
}

// Tests that a combined Profile report is sent when all user
// policies are enabled, with cookies.
IN_PROC_BROWSER_TEST_P(SecurityReportingBrowserTest, CombineReportWithAuth) {
  SetFakeCookieValue();

  base::test::TestFuture<CapturedProfileReportRequest> test_future;
  pending_capture_ =
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         test_future.GetCallback());

  base::flat_map<std::string, std::optional<base::Value>> policy_values;
  policy_values.insert(
      {policy::key::kCloudProfileReportingEnabled, base::Value(true)});
  policy_values.insert(
      {policy::key::kUserSecuritySignalsReporting, base::Value(true)});
  policy_values.insert(
      {policy::key::kUserSecurityAuthenticatedReporting, base::Value(true)});
  management_mixin()->SetCloudUserPolicies(std::move(policy_values));

  VerifyRequest(
      test_future.Get(),
      em::ChromeProfileReportRequest::PROFILE_REPORT_WITH_SECURITY_SIGNALS,
      CreateFakeAuthCookieValue());
}

// Tests that a standard Profile report is sent when only its user
// policy is enabled.
IN_PROC_BROWSER_TEST_P(SecurityReportingBrowserTest, OnlyProfileReport) {
  SetFakeCookieValue();

  base::test::TestFuture<CapturedProfileReportRequest> test_future;
  pending_capture_ =
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         test_future.GetCallback());

  base::flat_map<std::string, std::optional<base::Value>> policy_values;
  policy_values.insert(
      {policy::key::kCloudProfileReportingEnabled, base::Value(true)});
  management_mixin()->SetCloudUserPolicies(std::move(policy_values));

  VerifyRequest(test_future.Get(),
                em::ChromeProfileReportRequest::PROFILE_REPORT);
}

INSTANTIATE_TEST_SUITE_P(
    ManagedDeviceCase,
    SecurityReportingBrowserTest,
    testing::Combine(/*is_device_managed=*/testing::Values(true),
                     /*is_affiliated=*/testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    UnmanagedDeviceCase,
    SecurityReportingBrowserTest,
    testing::Combine(/*is_device_managed=*/testing::Values(false),
                     /*is_affiliated=*/testing::Values(false)));

// Test that confirms the correct form of reports are being triggered.
// Collection contexts such as management state don't affect the expectations so
// we don't need to covered them with redundant test cases.
class SecurityReportTriggerBrowserTest : public SecurityReportingBrowserTest {
 protected:
  SecurityReportTriggerBrowserTest() : SecurityReportingBrowserTest() {}
};

// Tests that a security-only report is sent when only the security reports
// user policy is enabled. It should include the cookie, as the authenticated
// reporting policy is enabled. Updating the cookie again should also trigger
// another report.
IN_PROC_BROWSER_TEST_P(SecurityReportTriggerBrowserTest,
                       SecurityReportWithAuth_Reauth) {
  SetFakeCookieValue();

  base::test::TestFuture<CapturedProfileReportRequest> test_future;
  pending_capture_ =
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         test_future.GetCallback());

  base::flat_map<std::string, std::optional<base::Value>> policy_values;
  policy_values.insert(
      {policy::key::kUserSecuritySignalsReporting, base::Value(true)});
  policy_values.insert(
      {policy::key::kUserSecurityAuthenticatedReporting, base::Value(true)});
  management_mixin()->SetCloudUserPolicies(std::move(policy_values));

  VerifyRequest(test_future.Get(),
                em::ChromeProfileReportRequest::PROFILE_SECURITY_SIGNALS,
                CreateFakeAuthCookieValue());

  // Commented-out, as the cookies are not set for "google.com", so our
  // trigger never kicks-in. Verify that another request will be uploaded if
  // the auth cookie changes.
  test_future.Clear();

  pending_capture_ =
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         test_future.GetCallback());

  SetOtherFakeCookieValue();

  VerifyRequest(test_future.Get(),
                em::ChromeProfileReportRequest::PROFILE_SECURITY_SIGNALS,
                CreateOtherFakeAuthCookieValue());
}

// Tests that a standard Profile report is sent when only its user
// policy is enabled. Also sets the authentication policy, but the
// cookie should not be forwarded, as that policy only works with
// reports containing security information.
IN_PROC_BROWSER_TEST_P(SecurityReportTriggerBrowserTest,
                       OnlyProfileReportWithAuth_NoCookie) {
  SetFakeCookieValue();

  base::test::TestFuture<CapturedProfileReportRequest> test_future;
  pending_capture_ =
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         test_future.GetCallback());

  base::flat_map<std::string, std::optional<base::Value>> policy_values;
  policy_values.insert(
      {policy::key::kCloudProfileReportingEnabled, base::Value(true)});
  policy_values.insert(
      {policy::key::kUserSecurityAuthenticatedReporting, base::Value(true)});
  management_mixin()->SetCloudUserPolicies(std::move(policy_values));

  VerifyRequest(test_future.Get(),
                em::ChromeProfileReportRequest::PROFILE_REPORT);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SecurityReportTriggerBrowserTest,
    testing::Combine(/*is_device_managed*/ ::testing::Values(false),
                     /*is_affiliated*/ ::testing::Values(false)));

}  // namespace enterprise_reporting
