// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/client_certificates/certificate_provisioning_service_factory.h"
#include "chrome/browser/enterprise/test/management_context_mixin.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/platform_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
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

// Verifies the request's content and auth values.
// TODO(crbug.com/421384110): Add logic to verify the presence/value of signals.
void VerifyRequest(const CapturedProfileReportRequest& request,
                   em::ChromeProfileReportRequest::ReportType profile_type,
                   std::optional<std::string> cookie_value = std::nullopt) {
  ASSERT_TRUE(request.request);
  ASSERT_TRUE(request.request->has_chrome_profile_report_request());

  const auto& profile_report_request =
      request.request->chrome_profile_report_request();
  EXPECT_EQ(profile_report_request.report_type(), profile_type);

  EXPECT_EQ(request.cookie, cookie_value);
}

}  // namespace

class SecurityReportingBrowserTest : public MixinBasedInProcessBrowserTest {
 protected:
  SecurityReportingBrowserTest() {
    management_mixin_ =
        ManagementContextMixin::Create(&mixin_host_, this,
                                       {
                                           .is_cloud_user_managed = true,
                                           .is_cloud_machine_managed = false,
                                           .affiliated = false,
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

  ManagementContextMixin* management_mixin() { return management_mixin_.get(); }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  base::OnceCallback<void(CapturedProfileReportRequest)> pending_capture_;

 private:
  void SetFakeCookieValue(std::string_view cookie_value) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_https_test_server().GetURL("accounts.google.com",
                                            GetSetCookiesPath(cookie_value))));
  }

  base::HistogramTester histogram_tester_;
  std::unique_ptr<ManagementContextMixin> management_mixin_;
};

// Tests that a security-only report is sent when only the security reports user
// policy is enabled. It should also not include the cookie, as the
// authenticated reporting policy is not set.
IN_PROC_BROWSER_TEST_F(SecurityReportingBrowserTest, SecurityReportOnly) {
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

// Tests that a security-only report is sent when only the security reports user
// policy is enabled. It should include the cookie, as the authenticated
// reporting policy is enabled. Updating the cookie again should also trigger
// another report.
IN_PROC_BROWSER_TEST_F(SecurityReportingBrowserTest,
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

  // Commented-out, as the cookies are not set for "google.com", so our trigger
  // never kicks-in. Verify that another request will be uploaded if the auth
  // cookie changes.
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
// policy is enabled.
IN_PROC_BROWSER_TEST_F(SecurityReportingBrowserTest, OnlyProfileReport) {
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

// Tests that a standard Profile report is sent when only its user
// policy is enabled. Also sets the authentication policy, but the
// cookie should not be forwarded, as that policy only works with
// reports containing security information.
IN_PROC_BROWSER_TEST_F(SecurityReportingBrowserTest,
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

// Tests that a combined Profile report is sent when all user
// policies are enabled, with cookies.
IN_PROC_BROWSER_TEST_F(SecurityReportingBrowserTest, CombineReportWithAuth) {
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

}  // namespace enterprise_reporting
