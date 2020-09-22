// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/attestation/tpm_challenge_key.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/enrollment_ui_mixin.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/https_forwarder.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/test_users.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/affiliation_test_helper.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/login/login_handler_test_utils.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/saml_challenge_key_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/features/feature_channel.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace em = enterprise_management;

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::WithArgs;

namespace chromeos {

namespace {

const test::UIPath kPasswordInput = {"saml-confirm-password", "passwordInput"};
const test::UIPath kPasswordConfirmInput = {"saml-confirm-password",
                                            "confirmPasswordInput"};
const test::UIPath kPasswordSubmit = {"saml-confirm-password", "next"};

constexpr char kGAIASIDCookieName[] = "SID";
constexpr char kGAIALSIDCookieName[] = "LSID";

constexpr char kTestAuthSIDCookie1[] = "fake-auth-SID-cookie-1";
constexpr char kTestAuthSIDCookie2[] = "fake-auth-SID-cookie-2";
constexpr char kTestAuthLSIDCookie1[] = "fake-auth-LSID-cookie-1";
constexpr char kTestAuthLSIDCookie2[] = "fake-auth-LSID-cookie-2";

constexpr char kNonSAMLUserEmail[] = "frank@corp.example.com";

constexpr char kFirstSAMLUserGaiaId[] = "alice-gaia";
constexpr char kSecondSAMLUserGaiaId[] = "bob-gaia";
constexpr char kThirdSAMLUserGaiaId[] = "carol-gaia";
constexpr char kFifthSAMLUserGaiaId[] = "eve-gaia";
constexpr char kNonSAMLUserGaiaId[] = "frank-gaia";

constexpr char kIdPHost[] = "login.corp.example.com";
constexpr char kAdditionalIdPHost[] = "login2.corp.example.com";

// The header that the server returns in a HTTP response to ask the client to
// authenticate.
constexpr char kAuthenticateResponseHeader[] = "WWW-Authenticate";

// The response header that the client sends to transfer HTTP auth credentials.
constexpr char kAuthorizationRequestHeader[] = "Authorization";

constexpr char kSAMLIdPCookieName[] = "saml";
constexpr char kSAMLIdPCookieValue1[] = "value-1";
constexpr char kSAMLIdPCookieValue2[] = "value-2";

constexpr char kRelayState[] = "RelayState";

constexpr char kTestUserinfoToken[] = "fake-userinfo-token";
constexpr char kTestRefreshToken[] = "fake-refresh-token";

constexpr char kAffiliationID[] = "some-affiliation-id";

constexpr char kSamlLoginPath[] = "SAML";
constexpr char kSamlLoginWithDeviceAttestationPath[] =
    "SAML-with-device-attestation";
constexpr char kSamlLoginCheckDeviceAnswerPath[] = "SAML-check-device-answer";

// Must be equal to SAML_VERIFIED_ACCESS_CHALLENGE_HEADER from saml_handler.js.
constexpr char kSamlVerifiedAccessChallengeHeader[] =
    "x-verified-access-challenge";
// Must be equal to SAML_VERIFIED_ACCESS_RESPONSE_HEADER from saml_handler.js.
constexpr char kSamlVerifiedAccessResponseHeader[] =
    "x-verified-access-challenge-response";

constexpr char kTpmChallenge[] = {0, 1, 2, 'c', 'h', 'a', 'l', 253, 254, 255};
constexpr char kTpmChallengeResponse[] = {0,   1,   2,   'r', 'e',
                                          's', 'p', 253, 254, 255};

std::string GetTpmChallenge() {
  return std::string(kTpmChallenge, base::size(kTpmChallenge));
}

std::string GetTpmResponse() {
  return std::string(kTpmChallengeResponse, base::size(kTpmChallengeResponse));
}

std::string GetTpmChallengeBase64() {
  return base::Base64Encode(
      base::as_bytes(base::span<const char>(kTpmChallenge)));
}

std::string GetTpmResponseBase64() {
  return base::Base64Encode(
      base::as_bytes(base::span<const char>(kTpmChallengeResponse)));
}

// Returns relay state from http get/post requests.
std::string GetRelayState(const HttpRequest& request) {
  std::string relay_state;

  if (request.method == net::test_server::HttpMethod::METHOD_GET) {
    EXPECT_TRUE(net::GetValueForKeyInQuery(request.GetURL(), kRelayState,
                                           &relay_state));
  } else if (request.method == net::test_server::HttpMethod::METHOD_POST) {
    GURL query_url("http://localhost?" + request.content);
    EXPECT_TRUE(
        net::GetValueForKeyInQuery(query_url, kRelayState, &relay_state));
  } else {
    EXPECT_TRUE(false);  // gtest friendly implementation of NOTREACHED().
  }

  return relay_state;
}

// FakeSamlIdp serves IdP auth form and the form submission. The form is
// served with the template's RelayState placeholder expanded to the real
// RelayState parameter from request. The form submission redirects back to
// FakeGaia with the same RelayState.
class FakeSamlIdp {
 public:
  FakeSamlIdp();
  ~FakeSamlIdp();

  void SetUp(const GURL& base_login_url,
             const GURL& device_attest_url,
             const GURL& check_device_answer_url,
             const GURL& gaia_url);

  void SetLoginHTMLTemplate(const std::string& template_file);
  void SetLoginAuthHTMLTemplate(const std::string& template_file);
  void SetRefreshURL(const GURL& refresh_url);
  void SetCookieValue(const std::string& cookie_value);
  void SetRequireHttpBasicAuth(bool require_http_basic_auth);
  void SetSamlResponseFile(const std::string& xml_file);
  bool IsLastChallengeResponseExists() const;
  const std::string& GetLastChallengeResponse() const;

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

 private:
  // Enumerates requests that this FakeSamlIdp may get.
  enum class RequestType {
    // Not a known request.
    kUnknown,
    kLogin,
    kLoginAuth,
    kLoginWithDeviceAttestation,
    kLoginCheckDeviceAnswer
  };

  // Returns the RequestType that corresponds to |url|, or RequestType::Unknown
  // if this is not a request for the FakeSamlIdp.
  RequestType ParseRequestTypeFromRequestPath(const GURL& request_url) const;

  std::unique_ptr<HttpResponse> BuildResponseForLogin(
      const HttpRequest& request,
      const GURL& request_url);
  std::unique_ptr<HttpResponse> BuildResponseForLoginAuth(
      const HttpRequest& request,
      const GURL& request_url);
  std::unique_ptr<HttpResponse> BuildResponseForLoginWithDeviceAttestation(
      const HttpRequest& request,
      const GURL& request_url);
  std::unique_ptr<HttpResponse> BuildResponseForCheckDeviceAnswer(
      const HttpRequest& request,
      const GURL& request_url);

  std::unique_ptr<HttpResponse> BuildHTMLResponse(
      const std::string& html_template,
      const std::string& relay_state,
      const std::string& next_path);

  void SaveChallengeResponse(const std::string& response);
  void ClearChallengeResponse();

  base::FilePath html_template_dir_;
  base::FilePath saml_response_dir_;

  GURL login_url_;
  std::string login_auth_path_;
  GURL login_with_device_attest_url_;
  GURL login_check_device_answer_url_;

  std::string login_html_template_;
  std::string login_auth_html_template_;
  GURL gaia_assertion_url_;
  GURL refresh_url_;
  std::string cookie_value_;
  std::string saml_response_{"fake_response"};

  bool require_http_basic_auth_ = false;

  base::Optional<std::string> challenge_response_;

  DISALLOW_COPY_AND_ASSIGN(FakeSamlIdp);
};

FakeSamlIdp::FakeSamlIdp() {}

FakeSamlIdp::~FakeSamlIdp() {}

void FakeSamlIdp::SetUp(const GURL& base_login_url,
                        const GURL& device_attest_url,
                        const GURL& check_device_answer_url,
                        const GURL& gaia_url) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  // NOTE: Ideally testdata would all be in chromeos/login, to match the test.
  html_template_dir_ = test_data_dir.Append("login");
  saml_response_dir_ = test_data_dir.Append("chromeos").Append("login");

  login_url_ = base_login_url;
  login_auth_path_ = login_url_.path() + "Auth";

  login_with_device_attest_url_ = device_attest_url;
  login_check_device_answer_url_ = check_device_answer_url;

