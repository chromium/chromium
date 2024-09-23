// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/fake_saml_idp_mixin.h"

#include <iterator>
#include <memory>
#include <optional>
#include <string>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

// The header that the server returns in a HTTP response to ask the client to
// authenticate.
constexpr char kAuthenticateResponseHeader[] = "WWW-Authenticate";

// The response header that the client sends to transfer HTTP auth credentials.
constexpr char kAuthorizationRequestHeader[] = "Authorization";

constexpr char kRelayState[] = "RelayState";

constexpr char kIdpDomain[] = "example.com";
constexpr char kIdPHost[] = "login.corp.example.com";
constexpr char kLinkedPageHost[] = "localhost";
constexpr char kIdpSsoProfile[] = "inboundSamlSsoProfiles/example";
constexpr char kSamlLoginPath[] = "SAML";
constexpr char kSamlLoginAuthPath[] = "SAMLAuth";
constexpr char kSamlLoginWithDeviceAttestationPath[] =
    "SAML-with-device-attestation";
constexpr char kSamlLoginWithDeviceTrustPath[] = "SAML-with-device-trust";
constexpr char kSamlLoginCheckDeviceAnswerPath[] = "SAML-check-device-answer";
constexpr char kLinkedPagePath[] = "linked";

// Must be equal to SAML_VERIFIED_ACCESS_RESPONSE_HEADER from
// chrome/browser/enterprise/connectors/device_trust/navigation_throttle.cc.
constexpr char kDeviceTrustHeader[] = "x-device-trust";
// Must be equal to SAML_VERIFIED_ACCESS_CHALLENGE_HEADER from saml_handler.js.
constexpr char kSamlVerifiedAccessChallengeHeader[] =
    "x-verified-access-challenge";
// Must be equal to SAML_VERIFIED_ACCESS_RESPONSE_HEADER from saml_handler.js.
constexpr char kSamlVerifiedAccessResponseHeader[] =
    "x-verified-access-challenge-response";

constexpr char kTpmChallenge[] = {0,   1,   2,      'c',    'h',
                                  'a', 'l', '\xFD', '\xFE', '\xFF'};

std::string GetTpmChallenge() {
  return std::string(kTpmChallenge, std::size(kTpmChallenge));
}

std::string GetTpmChallengeBase64() {
  return base::Base64Encode(
      base::as_bytes(base::span<const char>(kTpmChallenge)));
}

std::string GetTpmResponse() {
  return AttestationClient::Get()
      ->GetTestInterface()
      ->GetEnterpriseChallengeFakeSignature(GetTpmChallenge(),
                                            /*include_spkac=*/false);
}

std::string GetTpmResponseBase64() {
  const std::string response = GetTpmResponse();
  return base::Base64Encode(base::as_byte_span(response));
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

}  // namespace

FakeSamlIdpMixin::FakeSamlIdpMixin(InProcessBrowserTestMixinHost* host,
                                   FakeGaiaMixin* gaia_mixin)
    : InProcessBrowserTestMixin(host), gaia_mixin_(gaia_mixin) {
  saml_server_.RegisterRequestHandler(base::BindRepeating(
      &FakeSamlIdpMixin::HandleRequest, base::Unretained(this)));
  saml_http_server_.RegisterRequestHandler(base::BindRepeating(
      &FakeSamlIdpMixin::HandleRequest, base::Unretained(this)));
}

FakeSamlIdpMixin::~FakeSamlIdpMixin() = default;

