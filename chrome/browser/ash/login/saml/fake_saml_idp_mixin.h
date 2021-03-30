// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SAML_FAKE_SAML_IDP_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_SAML_FAKE_SAML_IDP_MIXIN_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/optional.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace chromeos {

class FakeGaiaMixin;

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
  bool IsLastChallengeResponseExists() const;
  void AssertChallengeResponseMatchesTpmResponse() const;

  std::string GetIdpHost() const;
  std::string GetIdpDomain() const;
  GURL GetSamlPageUrl() const;
  GURL GetHttpSamlPageUrl() const;
  GURL GetSamlWithDeviceAttestationUrl() const;

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
    kLoginCheckDeviceAnswer
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
  BuildResponseForCheckDeviceAnswer(
      const net::test_server::HttpRequest& request,
      const GURL& request_url);

  std::unique_ptr<net::test_server::HttpResponse> BuildHTMLResponse(
      const std::string& html_template,
      const std::string& relay_state,
      const std::string& next_path) const;

  void SaveChallengeResponse(const std::string& response);
  void ClearChallengeResponse();

  FakeGaiaMixin* const gaia_mixin_;
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

  base::Optional<std::string> challenge_response_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SAML_FAKE_SAML_IDP_MIXIN_H_