  gaia_assertion_url_ = gaia_url.Resolve("/SSO");
}

void FakeSamlIdp::SetLoginHTMLTemplate(const std::string& template_file) {
  base::ScopedAllowBlockingForTesting allow_io;
  EXPECT_TRUE(base::ReadFileToString(html_template_dir_.Append(template_file),
                                     &login_html_template_));
}

void FakeSamlIdp::SetLoginAuthHTMLTemplate(const std::string& template_file) {
  base::ScopedAllowBlockingForTesting allow_io;
  EXPECT_TRUE(base::ReadFileToString(html_template_dir_.Append(template_file),
                                     &login_auth_html_template_));
}

void FakeSamlIdp::SetSamlResponseFile(const std::string& xml_file) {
  base::ScopedAllowBlockingForTesting allow_io;
  EXPECT_TRUE(base::ReadFileToString(saml_response_dir_.Append(xml_file),
                                     &saml_response_));
  base::Base64Encode(saml_response_, &saml_response_);
}

void FakeSamlIdp::SetRefreshURL(const GURL& refresh_url) {
  refresh_url_ = refresh_url;
}

void FakeSamlIdp::SetCookieValue(const std::string& cookie_value) {
  cookie_value_ = cookie_value;
}

void FakeSamlIdp::SetRequireHttpBasicAuth(bool require_http_basic_auth) {
  require_http_basic_auth_ = require_http_basic_auth;
}

std::unique_ptr<net::test_server::HttpResponse> FakeSamlIdp::HandleRequest(
    const net::test_server::HttpRequest& request) {
  // The scheme and host of the URL is actually not important but required to
  // get a valid GURL in order to parse |request.relative_url|.
  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
  const RequestType request_type = ParseRequestTypeFromRequestPath(request_url);

  if (request_type == RequestType::kUnknown) {
    // Ignore this request. Note that another handler may still care.
    LOG(INFO) << "Request is ignored by FakeSamlIdp: " << request.GetURL();
    return std::unique_ptr<HttpResponse>();
  }

  // For HTTP Basic Auth, we don't care to check the credentials, just
  // if some credentials were provided. If not, respond with an authentication
  // request that should make the browser pop up a credentials entry UI.
  if (require_http_basic_auth_ &&
      !base::Contains(request.headers, kAuthorizationRequestHeader)) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_UNAUTHORIZED);
    http_response->AddCustomHeader(kAuthenticateResponseHeader,
                                   "Basic realm=\"test realm\"");
    return http_response;
  }

  switch (request_type) {
    case RequestType::kLogin:
      return BuildResponseForLogin(request, request_url);
    case RequestType::kLoginAuth:
      return BuildResponseForLoginAuth(request, request_url);
    case RequestType::kLoginWithDeviceAttestation:
      return BuildResponseForLoginWithDeviceAttestation(request, request_url);
    case RequestType::kLoginCheckDeviceAnswer:
      return BuildResponseForCheckDeviceAnswer(request, request_url);
    case RequestType::kUnknown:
      NOTREACHED();
      return std::unique_ptr<HttpResponse>();
  }
}

FakeSamlIdp::RequestType FakeSamlIdp::ParseRequestTypeFromRequestPath(
    const GURL& request_url) const {
  std::string request_path = request_url.path();

  if (request_path == login_url_.path())
    return RequestType::kLogin;
  if (request_path == login_auth_path_)
    return RequestType::kLoginAuth;
  if (request_path == login_with_device_attest_url_.path())
    return RequestType::kLoginWithDeviceAttestation;
  if (request_path == login_check_device_answer_url_.path())
    return RequestType::kLoginCheckDeviceAnswer;

  return RequestType::kUnknown;
}

std::unique_ptr<HttpResponse> FakeSamlIdp::BuildResponseForLogin(
    const HttpRequest& request,
    const GURL& request_url) {
  const std::string relay_state = GetRelayState(request);
  return BuildHTMLResponse(login_html_template_, relay_state, login_auth_path_);
}

std::unique_ptr<HttpResponse> FakeSamlIdp::BuildResponseForLoginAuth(
    const HttpRequest& request,
    const GURL& request_url) {
  const std::string relay_state = GetRelayState(request);
  GURL redirect_url = gaia_assertion_url_;

  if (!login_auth_html_template_.empty()) {
    return BuildHTMLResponse(login_auth_html_template_, relay_state,
                             redirect_url.spec());
  }

  redirect_url =
      net::AppendQueryParameter(redirect_url, "SAMLResponse", saml_response_);
  redirect_url =
      net::AppendQueryParameter(redirect_url, kRelayState, relay_state);

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url.spec());
  http_response->AddCustomHeader(
      "Set-cookie", base::StringPrintf("saml=%s", cookie_value_.c_str()));
  return http_response;
}

std::unique_ptr<HttpResponse>
FakeSamlIdp::BuildResponseForLoginWithDeviceAttestation(
    const HttpRequest& request,
    const GURL& request_url) {
  std::string relay_state = GetRelayState(request);

  GURL redirect_url = login_check_device_answer_url_;
  redirect_url =
      net::AppendQueryParameter(redirect_url, kRelayState, relay_state);

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url.spec());
  http_response->AddCustomHeader(kSamlVerifiedAccessChallengeHeader,
                                 GetTpmChallengeBase64());

  return std::move(http_response);
}

std::unique_ptr<HttpResponse> FakeSamlIdp::BuildResponseForCheckDeviceAnswer(
    const HttpRequest& request,
    const GURL& request_url) {
  std::string relay_state = GetRelayState(request);

  auto iter = request.headers.find(kSamlVerifiedAccessResponseHeader);
  if (iter != request.headers.end()) {
    SaveChallengeResponse(/*challenge_response=*/iter->second);
  } else {
    ClearChallengeResponse();
  }

  GURL redirect_url =
      net::AppendQueryParameter(login_url_, kRelayState, relay_state);

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url.spec());
  return std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse> FakeSamlIdp::BuildHTMLResponse(
    const std::string& html_template,
    const std::string& relay_state,
    const std::string& next_path) {
  std::string response_html = html_template;
  base::ReplaceSubstringsAfterOffset(&response_html, 0, "$RelayState",
                                     relay_state);
  base::ReplaceSubstringsAfterOffset(&response_html, 0, "$Post", next_path);
  base::ReplaceSubstringsAfterOffset(&response_html, 0, "$Refresh",
                                     refresh_url_.spec());

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(response_html);
  http_response->set_content_type("text/html");

  return std::move(http_response);
}

void FakeSamlIdp::SaveChallengeResponse(const std::string& response) {
  EXPECT_TRUE(!challenge_response_);
  challenge_response_ = response;
}

void FakeSamlIdp::ClearChallengeResponse() {
  challenge_response_.reset();
}

bool FakeSamlIdp::IsLastChallengeResponseExists() const {
  return challenge_response_.has_value();
}

const std::string& FakeSamlIdp::GetLastChallengeResponse() const {
  return challenge_response_.value();
}

// A FakeCryptohomeClient that stores the salted and hashed secret passed to
// MountEx().
class SecretInterceptingFakeCryptohomeClient : public FakeCryptohomeClient {
 public:
  SecretInterceptingFakeCryptohomeClient();

  void MountEx(const cryptohome::AccountIdentifier& id,
               const cryptohome::AuthorizationRequest& auth,
               const cryptohome::MountRequest& request,
               DBusMethodCallback<cryptohome::BaseReply> callback) override;

  const std::string& salted_hashed_secret() { return salted_hashed_secret_; }

 private:
  std::string salted_hashed_secret_;

  DISALLOW_COPY_AND_ASSIGN(SecretInterceptingFakeCryptohomeClient);
};

SecretInterceptingFakeCryptohomeClient::
    SecretInterceptingFakeCryptohomeClient() {}

void SecretInterceptingFakeCryptohomeClient::MountEx(
    const cryptohome::AccountIdentifier& id,
    const cryptohome::AuthorizationRequest& auth,
    const cryptohome::MountRequest& request,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  salted_hashed_secret_ = auth.key().secret();
  FakeCryptohomeClient::MountEx(id, auth, request, std::move(callback));
}

}  // namespace

class SamlTest : public OobeBaseTest {
 public:
  SamlTest() {
    // TODO(crbug.com/1121910): Fix tests.
    feature_list_.InitAndDisableFeature(
        chromeos::features::kChildSpecificSignin);
    fake_gaia_.set_initialize_fake_merge_session(false);
  }
  ~SamlTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
    command_line->AppendSwitch(
        chromeos::switches::kAllowFailedPolicyFetchForTest);

    ASSERT_TRUE(saml_https_forwarder_.Initialize(
        kIdPHost, embedded_test_server()->base_url()));

    const GURL gaia_url =
        fake_gaia_.gaia_https_forwarder()->GetURLForSSLHost("");

    const GURL base_login_saml_idp_url =
        saml_https_forwarder_.GetURLForSSLHost(kSamlLoginPath);

    const GURL device_attest_saml_idp_url =
        saml_https_forwarder_.GetURLForSSLHost(
            kSamlLoginWithDeviceAttestationPath);

    const GURL device_attest_check_response_saml_idp_url =
        saml_https_forwarder_.GetURLForSSLHost(kSamlLoginCheckDeviceAnswerPath);

    const GURL http_saml_idp_url =
        embedded_test_server()->base_url().Resolve(kSamlLoginPath);