void FakeSamlIdpMixin::SetUpCommandLine(base::CommandLine* command_line) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  // NOTE: Ideally testdata would all be in chromeos/login, to match the test.
  html_template_dir_ = test_data_dir.Append("login");
  // saml_with_password_attributes.xml is also used by cross-platform
  // gaia_auth_host WebUI tests. Since cross-platform WebUI shouldn't depend on
  // Ash/ChromeOS test data, the file is located in the gaia_auth_host test data
  // directory.
  saml_response_dir_ = test_data_dir.Append("webui").Append("gaia_auth_host");

  {
    base::ScopedAllowBlockingForTesting allow_io;
    std::string fake_saml_continue_response;
    EXPECT_TRUE(base::ReadFileToString(
        html_template_dir_.Append("gaia_finish_after_saml.html"),
        &fake_saml_continue_response));
    gaia_mixin_->fake_gaia()->SetFakeSamlContinueResponse(
        fake_saml_continue_response);
  }

  ASSERT_TRUE(saml_server_.Start());
  ASSERT_TRUE(saml_http_server_.Start());
}

void FakeSamlIdpMixin::SetUpOnMainThread() {
  gaia_mixin_->fake_gaia()->RegisterSamlDomainRedirectUrl(GetIdpDomain(),
                                                          GetSamlPageUrl());
  gaia_mixin_->fake_gaia()->RegisterSamlSsoProfileRedirectUrl(
      GetIdpSsoProfile(), GetSamlPageUrl());
}

void FakeSamlIdpMixin::SetLoginHTMLTemplate(const std::string& template_file) {
  base::ScopedAllowBlockingForTesting allow_io;
  EXPECT_TRUE(base::ReadFileToString(html_template_dir_.Append(template_file),
                                     &login_html_template_));
}

void FakeSamlIdpMixin::SetLoginAuthHTMLTemplate(
    const std::string& template_file) {
  base::ScopedAllowBlockingForTesting allow_io;
  EXPECT_TRUE(base::ReadFileToString(html_template_dir_.Append(template_file),
                                     &login_auth_html_template_));
}

void FakeSamlIdpMixin::SetRefreshURL(const GURL& refresh_url) {
  refresh_url_ = refresh_url;
}

void FakeSamlIdpMixin::SetCookieValue(const std::string& cookie_value) {
  cookie_value_ = cookie_value;
}

void FakeSamlIdpMixin::SetRequireHttpBasicAuth(bool require_http_basic_auth) {
  require_http_basic_auth_ = require_http_basic_auth;
}

void FakeSamlIdpMixin::SetSamlResponseFile(const std::string& xml_file) {
  base::ScopedAllowBlockingForTesting allow_io;
  EXPECT_TRUE(base::ReadFileToString(saml_response_dir_.Append(xml_file),
                                     &saml_response_));
  saml_response_ = base::Base64Encode(saml_response_);
}

bool FakeSamlIdpMixin::DeviceTrustHeaderRecieved() const {
  return device_trust_header_recieved_;
}

bool FakeSamlIdpMixin::IsLastChallengeResponseExists() const {
  return challenge_response_.has_value();
}

bool FakeSamlIdpMixin::IsLastChallengeResponseError() const {
  return error_challenge_response_.has_value();
}

int FakeSamlIdpMixin::GetChallengeResponseCount() const {
  return challenge_response_count_;
}

void FakeSamlIdpMixin::AssertChallengeResponseMatchesTpmResponse() const {
  ASSERT_EQ(challenge_response_.value(), GetTpmResponseBase64());
}

std::string FakeSamlIdpMixin::GetIdpHost() const {
  return kIdPHost;
}

std::string FakeSamlIdpMixin::GetIdpDomain() const {
  return kIdpDomain;
}

std::string FakeSamlIdpMixin::GetIdpSsoProfile() const {
  return kIdpSsoProfile;
}

GURL FakeSamlIdpMixin::GetSamlPageUrl() const {
  return saml_server_.GetURL(kIdPHost, std::string("/") + kSamlLoginPath);
}

GURL FakeSamlIdpMixin::GetHttpSamlPageUrl() const {
  return saml_http_server_.GetURL(kIdPHost, std::string("/") + kSamlLoginPath);
}

