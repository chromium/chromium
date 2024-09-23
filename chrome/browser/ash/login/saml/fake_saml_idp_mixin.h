// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SAML_FAKE_SAML_IDP_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_SAML_FAKE_SAML_IDP_MIXIN_H_

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

class FakeGaiaMixin;

namespace base {
class CommandLine;
}

namespace ash {

class FakeSamlIdpMixin final : public InProcessBrowserTestMixin {
 public:
  FakeSamlIdpMixin(InProcessBrowserTestMixinHost* host,
                   FakeGaiaMixin* gaia_mixin);

  FakeSamlIdpMixin(const FakeSamlIdpMixin&) = delete;
  FakeSamlIdpMixin& operator=(const FakeSamlIdpMixin&) = delete;
  ~FakeSamlIdpMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  // TODO(crbug.com/1177416) - Improve comments
  void SetLoginHTMLTemplate(const std::string& template_file);
  void SetLoginAuthHTMLTemplate(const std::string& template_file);
  void SetRefreshURL(const GURL& refresh_url);
  void SetCookieValue(const std::string& cookie_value);
  void SetRequireHttpBasicAuth(bool require_http_basic_auth);
  void SetSamlResponseFile(const std::string& xml_file);
  bool DeviceTrustHeaderRecieved() const;
  int GetChallengeResponseCount() const;
  void AssertChallengeResponseMatchesTpmResponse() const;

  // Returns true if a successful challenge response was captured.
  bool IsLastChallengeResponseExists() const;

  // Returns true if a failed challenge response was captured.
  bool IsLastChallengeResponseError() const;

  std::string GetIdpHost() const;
  std::string GetIdpDomain() const;
  std::string GetIdpSsoProfile() const;
  GURL GetSamlPageUrl() const;
  GURL GetHttpSamlPageUrl() const;
  GURL GetSamlWithDeviceAttestationUrl() const;
  GURL GetSamlWithDeviceTrustUrl() const;
  GURL GetLinkedPageUrl() const;

 private:
  GURL GetSamlAuthPageUrl() const;
  GURL GetSamlWithCheckDeviceAnswerUrl() const;

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  enum class RequestType {
    // Not a known request.
    kUnknown,
    kLogin,
    kLoginAuth,
    kLoginWithDeviceAttestation,
    kLoginCheckDeviceAnswer,
    kLoginWithDeviceTrust,
    kLinkedPage
  };

  // Returns the RequestType that corresponds to `url`, or RequestType::Unknown
  // if this is not a request for the FakeSamlIdp.
  RequestType ParseRequestTypeFromRequestPath(const GURL& request_url) const;

  std::unique_ptr<net::test_server::HttpResponse> BuildResponseForLogin(
      const net::test_server::HttpRequest& request,
      const GURL& request_url) const;
  std::unique_ptr<net::test_server::HttpResponse> BuildResponseForLoginAuth(
      const net::test_server::HttpRequest& request,
      const GURL& request_url);
  std::unique_ptr<net::test_server::HttpResponse>
  BuildResponseForLoginWithDeviceAttestation(
      const net::test_server::HttpRequest& request,
      const GURL& request_url) const;
  std::unique_ptr<net::test_server::HttpResponse>
  BuildResponseForLoginWithDeviceTrust(
      const net::test_server::HttpRequest& request,
      const GURL& request_url);
  std::unique_ptr<net::test_server::HttpResponse>
  BuildResponseForCheckDeviceAnswer(
      const net::test_server::HttpRequest& request,
      const GURL& request_url);
  std::unique_ptr<net::test_server::HttpResponse> BuildResponseForLinkedPage(
      const net::test_server::HttpRequest& request,
      const GURL& request_url) const;

  std::unique_ptr<net::test_server::HttpResponse> BuildHTMLResponse(
      const std::string& html_template,
      const std::string& relay_state,
      const std::string& next_path) const;

  std::unique_ptr<net::test_server::HttpResponse> BuildHTMLResponse(
      const std::string& response_html) const;

  void SaveChallengeResponse(const std::string& response);
  void ClearChallengeResponse();

  const raw_ptr<FakeGaiaMixin> gaia_mixin_;
  net::EmbeddedTestServer saml_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  net::EmbeddedTestServer saml_http_server_{net::EmbeddedTestServer::TYPE_HTTP};

  base::FilePath html_template_dir_;
  base::FilePath saml_response_dir_;
  GURL refresh_url_;
  std::string login_html_template_;
  std::string login_auth_html_template_;
  std::string cookie_value_;
  std::string saml_response_{"fake_response"};

  bool require_http_basic_auth_ = false;

  bool device_trust_header_recieved_ = false;
  int challenge_response_count_ = 0;
  std::optional<std::string> challenge_response_;
  std::optional<std::string> error_challenge_response_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SAML_FAKE_SAML_IDP_MIXIN_H_