    fake_saml_idp_.SetUp(base_login_saml_idp_url, device_attest_saml_idp_url,
                         device_attest_check_response_saml_idp_url, gaia_url);
    fake_gaia_.fake_gaia()->RegisterSamlUser(
        saml_test_users::kFirstUserCorpExampleComEmail,
        base_login_saml_idp_url);
    fake_gaia_.fake_gaia()->RegisterSamlUser(
        saml_test_users::kSecondUserCorpExampleComEmail,
        base_login_saml_idp_url);
    fake_gaia_.fake_gaia()->RegisterSamlUser(
        saml_test_users::kThirdUserCorpExampleComEmail, http_saml_idp_url);
    fake_gaia_.fake_gaia()->RegisterSamlUser(
        saml_test_users::kFourthUserCorpExampleTestEmail,
        device_attest_saml_idp_url);
    fake_gaia_.fake_gaia()->RegisterSamlUser(
        saml_test_users::kFifthUserExampleTestEmail, base_login_saml_idp_url);
    fake_gaia_.fake_gaia()->RegisterSamlDomainRedirectUrl(
        "example.com", base_login_saml_idp_url);
  }

  void SetUpInProcessBrowserTestFixture() override {
    // Creates a fake CryptohomeClient. Will be destroyed in browser shutdown.
    cryptohome_client_ = new SecretInterceptingFakeCryptohomeClient();

    OobeBaseTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    OobeBaseTest::TearDownInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    fake_gaia_.fake_gaia()->SetFakeMergeSessionParams(
        saml_test_users::kFirstUserCorpExampleComEmail, kTestAuthSIDCookie1,
        kTestAuthLSIDCookie1);

    embedded_test_server()->RegisterRequestHandler(base::Bind(
        &FakeSamlIdp::HandleRequest, base::Unretained(&fake_saml_idp_)));

    OobeBaseTest::SetUpOnMainThread();
  }

  void SetupAuthFlowChangeListener() {
    content::ExecuteScriptAsync(
        GetLoginUI()->GetWebContents(),
        "$('gaia-signin').authenticator_.addEventListener('authFlowChange',"
        "    function f() {"
        "      $('gaia-signin').authenticator_.removeEventListener("
        "          'authFlowChange', f);"
        "      window.domAutomationController.send("
        "          $('gaia-signin').isSamlForTesting() ?"
        "              'SamlLoaded' : 'GaiaLoaded');"
        "    });");
  }

  virtual void StartSamlAndWaitForIdpPageLoad(const std::string& gaia_email) {
    OobeScreenWaiter(GetFirstSigninScreen()).Wait();

    content::DOMMessageQueue message_queue;  // Start observe before SAML.
    SetupAuthFlowChangeListener();
    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<GaiaScreenHandler>()
        ->ShowSigninScreenForTest(gaia_email, "", "[]");

    std::string message;
    do {
      ASSERT_TRUE(message_queue.WaitForMessage(&message));
    } while (message != "\"SamlLoaded\"");
  }

  void SendConfirmPassword(const std::string& password_to_confirm) {
    test::OobeJS().TypeIntoPath(password_to_confirm, kPasswordInput);
    test::OobeJS().TapOnPath(kPasswordSubmit);
  }

  void SetManualPasswords(const std::string& password,
                          const std::string& confirm_password) {
    test::OobeJS().TypeIntoPath(password, kPasswordInput);
    test::OobeJS().TypeIntoPath(confirm_password, kPasswordConfirmInput);
    test::OobeJS().TapOnPath(kPasswordSubmit);
  }

  std::string WaitForAndGetFatalErrorMessage() {
    OobeScreenWaiter(OobeScreen::SCREEN_FATAL_ERROR).Wait();

    EXPECT_TRUE(ash::LoginScreenTestApi::IsShutdownButtonShown());
    EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
    EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());

    std::string message_element = "$('fatal-error-card')";
    std::string error_message;
    if (!content::ExecuteScriptAndExtractString(
            GetLoginUI()->GetWebContents(),
            "window.domAutomationController.send(" + message_element +
                ".textContent);",
            &error_message)) {
      ADD_FAILURE();
    }
    return error_message;
  }

  FakeSamlIdp* fake_saml_idp() { return &fake_saml_idp_; }

 protected:
  HTTPSForwarder saml_https_forwarder_;

  SecretInterceptingFakeCryptohomeClient* cryptohome_client_;

  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};

  chromeos::DeviceStateMixin device_state_{
      &mixin_host_, chromeos::DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};

 private:
  FakeSamlIdp fake_saml_idp_;

  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SamlTest);
};

// Tests that signin frame should display the SAML notice and the 'back' button
// when SAML IdP page is loaded. And the 'back' button goes back to gaia on
// clicking.
IN_PROC_BROWSER_TEST_F(SamlTest, SamlUI) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Saml flow UI expectations.
  test::OobeJS().ExpectVisiblePath({"gaia-signin", "saml-notice-container"});
  test::OobeJS().ExpectVisiblePath({"gaia-signin", "signin-back-button"});
  std::string js = "$SamlNoticeMessagePath.textContent.indexOf('$Host') > -1";
  base::ReplaceSubstringsAfterOffset(
      &js, 0, "$SamlNoticeMessagePath",
      test::GetOobeElementPath({"gaia-signin", "saml-notice-message"}));
  base::ReplaceSubstringsAfterOffset(&js, 0, "$Host", kIdPHost);
  test::OobeJS().ExpectTrue(js);

  content::DOMMessageQueue message_queue;  // Observe before 'back'.
  SetupAuthFlowChangeListener();
  // Click on 'back'.
  content::ExecuteScriptAsync(
      GetLoginUI()->GetWebContents(),
      test::GetOobeElementPath({"gaia-signin", "signin-back-button"}) +
          ".fire('click');");

  // Auth flow should change back to Gaia.
  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"GaiaLoaded\"");

  // Saml flow is gone.
  test::OobeJS().ExpectHiddenPath({"gaia-signin", "saml-notice-container"});
}

// The SAML IdP requires HTTP Protocol-level authentication (Basic in this
// case).
IN_PROC_BROWSER_TEST_F(SamlTest, IdpRequiresHttpAuth) {
  fake_saml_idp()->SetRequireHttpBasicAuth(true);
  fake_saml_idp()->SetLoginHTMLTemplate("saml_api_login.html");
  fake_saml_idp()->SetLoginAuthHTMLTemplate("saml_api_login_auth.html");

  // This is not calling StartSamlAndWaitForIdpPageLoad because it has
  // to wait for the auth credentials entry dialog in between. Also, only load
  // the gaia page first so we can get a pointer to the gaia frame's
  // WebContents.
  WaitForGaiaPageLoad();

  content::WebContents* gaia_frame_web_contents =
      signin::GetAuthFrameWebContents(GetLoginUI()->GetWebContents(),
                                      gaia_frame_parent_);
  content::NavigationController* gaia_frame_navigation_controller =
      &(gaia_frame_web_contents->GetController());

  // Start observing before initiating SAML sign-in.
  content::DOMMessageQueue message_queue;
  LoginPromptBrowserTestObserver login_prompt_observer;
  login_prompt_observer.Register(content::Source<content::NavigationController>(
      gaia_frame_navigation_controller));
  WindowedAuthNeededObserver auth_needed_waiter(
      gaia_frame_navigation_controller);

  SetupAuthFlowChangeListener();
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(saml_test_users::kFirstUserCorpExampleComEmail,
                                "", "[]");

  auth_needed_waiter.Wait();
  ASSERT_FALSE(login_prompt_observer.handlers().empty());
  LoginHandler* handler = *login_prompt_observer.handlers().begin();
  // Note that the actual credentials don't matter because |fake_saml_idp()|
  // doesn't check those (only that something has been provided).
  handler->SetAuth(base::UTF8ToUTF16("user"), base::UTF8ToUTF16("pwd"));

  // Now the SAML sign-in form should actually load.
  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"SamlLoaded\"");

  // Fill-in the SAML IdP form and submit.
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("not_the_password", {"Dummy"});
  SigninFrameJS().TypeIntoPath("actual_password", {"Password"});

  SigninFrameJS().TapOn("Submit");

  // Login should finish login and a session should start.
  test::WaitForPrimaryUserSessionStart();
}

// Tests the sign-in flow when the credentials passing API is used.
IN_PROC_BROWSER_TEST_F(SamlTest, CredentialPassingAPI) {
  base::HistogramTester histogram_tester;
  fake_saml_idp()->SetLoginHTMLTemplate("saml_api_login.html");
  fake_saml_idp()->SetLoginAuthHTMLTemplate("saml_api_login_auth.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Fill-in the SAML IdP form and submit.
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("not_the_password", {"Dummy"});
  SigninFrameJS().TypeIntoPath("actual_password", {"Password"});

  SigninFrameJS().TapOn("Submit");

  // Login should finish login and a session should start.
  test::WaitForPrimaryUserSessionStart();

  // Regression test for http://crbug.com/490737: Verify that the user's actual
  // password was used, not the contents of the first type=password input field
  // found on the page.
  Key key("actual_password");
  key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                SystemSaltGetter::ConvertRawSaltToHexString(
                    FakeCryptohomeClient::GetStubSystemSalt()));
  EXPECT_EQ(key.GetSecret(), cryptohome_client_->salted_hashed_secret());

  EXPECT_TRUE(user_manager::known_user::GetIsUsingSAMLPrincipalsAPI(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kFirstUserCorpExampleComEmail,
          kFirstSAMLUserGaiaId)));

  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.APILogin", 1, 1);
  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.Provider", 1, 1);
}

// Tests the sign-in flow when the credentials passing API is used w/o 'confirm'
// call. The password from the last 'add' should be used.
IN_PROC_BROWSER_TEST_F(SamlTest, CredentialPassingAPIWithoutConfirm) {
  base::HistogramTester histogram_tester;
  fake_saml_idp()->SetLoginHTMLTemplate("saml_api_login.html");
  fake_saml_idp()->SetLoginAuthHTMLTemplate(
      "saml_api_login_auth_without_confirm.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Fill-in the SAML IdP form and submit.
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("last_password", {"Dummy"});
  SigninFrameJS().TypeIntoPath("not_confirmed_password", {"Password"});

  SigninFrameJS().TapOn("Submit");

  // Login should finish login and a session should start.
  test::WaitForPrimaryUserSessionStart();

  // Verify that last password sent by 'add' used.
  Key key("last_password");
  key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                SystemSaltGetter::ConvertRawSaltToHexString(
                    FakeCryptohomeClient::GetStubSystemSalt()));
  EXPECT_EQ(key.GetSecret(), cryptohome_client_->salted_hashed_secret());

  EXPECT_TRUE(user_manager::known_user::GetIsUsingSAMLPrincipalsAPI(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kFirstUserCorpExampleComEmail,
          kFirstSAMLUserGaiaId)));

  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.APILogin", 1, 1);
  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.Provider", 1, 1);
}

// Tests the single password scraped flow.
IN_PROC_BROWSER_TEST_F(SamlTest, ScrapedSingle) {
  base::HistogramTester histogram_tester;
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  content::DOMMessageQueue message_queue;
  // Make sure that the password is scraped correctly.
  ASSERT_TRUE(content::ExecuteScript(
      GetLoginUI()->GetWebContents(),
      "$('gaia-signin').authenticator_.addEventListener('authCompleted',"
      "    function(e) {"
      "      var password = e.detail.password;"
      "      window.domAutomationController.send(password);"
      "    });"));

  // Fill-in the SAML IdP form and submit.
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});

  // Scraping a single password should finish the login and start the session.
  SigninFrameJS().TapOn("Submit");
  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"fake_password\"");

  test::WaitForPrimaryUserSessionStart();

  EXPECT_FALSE(user_manager::known_user::GetIsUsingSAMLPrincipalsAPI(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kFirstUserCorpExampleComEmail,
          kFirstSAMLUserGaiaId)));

  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.APILogin", 2, 1);
  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.Scraping.PasswordCountAll",
                                      1, 1);
}