GURL FakeSamlIdpMixin::GetSamlWithDeviceAttestationUrl() const {
  return saml_server_.GetURL(
      kIdPHost, std::string("/") + kSamlLoginWithDeviceAttestationPath);
}

GURL FakeSamlIdpMixin::GetSamlWithDeviceTrustUrl() const {
  return saml_server_.GetURL(kIdPHost,
                             std::string("/") + kSamlLoginWithDeviceTrustPath);
}

GURL FakeSamlIdpMixin::GetSamlAuthPageUrl() const {
  return saml_server_.GetURL(kIdPHost, std::string("/") + kSamlLoginAuthPath);
}

GURL FakeSamlIdpMixin::GetSamlWithCheckDeviceAnswerUrl() const {
  return saml_server_.GetURL(
      kIdPHost, std::string("/") + kSamlLoginCheckDeviceAnswerPath);
}

GURL FakeSamlIdpMixin::GetLinkedPageUrl() const {
  return saml_server_.GetURL(kLinkedPageHost,
                             std::string("/") + kLinkedPagePath);
}

std::unique_ptr<net::test_server::HttpResponse> FakeSamlIdpMixin::HandleRequest(
    const net::test_server::HttpRequest& request) {
  GURL request_url = request.GetURL();
  const RequestType request_type = ParseRequestTypeFromRequestPath(request_url);

  if (request_type == RequestType::kUnknown) {
    // Ignore this request.
    return nullptr;
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
    case RequestType::kLoginWithDeviceTrust:
      return BuildResponseForLoginWithDeviceTrust(request, request_url);
    case RequestType::kLoginCheckDeviceAnswer:
      return BuildResponseForCheckDeviceAnswer(request, request_url);
    case RequestType::kLinkedPage:
      return BuildResponseForLinkedPage(request, request_url);
    case RequestType::kUnknown:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

FakeSamlIdpMixin::RequestType FakeSamlIdpMixin::ParseRequestTypeFromRequestPath(
    const GURL& request_url) const {
  std::string request_path = request_url.path();

  if (request_path == GetSamlPageUrl().path())
    return RequestType::kLogin;
  if (request_path == GetSamlAuthPageUrl().path())
    return RequestType::kLoginAuth;
  if (request_path == GetSamlWithDeviceAttestationUrl().path())
    return RequestType::kLoginWithDeviceAttestation;
  if (request_path == GetSamlWithDeviceTrustUrl().path())
    return RequestType::kLoginWithDeviceTrust;
  if (request_path == GetSamlWithCheckDeviceAnswerUrl().path())
    return RequestType::kLoginCheckDeviceAnswer;
  if (request_path == GetLinkedPageUrl().path()) {
    return RequestType::kLinkedPage;
  }

  return RequestType::kUnknown;
}

std::unique_ptr<HttpResponse> FakeSamlIdpMixin::BuildResponseForLogin(
    const HttpRequest& request,
    const GURL& request_url) const {
  const std::string relay_state = GetRelayState(request);
  return BuildHTMLResponse(login_html_template_, relay_state,
                           GetSamlAuthPageUrl().path());
}

std::unique_ptr<HttpResponse> FakeSamlIdpMixin::BuildResponseForLoginAuth(
    const HttpRequest& request,
    const GURL& request_url) {
  const std::string relay_state = GetRelayState(request);
  GURL redirect_url = gaia_mixin_->GetFakeGaiaURL("/SSO");

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
FakeSamlIdpMixin::BuildResponseForLoginWithDeviceAttestation(
    const HttpRequest& request,
    const GURL& request_url) const {
  std::string relay_state = GetRelayState(request);

  GURL redirect_url = GetSamlWithCheckDeviceAnswerUrl();
  redirect_url =
      net::AppendQueryParameter(redirect_url, kRelayState, relay_state);

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url.spec());
  http_response->AddCustomHeader(kSamlVerifiedAccessChallengeHeader,
                                 GetTpmChallengeBase64());

  return http_response;
}

std::unique_ptr<HttpResponse>
FakeSamlIdpMixin::BuildResponseForLoginWithDeviceTrust(
    const HttpRequest& request,
    const GURL& request_url) {
  std::string relay_state = GetRelayState(request);

  device_trust_header_recieved_ =
      base::Contains(request.headers, kDeviceTrustHeader);

  GURL redirect_url = GetSamlWithCheckDeviceAnswerUrl();
  redirect_url =
      net::AppendQueryParameter(redirect_url, kRelayState, relay_state);

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url.spec());

  // Device Trust only supports V2 challenges, which are formatted as a JSON
  // object with only one "challenge" property (containing the value from V1).
  // TODO(b:253427534): Update code to handle V1 challenges.
  base::Value::Dict challenge_value;
  challenge_value.Set("challenge", GetTpmChallengeBase64());
  std::string challenge_json_value;
  EXPECT_TRUE(base::JSONWriter::Write(challenge_value, &challenge_json_value));

  http_response->AddCustomHeader(kSamlVerifiedAccessChallengeHeader,
                                 challenge_json_value);
  return http_response;
}

std::unique_ptr<HttpResponse>
FakeSamlIdpMixin::BuildResponseForCheckDeviceAnswer(const HttpRequest& request,
                                                    const GURL& request_url) {
  std::string relay_state = GetRelayState(request);

  auto iter = request.headers.find(kSamlVerifiedAccessResponseHeader);
  if (iter != request.headers.end()) {
    SaveChallengeResponse(/*response=*/iter->second);
  } else {
    ClearChallengeResponse();
  }

  GURL redirect_url =
      net::AppendQueryParameter(GetSamlPageUrl(), kRelayState, relay_state);

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url.spec());
  return http_response;
}