// Tests password scraping from a dynamically created password field.
IN_PROC_BROWSER_TEST_F(SamlTest, ScrapedDynamic) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  SigninFrameJS().Evaluate(
      "(function() {"
      "  var newPassInput = document.createElement('input');"
      "  newPassInput.id = 'DynamicallyCreatedPassword';"
      "  newPassInput.type = 'password';"
      "  newPassInput.name = 'Password';"
      "  document.forms[0].appendChild(newPassInput);"
      "})();");

  // Fill-in the SAML IdP form and submit.
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"DynamicallyCreatedPassword"});

  // Scraping a single password should finish the login and start the session.
  SigninFrameJS().TapOn("Submit");
  test::WaitForPrimaryUserSessionStart();

  EXPECT_FALSE(user_manager::known_user::GetIsUsingSAMLPrincipalsAPI(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kFirstUserCorpExampleComEmail,
          kFirstSAMLUserGaiaId)));
}

// Tests the multiple password scraped flow.
IN_PROC_BROWSER_TEST_F(SamlTest, ScrapedMultiple) {
  base::HistogramTester histogram_tester;
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_two_passwords.html");

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});
  SigninFrameJS().TypeIntoPath("password1", {"Password1"});
  SigninFrameJS().TapOn("Submit");
  // Lands on confirm password screen.
  OobeScreenWaiter(OobeScreen::SCREEN_CONFIRM_PASSWORD).Wait();
  test::OobeJS().ExpectHiddenPath(kPasswordConfirmInput);
  // Entering an unknown password should go back to the confirm password screen.
  SendConfirmPassword("wrong_password");
  OobeScreenWaiter(OobeScreen::SCREEN_CONFIRM_PASSWORD).Wait();
  test::OobeJS().ExpectHiddenPath(kPasswordConfirmInput);
  // Either scraped password should be able to sign-in.
  SendConfirmPassword("password1");
  test::WaitForPrimaryUserSessionStart();

  EXPECT_FALSE(user_manager::known_user::GetIsUsingSAMLPrincipalsAPI(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kFirstUserCorpExampleComEmail,
          kFirstSAMLUserGaiaId)));

  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.APILogin", 2, 1);
  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.Scraping.PasswordCountAll",
                                      2, 1);
}

// Tests the no password scraped flow.
IN_PROC_BROWSER_TEST_F(SamlTest, ScrapedNone) {
  base::HistogramTester histogram_tester;
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_no_passwords.html");

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TapOn("Submit");

  // Lands on confirm password screen with manual input state.
  OobeScreenWaiter(OobeScreen::SCREEN_CONFIRM_PASSWORD).Wait();
  test::OobeJS().ExpectTrue("$('saml-confirm-password').isManualInput");
  // Entering passwords that don't match will make us land again in the same
  // page.
  SetManualPasswords("Test1", "Test2");
  OobeScreenWaiter(OobeScreen::SCREEN_CONFIRM_PASSWORD).Wait();
  test::OobeJS().ExpectTrue("$('saml-confirm-password').isManualInput");

  // Two matching passwords should let the user to sign in.
  SetManualPasswords("Test1", "Test1");
  test::WaitForPrimaryUserSessionStart();

  EXPECT_FALSE(user_manager::known_user::GetIsUsingSAMLPrincipalsAPI(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kFirstUserCorpExampleComEmail,
          kFirstSAMLUserGaiaId)));

  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.APILogin", 2, 1);
  histogram_tester.ExpectUniqueSample("ChromeOS.SAML.Scraping.PasswordCountAll",
                                      0, 1);
}

// Types the second user e-mail into the GAIA login form but then authenticates
// as the first user via SAML. Verifies that the logged-in user is correctly
// identified as the first user.
IN_PROC_BROWSER_TEST_F(SamlTest, UseAutenticatedUserEmailAddress) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  // Type the second user e-mail into the GAIA login form.
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kSecondUserCorpExampleComEmail);

  // Authenticate as the first user via SAML (the |Email| provided here is
  // irrelevant - the authenticated user's e-mail address that FakeGAIA reports
  // was set via |SetFakeMergeSessionParams|).
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});

  SigninFrameJS().TapOn("Submit");
  test::WaitForPrimaryUserSessionStart();

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  ASSERT_TRUE(user);
  EXPECT_EQ(saml_test_users::kFirstUserCorpExampleComEmail,
            user->GetAccountId().GetUserEmail());
}

// Verifies that if the authenticated user's e-mail address cannot be retrieved,
// an error message is shown.
IN_PROC_BROWSER_TEST_F(SamlTest, FailToRetrieveAutenticatedUserEmailAddress) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  fake_gaia_.fake_gaia()->SetFakeMergeSessionParams("", kTestAuthSIDCookie1,
                                                    kTestAuthLSIDCookie1);
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});
  SigninFrameJS().TapOn("Submit");

  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_LOGIN_FATAL_ERROR_NO_ACCOUNT_DETAILS),
            WaitForAndGetFatalErrorMessage());
}

// Tests the password confirm flow when more than one password is scraped: show
// error on the first failure and fatal error on the second failure.
IN_PROC_BROWSER_TEST_F(SamlTest, PasswordConfirmFlow) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_two_passwords.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Fill-in the SAML IdP form and submit.
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});
  SigninFrameJS().TypeIntoPath("password1", {"Password1"});
  SigninFrameJS().TapOn("Submit");

  // Lands on confirm password screen with no error message.
  OobeScreenWaiter(OobeScreen::SCREEN_CONFIRM_PASSWORD).Wait();
  test::OobeJS().ExpectHiddenPath(kPasswordConfirmInput);
  test::OobeJS().ExpectTrue(
      "!$('saml-confirm-password').$.passwordInput.invalid");

  // Enter an unknown password for the first time should go back to confirm
  // password screen with error message.
  SendConfirmPassword("wrong_password");
  OobeScreenWaiter(OobeScreen::SCREEN_CONFIRM_PASSWORD).Wait();
  test::OobeJS().ExpectHiddenPath(kPasswordConfirmInput);
  test::OobeJS().ExpectTrue(
      "$('saml-confirm-password').$.passwordInput.invalid");

  // Enter an unknown password 2nd time should go back fatal error message.
  SendConfirmPassword("wrong_password");
  EXPECT_EQ(
      l10n_util::GetStringUTF8(IDS_LOGIN_FATAL_ERROR_PASSWORD_VERIFICATION),
      WaitForAndGetFatalErrorMessage());
}

// Verifies that when the login flow redirects from one host to another, the
// notice shown to the user is updated. This guards against regressions of
// http://crbug.com/447818.
IN_PROC_BROWSER_TEST_F(SamlTest, NoticeUpdatedOnRedirect) {
  // Start another https server at |kAdditionalIdPHost|.
  HTTPSForwarder saml_https_forwarder_2;
  ASSERT_TRUE(saml_https_forwarder_2.Initialize(
      kAdditionalIdPHost, embedded_test_server()->base_url()));

  // Make the login flow redirect to |kAdditionalIdPHost|.
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_instant_meta_refresh.html");
  fake_saml_idp()->SetRefreshURL(
      saml_https_forwarder_2.GetURLForSSLHost("simple.html"));
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Wait until the notice shown to the user is updated to contain
  // |kAdditionalIdPHost|.
  std::string js =
      "var sendIfHostFound = function() {"
      "  var found = $SamlNoticeMessagePath.textContent.indexOf('$Host') > -1;"
      "  if (found)"
      "    window.domAutomationController.send(true);"
      "  return found;"
      "};"
      "var processEventsAndSendIfHostFound = function() {"
      "  window.setTimeout(function() {"
      "    if (sendIfHostFound()) {"
      "      $('gaia-signin').authenticator_.removeEventListener("
      "          'authDomainChange',"
      "          processEventsAndSendIfHostFound);"
      "    }"
      "  }, 0);"
      "};"
      "if (!sendIfHostFound()) {"
      "  $('gaia-signin').authenticator_.addEventListener("
      "      'authDomainChange',"
      "      processEventsAndSendIfHostFound);"
      "}";
  base::ReplaceSubstringsAfterOffset(
      &js, 0, "$SamlNoticeMessagePath",
      test::GetOobeElementPath({"gaia-signin", "saml-notice-message"}));
  base::ReplaceSubstringsAfterOffset(&js, 0, "$Host", kAdditionalIdPHost);
  bool dummy;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetLoginUI()->GetWebContents(), js, &dummy));

  // Verify that the notice is visible.
  test::OobeJS().ExpectVisiblePath({"gaia-signin", "saml-notice-container"});
}

// Verifies that when GAIA attempts to redirect to a SAML IdP served over http,
// not https, the redirect is blocked and an error message is shown.
IN_PROC_BROWSER_TEST_F(SamlTest, HTTPRedirectDisallowed) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  WaitForSigninScreen();
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(saml_test_users::kThirdUserCorpExampleComEmail,
                                "", "[]");

  const GURL url = embedded_test_server()->base_url().Resolve("/SAML");
  EXPECT_EQ(l10n_util::GetStringFUTF8(IDS_LOGIN_FATAL_ERROR_TEXT_INSECURE_URL,
                                      base::UTF8ToUTF16(url.spec())),
            WaitForAndGetFatalErrorMessage());
}

// Verifies that when GAIA attempts to redirect to a page served over http, not
// https, via an HTML meta refresh, the redirect is blocked and an error message
// is shown. This guards against regressions of http://crbug.com/359515.
IN_PROC_BROWSER_TEST_F(SamlTest, MetaRefreshToHTTPDisallowed) {
  const GURL url = embedded_test_server()->base_url().Resolve("/SSO");
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_instant_meta_refresh.html");
  fake_saml_idp()->SetRefreshURL(url);

  WaitForSigninScreen();
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(saml_test_users::kFirstUserCorpExampleComEmail,
                                "", "[]");

  EXPECT_EQ(l10n_util::GetStringFUTF8(IDS_LOGIN_FATAL_ERROR_TEXT_INSECURE_URL,
                                      base::UTF8ToUTF16(url.spec())),
            WaitForAndGetFatalErrorMessage());
}

class SAMLEnrollmentTest : public SamlTest {
 public:
  SAMLEnrollmentTest();
  ~SAMLEnrollmentTest() override;

  // SamlTest:
  void SetUpOnMainThread() override;
  void StartSamlAndWaitForIdpPageLoad(const std::string& gaia_email) override;

 protected:
  LocalPolicyTestServerMixin local_policy_mixin_{&mixin_host_};
  test::EnrollmentUIMixin enrollment_ui_{&mixin_host_};

 private:
  DISALLOW_COPY_AND_ASSIGN(SAMLEnrollmentTest);
};

SAMLEnrollmentTest::SAMLEnrollmentTest() {
  gaia_frame_parent_ = "authView";
  authenticator_id_ = "$('enterprise-enrollment').authenticator_";
}

SAMLEnrollmentTest::~SAMLEnrollmentTest() {}

void SAMLEnrollmentTest::SetUpOnMainThread() {
  FakeGaia::AccessTokenInfo token_info;
  token_info.token = kTestUserinfoToken;
  token_info.scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
  token_info.scopes.insert(GaiaConstants::kOAuthWrapBridgeUserInfoScope);
  token_info.audience = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  token_info.email = saml_test_users::kFirstUserCorpExampleComEmail;
  fake_gaia_.fake_gaia()->IssueOAuthToken(kTestRefreshToken, token_info);

  SamlTest::SetUpOnMainThread();
}

void SAMLEnrollmentTest::StartSamlAndWaitForIdpPageLoad(
    const std::string& gaia_email) {
  LoginDisplayHost::default_host()->StartWizard(
      EnrollmentScreenView::kScreenId);
  WaitForGaiaPageBackButtonUpdate();
  auto flow_change_waiter =
      OobeBaseTest::CreateGaiaPageEventWaiter("authFlowChange");
  SigninFrameJS().TypeIntoPath(gaia_email, {"identifier"});
  SigninFrameJS().TapOn("nextButton");
  flow_change_waiter->Wait();
}

IN_PROC_BROWSER_TEST_F(SAMLEnrollmentTest, WithoutCredentialsPassingAPI) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Fill-in the SAML IdP form and submit.
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});
  SigninFrameJS().TapOn("Submit");

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepDeviceAttributes);
}

IN_PROC_BROWSER_TEST_F(SAMLEnrollmentTest, WithCredentialsPassingAPI) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_api_login.html");
  fake_saml_idp()->SetLoginAuthHTMLTemplate("saml_api_login_auth.html");
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Fill-in the SAML IdP form and submit.
  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});
  SigninFrameJS().TapOn("Submit");

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepDeviceAttributes);
}

class SAMLPolicyTest : public SamlTest {
 public:
  SAMLPolicyTest();
  ~SAMLPolicyTest() override;

  // SamlTest:
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;

  void SetSAMLOfflineSigninTimeLimitPolicy(int limit);
  void EnableTransferSAMLCookiesPolicy();
  void SetLoginBehaviorPolicyToSAMLInterstitial();
  void SetLoginVideoCaptureAllowedUrls(const std::vector<GURL>& allowed);

  void ShowGAIALoginForm();
  void ShowSAMLInterstitial();
  void ClickBackOnSAMLInterstitialPage();
  void ClickNextOnSAMLInterstitialPage();
  void LogInWithSAML(const std::string& user_id,
                     const std::string& auth_sid_cookie,
                     const std::string& auth_lsid_cookie);

  std::string GetCookieValue(const std::string& name);

  void GetCookies();

 protected:
  policy::DevicePolicyCrosTestHelper test_helper_;
  policy::DevicePolicyBuilder* device_policy_;
  policy::MockConfigurationPolicyProvider provider_;
  net::CookieList cookie_list_;

  // Add a fake user so the login screen does not show GAIA auth by default.
  // This enables tests to control when the GAIA is shown (and ensure it's
  // loaded after SAML config has been set up).
  chromeos::LoginManagerMixin login_manager_{
      &mixin_host_,
      {chromeos::LoginManagerMixin::TestUserInfo(
          AccountId::FromUserEmailGaiaId("user@gmail.com", "1111"))}};

 private:
  DISALLOW_COPY_AND_ASSIGN(SAMLPolicyTest);
};

SAMLPolicyTest::SAMLPolicyTest()
    : device_policy_(test_helper_.device_policy()) {
  device_state_.SetState(
      DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED);
  device_state_.set_skip_initial_policy_setup(true);
}

SAMLPolicyTest::~SAMLPolicyTest() {}

void SAMLPolicyTest::SetUpInProcessBrowserTestFixture() {
  SessionManagerClient::InitializeFakeInMemory();

  SamlTest::SetUpInProcessBrowserTestFixture();

  // Initialize device policy.
  std::set<std::string> device_affiliation_ids;
  device_affiliation_ids.insert(kAffiliationID);
  auto affiliation_helper = policy::AffiliationTestHelper::CreateForCloud(
      FakeSessionManagerClient::Get());
  ASSERT_NO_FATAL_FAILURE((affiliation_helper.SetDeviceAffiliationIDs(
      &test_helper_, device_affiliation_ids)));

  // Initialize user policy.
  EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
      .WillRepeatedly(testing::Return(true));
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
}

void SAMLPolicyTest::SetUpOnMainThread() {
  SamlTest::SetUpOnMainThread();

  // Pretend that the test users' OAuth tokens are valid.
  user_manager::UserManager::Get()->SaveUserOAuthStatus(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kFirstUserCorpExampleComEmail, kFirstSAMLUserGaiaId),
      user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
  user_manager::UserManager::Get()->SaveUserOAuthStatus(
      AccountId::FromUserEmailGaiaId(kNonSAMLUserEmail, kNonSAMLUserGaiaId),
      user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
  user_manager::UserManager::Get()->SaveUserOAuthStatus(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kFifthUserExampleTestEmail, kFifthSAMLUserGaiaId),
      user_manager::User::OAUTH2_TOKEN_STATUS_VALID);

  // Give affiliated users appropriate affiliation IDs.
  std::set<std::string> user_affiliation_ids;
  user_affiliation_ids.insert(kAffiliationID);
  chromeos::ChromeUserManager::Get()->SetUserAffiliation(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kFirstUserCorpExampleComEmail, kFirstSAMLUserGaiaId),
      user_affiliation_ids);
  chromeos::ChromeUserManager::Get()->SetUserAffiliation(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kSecondUserCorpExampleComEmail,
          kSecondSAMLUserGaiaId),
      user_affiliation_ids);
  chromeos::ChromeUserManager::Get()->SetUserAffiliation(
      AccountId::FromUserEmailGaiaId(
          saml_test_users::kThirdUserCorpExampleComEmail, kThirdSAMLUserGaiaId),
      user_affiliation_ids);
  chromeos::ChromeUserManager::Get()->SetUserAffiliation(
      AccountId::FromUserEmailGaiaId(kNonSAMLUserEmail, kNonSAMLUserGaiaId),
      user_affiliation_ids);

  // Set up fake networks.
  DBusThreadManager::Get()
      ->GetShillManagerClient()
      ->GetTestInterface()
      ->SetupDefaultEnvironment();
}

void SAMLPolicyTest::SetSAMLOfflineSigninTimeLimitPolicy(int limit) {
  policy::PolicyMap user_policy;
  user_policy.Set(policy::key::kSAMLOfflineSigninTimeLimit,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD, base::Value(limit), nullptr);
  provider_.UpdateChromePolicy(user_policy);
  base::RunLoop().RunUntilIdle();
}

void SAMLPolicyTest::EnableTransferSAMLCookiesPolicy() {
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  proto.mutable_saml_settings()->set_transfer_saml_cookies(true);

  base::RunLoop run_loop;
  std::unique_ptr<CrosSettings::ObserverSubscription> observer =
      CrosSettings::Get()->AddSettingsObserver(kAccountsPrefTransferSAMLCookies,
                                               run_loop.QuitClosure());
  device_policy_->SetDefaultSigningKey();
  device_policy_->Build();
  FakeSessionManagerClient::Get()->set_device_policy(device_policy_->GetBlob());
  FakeSessionManagerClient::Get()->OnPropertyChangeComplete(true);
  run_loop.Run();
}

void SAMLPolicyTest::SetLoginBehaviorPolicyToSAMLInterstitial() {
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  proto.mutable_login_authentication_behavior()
      ->set_login_authentication_behavior(
          em::LoginAuthenticationBehaviorProto_LoginBehavior_SAML_INTERSTITIAL);

  base::RunLoop run_loop;
  std::unique_ptr<CrosSettings::ObserverSubscription> observer =
      CrosSettings::Get()->AddSettingsObserver(kLoginAuthenticationBehavior,
                                               run_loop.QuitClosure());
  device_policy_->SetDefaultSigningKey();
  device_policy_->Build();
  FakeSessionManagerClient::Get()->set_device_policy(device_policy_->GetBlob());
  FakeSessionManagerClient::Get()->OnPropertyChangeComplete(true);
  run_loop.Run();
}