std::unique_ptr<HttpResponse> FakeSamlIdpMixin::BuildResponseForLinkedPage(
    const HttpRequest& request,
    const GURL& request_url) const {
  return BuildHTMLResponse(login_html_template_, "linked",
                           GetLinkedPageUrl().path());
}

std::unique_ptr<net::test_server::HttpResponse>
FakeSamlIdpMixin::BuildHTMLResponse(const std::string& html_template,
                                    const std::string& relay_state,
                                    const std::string& next_path) const {
  std::string response_html = html_template;
  base::ReplaceSubstringsAfterOffset(&response_html, 0, "$RelayState",
                                     relay_state);
  base::ReplaceSubstringsAfterOffset(&response_html, 0, "$Post", next_path);
  base::ReplaceSubstringsAfterOffset(&response_html, 0, "$Refresh",
                                     refresh_url_.spec());

  return BuildHTMLResponse(response_html);
}

std::unique_ptr<net::test_server::HttpResponse>
FakeSamlIdpMixin::BuildHTMLResponse(const std::string& response_html) const {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(response_html);
  http_response->set_content_type("text/html");

  return http_response;
}

void FakeSamlIdpMixin::SaveChallengeResponse(const std::string& response) {
  EXPECT_EQ(challenge_response_, std::nullopt);
  auto parsed_value = base::JSONReader::Read(
      response, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);

  if (!parsed_value || !parsed_value->is_dict()) {
    // Most likely given a V1, no need to try parsing the values out.
    challenge_response_ = response;
    return;
  }

  const std::string* challenge_response_string =
      parsed_value->GetDict().FindString("challengeResponse");
  const std::string* error_string = parsed_value->GetDict().FindString("error");

  // Only one of those values should be set.
  EXPECT_NE(!!challenge_response_string, !!error_string);
  challenge_response_count_++;

  if (challenge_response_string) {
    challenge_response_ = response;
  } else {
    error_challenge_response_ = response;
  }
}

void FakeSamlIdpMixin::ClearChallengeResponse() {
  challenge_response_.reset();
  error_challenge_response_.reset();
}

}  // namespace ash