void SAMLPolicyTest::SetLoginVideoCaptureAllowedUrls(
    const std::vector<GURL>& allowed) {
  em::ChromeDeviceSettingsProto& proto(device_policy_->payload());
  for (const GURL& url : allowed)
    proto.mutable_login_video_capture_allowed_urls()->add_urls(url.spec());

  base::RunLoop run_loop;
  std::unique_ptr<CrosSettings::ObserverSubscription> observer =
      CrosSettings::Get()->AddSettingsObserver(kLoginVideoCaptureAllowedUrls,
                                               run_loop.QuitClosure());
  device_policy_->SetDefaultSigningKey();
  device_policy_->Build();
  FakeSessionManagerClient::Get()->set_device_policy(device_policy_->GetBlob());
  FakeSessionManagerClient::Get()->OnPropertyChangeComplete(true);
  run_loop.Run();
}

void SAMLPolicyTest::ShowGAIALoginForm() {
  content::DOMMessageQueue message_queue;
  ASSERT_TRUE(content::ExecuteScript(
      GetLoginUI()->GetWebContents(),
      "$('gaia-signin').authenticator_.addEventListener('ready', function() {"
      "  window.domAutomationController.send('ready');"
      "});"));
  ASSERT_TRUE(ash::LoginScreenTestApi::ClickAddUserButton());
  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"ready\"");
}

void SAMLPolicyTest::ShowSAMLInterstitial() {
  WaitForOobeUI();
  ASSERT_TRUE(ash::LoginScreenTestApi::ClickAddUserButton());
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"gaia-signin", "saml-interstitial"})
      ->Wait();
}

void SAMLPolicyTest::ClickBackOnSAMLInterstitialPage() {
  test::OobeJS().TapOnPath({"gaia-signin", "interstitial-back"});
}

void SAMLPolicyTest::ClickNextOnSAMLInterstitialPage() {
  content::DOMMessageQueue message_queue;
  SetupAuthFlowChangeListener();

  test::OobeJS().TapOnPath({"gaia-signin", "interstitial-next"});

  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"SamlLoaded\"");
}

void SAMLPolicyTest::LogInWithSAML(const std::string& user_id,
                                   const std::string& auth_sid_cookie,
                                   const std::string& auth_lsid_cookie) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  StartSamlAndWaitForIdpPageLoad(user_id);

  fake_gaia_.fake_gaia()->SetFakeMergeSessionParams(user_id, auth_sid_cookie,
                                                    auth_lsid_cookie);
  fake_gaia_.SetupFakeGaiaForLogin(user_id, "", kTestRefreshToken);

  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});

  // Scraping a single password should finish the login right away.
  SigninFrameJS().TapOn("Submit");
  test::WaitForPrimaryUserSessionStart();
}

std::string SAMLPolicyTest::GetCookieValue(const std::string& name) {
  for (net::CookieList::const_iterator it = cookie_list_.begin();
       it != cookie_list_.end(); ++it) {
    if (it->Name() == name)
      return it->Value();
  }
  return std::string();
}

void SAMLPolicyTest::GetCookies() {
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUserUnsafe(
      user_manager::UserManager::Get()->GetActiveUser());
  ASSERT_TRUE(profile);
  base::RunLoop run_loop;
  content::BrowserContext::GetDefaultStoragePartition(profile)
      ->GetCookieManagerForBrowserProcess()
      ->GetAllCookies(base::BindLambdaForTesting(
          [&](const std::vector<net::CanonicalCookie>& cookies) {
            cookie_list_ = cookies;
            run_loop.Quit();
          }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_NoSAML) {
  // Set the offline login time limit for SAML users to zero.
  SetSAMLOfflineSigninTimeLimitPolicy(0);

  WaitForSigninScreen();

  fake_gaia_.fake_gaia()->SetFakeMergeSessionParams(
      kNonSAMLUserEmail, FakeGaiaMixin::kFakeSIDCookie,
      FakeGaiaMixin::kFakeLSIDCookie);
  fake_gaia_.SetupFakeGaiaForLogin(kNonSAMLUserEmail, "", kTestRefreshToken);

  // Log in without SAML.
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(kNonSAMLUserEmail, "password", "[]");

  test::WaitForPrimaryUserSessionStart();
}

// Verifies that the offline login time limit does not affect a user who
// authenticated without SAML.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, NoSAML) {
  // Verify that offline login is allowed.
  ash::LoginScreenTestApi::SubmitPassword(
      AccountId::FromUserEmail(kNonSAMLUserEmail), "password",
      true /* check_if_submittable */);
  test::WaitForPrimaryUserSessionStart();
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_SAMLNoLimit) {
  // Remove the offline login time limit for SAML users.
  SetSAMLOfflineSigninTimeLimitPolicy(-1);

  ShowGAIALoginForm();
  LogInWithSAML(saml_test_users::kFirstUserCorpExampleComEmail,
                kTestAuthSIDCookie1, kTestAuthLSIDCookie1);
}

// Verifies that when no offline login time limit is set, a user who
// authenticated with SAML is allowed to log in offline.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, SAMLNoLimit) {
  // Verify that offline login is allowed.
  ash::LoginScreenTestApi::SubmitPassword(
      AccountId::FromUserEmail(saml_test_users::kFirstUserCorpExampleComEmail),
      "password", true /* check_if_submittable */);
  test::WaitForPrimaryUserSessionStart();
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_SAMLZeroLimit) {
  // Set the offline login time limit for SAML users to zero.
  SetSAMLOfflineSigninTimeLimitPolicy(0);

  ShowGAIALoginForm();
  LogInWithSAML(saml_test_users::kFirstUserCorpExampleComEmail,
                kTestAuthSIDCookie1, kTestAuthLSIDCookie1);
}

// Verifies that when the offline login time limit is exceeded for a user who
// authenticated via SAML, that user is forced to log in online the next time.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, SAMLZeroLimit) {
  // Verify that offline login is not allowed.
  ASSERT_TRUE(
      ash::LoginScreenTestApi::IsForcedOnlineSignin(AccountId::FromUserEmail(
          saml_test_users::kFirstUserCorpExampleComEmail)));
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_PRE_TransferCookiesAffiliated) {
  fake_saml_idp()->SetCookieValue(kSAMLIdPCookieValue1);
  ShowGAIALoginForm();
  LogInWithSAML(saml_test_users::kFirstUserCorpExampleComEmail,
                kTestAuthSIDCookie1, kTestAuthLSIDCookie1);

  GetCookies();
  EXPECT_EQ(kTestAuthSIDCookie1, GetCookieValue(kGAIASIDCookieName));
  EXPECT_EQ(kTestAuthLSIDCookie1, GetCookieValue(kGAIALSIDCookieName));
  EXPECT_EQ(kSAMLIdPCookieValue1, GetCookieValue(kSAMLIdPCookieName));
}

// Verifies that when the DeviceTransferSAMLCookies policy is not enabled, SAML
// IdP cookies are not transferred to a user's profile on subsequent login, even
// if the user belongs to the domain that the device is enrolled into. Also
// verifies that GAIA cookies are not transferred.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_TransferCookiesAffiliated) {
  fake_saml_idp()->SetCookieValue(kSAMLIdPCookieValue2);
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  ShowGAIALoginForm();
  LogInWithSAML(saml_test_users::kFirstUserCorpExampleComEmail,
                kTestAuthSIDCookie2, kTestAuthLSIDCookie2);

  GetCookies();
  EXPECT_EQ(kTestAuthSIDCookie1, GetCookieValue(kGAIASIDCookieName));
  EXPECT_EQ(kTestAuthLSIDCookie1, GetCookieValue(kGAIALSIDCookieName));
  EXPECT_EQ(kSAMLIdPCookieValue1, GetCookieValue(kSAMLIdPCookieName));
}

// Verifies that when the DeviceTransferSAMLCookies policy is enabled, SAML IdP
// cookies are transferred to a user's profile on subsequent login when the user
// belongs to the domain that the device is enrolled into. Also verifies that
// GAIA cookies are not transferred.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, TransferCookiesAffiliated) {
  fake_saml_idp()->SetCookieValue(kSAMLIdPCookieValue2);
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  ShowGAIALoginForm();

  EnableTransferSAMLCookiesPolicy();
  LogInWithSAML(saml_test_users::kFirstUserCorpExampleComEmail,
                kTestAuthSIDCookie2, kTestAuthLSIDCookie2);

  GetCookies();
  EXPECT_EQ(kTestAuthSIDCookie1, GetCookieValue(kGAIASIDCookieName));
  EXPECT_EQ(kTestAuthLSIDCookie1, GetCookieValue(kGAIALSIDCookieName));
  EXPECT_EQ(kSAMLIdPCookieValue2, GetCookieValue(kSAMLIdPCookieName));
}

IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, PRE_TransferCookiesUnaffiliated) {
  fake_saml_idp()->SetCookieValue(kSAMLIdPCookieValue1);
  ShowGAIALoginForm();
  LogInWithSAML(saml_test_users::kFifthUserExampleTestEmail,
                kTestAuthSIDCookie1, kTestAuthLSIDCookie1);

  GetCookies();
  EXPECT_EQ(kTestAuthSIDCookie1, GetCookieValue(kGAIASIDCookieName));
  EXPECT_EQ(kTestAuthLSIDCookie1, GetCookieValue(kGAIALSIDCookieName));
  EXPECT_EQ(kSAMLIdPCookieValue1, GetCookieValue(kSAMLIdPCookieName));
}

// Verifies that even if the DeviceTransferSAMLCookies policy is enabled, SAML
// IdP are not transferred to a user's profile on subsequent login if the user
// does not belong to the domain that the device is enrolled into. Also verifies
// that GAIA cookies are not transferred.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, TransferCookiesUnaffiliated) {
  fake_saml_idp()->SetCookieValue(kSAMLIdPCookieValue2);
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  ShowGAIALoginForm();

  EnableTransferSAMLCookiesPolicy();
  LogInWithSAML(saml_test_users::kFifthUserExampleTestEmail,
                kTestAuthSIDCookie1, kTestAuthLSIDCookie1);

  GetCookies();
  EXPECT_EQ(kTestAuthSIDCookie1, GetCookieValue(kGAIASIDCookieName));
  EXPECT_EQ(kTestAuthLSIDCookie1, GetCookieValue(kGAIALSIDCookieName));
  EXPECT_EQ(kSAMLIdPCookieValue1, GetCookieValue(kSAMLIdPCookieName));
}

// Tests that the SAML interstitial page is loaded when the authentication
// behavior device policy is set to SAML_INTERSTITIAL, and when the user clicks
// the "change account" link, we go back to the default GAIA signin screen.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, SAMLInterstitialChangeAccount) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  SetLoginBehaviorPolicyToSAMLInterstitial();
  WaitForSigninScreen();

  ShowSAMLInterstitial();
  test::OobeJS().ExpectHiddenPath({"gaia-signin", "signin-frame-dialog"});
  test::OobeJS().ExpectHiddenPath({"gaia-signin", "offline-gaia"});
  test::OobeJS().ExpectVisiblePath({"gaia-signin", "saml-interstitial"});

  // Click the "change account" link on the SAML interstitial page.
  test::OobeJS().TapLinkOnPath({"gaia-signin", "interstitial-change-account"});

  // Expects that only the gaia signin frame is visible and shown.
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"gaia-signin", "signin-frame-dialog"})
      ->Wait();
  test::OobeJS()
      .CreateVisibilityWaiter(false, {"gaia-signin", "gaia-loading"})
      ->Wait();
  test::OobeJS().ExpectHasNoAttribute(
      "transparent", {"gaia-signin", "signin-frame-container"});
  test::OobeJS().ExpectHiddenPath({"gaia-signin", "offline-gaia"});
  test::OobeJS().ExpectHiddenPath({"gaia-signin", "saml-interstitial"});
}

// Tests that clicking "Next" in the SAML interstitial page successfully
// triggers a SAML redirect request, and the SAML IdP authentication page is
// loaded and authenticaing there is successful.
// TODO(https://crbug.com/1102738) flaky test
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, DISABLED_SAMLInterstitialNext) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  fake_gaia_.fake_gaia()->SetFakeMergeSessionParams(
      saml_test_users::kFirstUserCorpExampleComEmail, kTestAuthSIDCookie1,
      kTestAuthLSIDCookie1);
  SetLoginBehaviorPolicyToSAMLInterstitial();
  WaitForSigninScreen();

  ShowSAMLInterstitial();
  ClickBackOnSAMLInterstitialPage();
  // Back button should hide OOBE dialog.
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_TRUE(ash::LoginScreenTestApi::IsAddUserButtonShown());

  ShowSAMLInterstitial();
  ClickNextOnSAMLInterstitialPage();

  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});

  // Scraping one password should finish login.
  SigninFrameJS().TapOn("Submit");
  test::WaitForPrimaryUserSessionStart();
}

// Ensure that the permission status of getUserMedia requests from SAML login
// pages is controlled by the kLoginVideoCaptureAllowedUrls pref rather than the
// underlying user content setting.
IN_PROC_BROWSER_TEST_F(SAMLPolicyTest, TestLoginMediaPermission) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  const GURL url1("https://google.com");
  const GURL url2("https://corp.example.com");
  const GURL url3("https://not-allowed.com");
  SetLoginVideoCaptureAllowedUrls({url1, url2});
  WaitForSigninScreen();

  content::WebContents* web_contents = GetLoginUI()->GetWebContents();
  content::WebContentsDelegate* web_contents_delegate =
      web_contents->GetDelegate();

  // Mic should always be blocked.
  EXPECT_FALSE(web_contents_delegate->CheckMediaAccessPermission(
      web_contents->GetMainFrame(), url1,
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE));

  // Camera should be allowed if allowed by the allowlist, otherwise blocked.
  EXPECT_TRUE(web_contents_delegate->CheckMediaAccessPermission(
      web_contents->GetMainFrame(), url1,
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));

  EXPECT_TRUE(web_contents_delegate->CheckMediaAccessPermission(
      web_contents->GetMainFrame(), url2,
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));

  EXPECT_FALSE(web_contents_delegate->CheckMediaAccessPermission(
      web_contents->GetMainFrame(), url3,
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));

  // Camera should be blocked in the login screen, even if it's allowed via
  // content setting.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetContentSettingDefaultScope(url3, url3,
                                      ContentSettingsType::MEDIASTREAM_CAMERA,
                                      std::string(), CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(web_contents_delegate->CheckMediaAccessPermission(
      web_contents->GetMainFrame(), url3,
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE));
}

class SAMLPasswordAttributesTest : public SAMLPolicyTest,
                                   public testing::WithParamInterface<bool> {
 public:
  SAMLPasswordAttributesTest() = default;
  void SetUpOnMainThread() override;

 protected:
  bool in_session_pw_change_policy_enabled() { return GetParam(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(SAMLPasswordAttributesTest);
};

void SAMLPasswordAttributesTest::SetUpOnMainThread() {
  policy::PolicyMap user_policy;
  user_policy.Set(policy::key::kSamlInSessionPasswordChangeEnabled,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD,
                  base::Value(in_session_pw_change_policy_enabled()), nullptr);
  provider_.UpdateChromePolicy(user_policy);
  base::RunLoop().RunUntilIdle();

  SAMLPolicyTest::SetUpOnMainThread();
}

// Verifies that password attributes are extracted and stored during a
// successful log in - but only if the appropriate policy is enabled.
IN_PROC_BROWSER_TEST_P(SAMLPasswordAttributesTest, LoginSucceeded) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  fake_saml_idp()->SetSamlResponseFile("saml_with_password_attributes.xml");
  ShowGAIALoginForm();
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});

  SigninFrameJS().TapOn("Submit");
  test::WaitForPrimaryUserSessionStart();

  Profile* profile = ProfileHelper::Get()->GetProfileByUser(
      user_manager::UserManager::Get()->GetPrimaryUser());
  SamlPasswordAttributes attrs =
      SamlPasswordAttributes::LoadFromPrefs(profile->GetPrefs());

  if (in_session_pw_change_policy_enabled()) {
    // These values are extracted from saml_with_password_attributes.xml
    EXPECT_EQ(base::Time::FromJsTime(1550836258421L), attrs.modified_time());
    EXPECT_EQ(base::Time::FromJsTime(1551873058421L), attrs.expiration_time());
    EXPECT_EQ("https://example.com/adfs/portal/updatepassword/",
              attrs.password_change_url());
  } else {
    // Nothing should be extracted when policy is disabled.
    EXPECT_FALSE(attrs.has_modified_time());
    EXPECT_FALSE(attrs.has_expiration_time());
    EXPECT_FALSE(attrs.has_password_change_url());
  }
}

// Verify that no password attributes are stored when login fails.
IN_PROC_BROWSER_TEST_P(SAMLPasswordAttributesTest, LoginFailed) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
  fake_saml_idp()->SetSamlResponseFile("saml_with_password_attributes.xml");
  ShowGAIALoginForm();
  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFirstUserCorpExampleComEmail);

  // Give fake gaia an empty email address, so login will fail:
  fake_gaia_.fake_gaia()->SetFakeMergeSessionParams(
      /*email=*/"", kTestAuthSIDCookie1, kTestAuthLSIDCookie1);

  SigninFrameJS().TypeIntoPath("fake_user", {"Email"});
  SigninFrameJS().TypeIntoPath("fake_password", {"Password"});
  SigninFrameJS().TapOn("Submit");

  // SAML login fails:
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_LOGIN_FATAL_ERROR_NO_ACCOUNT_DETAILS),
            WaitForAndGetFatalErrorMessage());

  // Make sure no SAML password attributes are saved.
  // None are saved for the logged in user, since there is no logged in user:
  EXPECT_EQ(nullptr, user_manager::UserManager::Get()->GetPrimaryUser());

  // Also, no attributes are saved in the signin profile:
  Profile* profile = ProfileHelper::Get()->GetSigninProfile();
  SamlPasswordAttributes attrs =
      SamlPasswordAttributes::LoadFromPrefs(profile->GetPrefs());
  EXPECT_FALSE(attrs.has_modified_time());
  EXPECT_FALSE(attrs.has_expiration_time());
  EXPECT_FALSE(attrs.has_password_change_url());
}

INSTANTIATE_TEST_SUITE_P(All, SAMLPasswordAttributesTest, testing::Bool());

void FakeGetCertificateCallbackTrue(
    attestation::AttestationFlow::CertificateCallback callback) {
  std::move(callback).Run(attestation::ATTESTATION_SUCCESS, "certificate");
}

void FakeEnterpriseChallenge(
    const std::string& challenge,
    cryptohome::AsyncMethodCaller::DataCallback callback) {
  if (challenge == GetTpmChallenge()) {
    std::move(callback).Run(/*success=*/true, GetTpmResponse());
  } else {
    NOTREACHED();
  }
}

constexpr base::TimeDelta kTimeoutTaskDelay =
    base::TimeDelta::FromMilliseconds(500);
constexpr base::TimeDelta kBuildResponseTaskDelay =
    base::TimeDelta::FromSeconds(3);
static_assert(
    kTimeoutTaskDelay < kBuildResponseTaskDelay,
    "kTimeoutTaskDelay should be less than kBuildResponseTaskDelay to trigger "
    "timeout error in SAMLDeviceAttestationTest.TimeoutError test.");

void FakeEnterpriseChallengeWithDelay(
    const std::string& challenge,
    cryptohome::AsyncMethodCaller::DataCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(FakeEnterpriseChallenge, challenge, std::move(callback)),
      kBuildResponseTaskDelay);
}

class SAMLDeviceAttestationTest : public SamlTest {
 public:
  SAMLDeviceAttestationTest() = default;

  SAMLDeviceAttestationTest(const SAMLDeviceAttestationTest&) = delete;
  SAMLDeviceAttestationTest& operator=(const SAMLDeviceAttestationTest&) =
      delete;

  void SetUpInProcessBrowserTestFixture() override;

 protected:
  void SetAllowedUrlsPolicy(const std::vector<std::string>& allowed_urls);

  chromeos::ScopedTestingCrosSettings settings_helper_;
  StubCrosSettingsProvider* settings_provider_ = nullptr;

  cryptohome::MockAsyncMethodCaller* mock_async_method_caller_ = nullptr;
  NiceMock<chromeos::attestation::MockAttestationFlow> mock_attestation_flow_;
  chromeos::ScopedStubInstallAttributes stub_install_attributes_;
};

void SAMLDeviceAttestationTest::SetUpInProcessBrowserTestFixture() {
  SamlTest::SetUpInProcessBrowserTestFixture();

  settings_provider_ = settings_helper_.device_settings();

  mock_async_method_caller_ = new NiceMock<cryptohome::MockAsyncMethodCaller>();
  mock_async_method_caller_->SetUp(/*success=*/true,
                                   cryptohome::MountError::MOUNT_ERROR_NONE);
  ON_CALL(*mock_async_method_caller_, TpmAttestationSignEnterpriseChallenge)
      .WillByDefault(WithArgs<6, 8>(Invoke(FakeEnterpriseChallenge)));

  // Ownership of mock_async_method_caller_ is transferred to
  // AsyncMethodCaller::InitializeForTesting.
  cryptohome::AsyncMethodCaller::InitializeForTesting(
      mock_async_method_caller_);

  ON_CALL(mock_attestation_flow_, GetCertificate)
      .WillByDefault(WithArgs<5>(Invoke(FakeGetCertificateCallbackTrue)));

  attestation::TpmChallengeKeyFactory::SetForTesting(
      std::make_unique<attestation::TpmChallengeKeyImpl>(
          &mock_attestation_flow_));

  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");
}

void SAMLDeviceAttestationTest::SetAllowedUrlsPolicy(
    const std::vector<std::string>& allowed_urls) {
  std::vector<base::Value> allowed_urls_values;
  for (const auto& url : allowed_urls) {
    allowed_urls_values.push_back(base::Value(url));
  }
  settings_provider_->Set(chromeos::kDeviceWebBasedAttestationAllowedUrls,
                          base::Value(std::move(allowed_urls_values)));
}

// Verify that device attestation is not available when
// DeviceWebBasedAttestationAllowedUrls policy is not set.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest, DefaultPolicy) {
  base::HistogramTester histogram_tester;

  // Leave policy unset.

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  ASSERT_FALSE(fake_saml_idp()->IsLastChallengeResponseExists());
  histogram_tester.ExpectUniqueSample(kSamlChallengeKeyHandlerResultMetric,
                                      attestation::TpmChallengeKeyResultCode::
                                          kDeviceWebBasedAttestationUrlError,
                                      1);
}

// Verify that device attestation is not available when
// DeviceWebBasedAttestationAllowedUrls policy is set to empty list of allowed
// URLs.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest, EmptyPolicy) {
  base::HistogramTester histogram_tester;
  SetAllowedUrlsPolicy({/* empty list */});

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  ASSERT_FALSE(fake_saml_idp()->IsLastChallengeResponseExists());
  histogram_tester.ExpectUniqueSample(kSamlChallengeKeyHandlerResultMetric,
                                      attestation::TpmChallengeKeyResultCode::
                                          kDeviceWebBasedAttestationUrlError,
                                      1);
}

// Verify that device attestation is not available when device is not enterprise
// enrolled.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest, NotEnterpriseEnrolledError) {
  base::HistogramTester histogram_tester;
  SetAllowedUrlsPolicy({"login.corp.example.com"});

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  ASSERT_FALSE(fake_saml_idp()->IsLastChallengeResponseExists());
  histogram_tester.ExpectUniqueSample(
      kSamlChallengeKeyHandlerResultMetric,
      attestation::TpmChallengeKeyResultCode::kNonEnterpriseDeviceError, 1);
}

// Verify that device attestation is not available when device attestation is
// not enabled.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest,
                       DeviceAttestationNotEnabledError) {
  base::HistogramTester histogram_tester;
  SetAllowedUrlsPolicy({"login.corp.example.com"});
  stub_install_attributes_.Get()->SetCloudManaged("google.com", "device_id");

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  ASSERT_FALSE(fake_saml_idp()->IsLastChallengeResponseExists());
  histogram_tester.ExpectUniqueSample(
      kSamlChallengeKeyHandlerResultMetric,
      attestation::TpmChallengeKeyResultCode::kDevicePolicyDisabledError, 1);
}

// Verify that device attestation works when all policies configured correctly.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest, Success) {
  base::HistogramTester histogram_tester;
  SetAllowedUrlsPolicy({"login.corp.example.com"});
  stub_install_attributes_.Get()->SetCloudManaged("google.com", "device_id");
  settings_provider_->SetBoolean(chromeos::kDeviceAttestationEnabled, true);

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  ASSERT_TRUE(fake_saml_idp()->IsLastChallengeResponseExists());
  ASSERT_EQ(fake_saml_idp()->GetLastChallengeResponse(),
            GetTpmResponseBase64());
  histogram_tester.ExpectUniqueSample(
      kSamlChallengeKeyHandlerResultMetric,
      attestation::TpmChallengeKeyResultCode::kSuccess, 1);
}

// Verify that device attestation is not available for URLs that are not in the
// allowed URLs list.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest, PolicyNoMatchError) {
  base::HistogramTester histogram_tester;
  SetAllowedUrlsPolicy({"example.com"});
  stub_install_attributes_.Get()->SetCloudManaged("google.com", "device_id");
  settings_provider_->SetBoolean(chromeos::kDeviceAttestationEnabled, true);

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  ASSERT_FALSE(fake_saml_idp()->IsLastChallengeResponseExists());
  histogram_tester.ExpectUniqueSample(kSamlChallengeKeyHandlerResultMetric,
                                      attestation::TpmChallengeKeyResultCode::
                                          kDeviceWebBasedAttestationUrlError,
                                      1);
}

// Verify that device attestation is available for URLs that match a pattern
// from allowed URLs list.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest, PolicyRegexSuccess) {
  base::HistogramTester histogram_tester;
  SetAllowedUrlsPolicy({"[*.]example.com"});
  stub_install_attributes_.Get()->SetCloudManaged("google.com", "device_id");
  settings_provider_->SetBoolean(chromeos::kDeviceAttestationEnabled, true);

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  ASSERT_TRUE(fake_saml_idp()->IsLastChallengeResponseExists());
  ASSERT_EQ(fake_saml_idp()->GetLastChallengeResponse(),
            GetTpmResponseBase64());
  histogram_tester.ExpectUniqueSample(
      kSamlChallengeKeyHandlerResultMetric,
      attestation::TpmChallengeKeyResultCode::kSuccess, 1);
}

// Verify that device attestation works in case of multiple items in allowed
// URLs list.
IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest, PolicyTwoEntriesSuccess) {
  base::HistogramTester histogram_tester;
  SetAllowedUrlsPolicy({"example2.com", "login.corp.example.com"});
  stub_install_attributes_.Get()->SetCloudManaged("google.com", "device_id");
  settings_provider_->SetBoolean(chromeos::kDeviceAttestationEnabled, true);

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  ASSERT_TRUE(fake_saml_idp()->IsLastChallengeResponseExists());
  ASSERT_EQ(fake_saml_idp()->GetLastChallengeResponse(),
            GetTpmResponseBase64());
  histogram_tester.ExpectUniqueSample(
      kSamlChallengeKeyHandlerResultMetric,
      attestation::TpmChallengeKeyResultCode::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(SAMLDeviceAttestationTest, TimeoutError) {
  base::HistogramTester histogram_tester;
  SetAllowedUrlsPolicy({"example2.com", "login.corp.example.com"});
  stub_install_attributes_.Get()->SetCloudManaged("google.com", "device_id");
  settings_provider_->SetBoolean(chromeos::kDeviceAttestationEnabled, true);

  ON_CALL(*mock_async_method_caller_, TpmAttestationSignEnterpriseChallenge)
      .WillByDefault(WithArgs<6, 8>(Invoke(FakeEnterpriseChallengeWithDelay)));

  auto handler = std::make_unique<SamlChallengeKeyHandler>();
  handler->SetTpmResponseTimeoutForTesting(kTimeoutTaskDelay);

  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetHandler<GaiaScreenHandler>()
      ->SetNextSamlChallengeKeyHandlerForTesting(std::move(handler));

  StartSamlAndWaitForIdpPageLoad(
      saml_test_users::kFourthUserCorpExampleTestEmail);

  if (Test::HasFailure()) {
    return;
  }

  ASSERT_FALSE(fake_saml_idp()->IsLastChallengeResponseExists());
  histogram_tester.ExpectUniqueSample(
      kSamlChallengeKeyHandlerResultMetric,
      attestation::TpmChallengeKeyResultCode::kTimeoutError, 1);
}

}  // namespace chromeos
