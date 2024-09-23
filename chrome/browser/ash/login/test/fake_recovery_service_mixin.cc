// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/fake_recovery_service_mixin.h"

#include "ash/constants/ash_switches.h"
#include "base/functional/bind.h"
#include "chromeos/ash/components/login/auth/recovery/service_constants.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

}  // namespace

FakeRecoveryServiceMixin::FakeRecoveryServiceMixin(
    InProcessBrowserTestMixinHost* host,
    net::EmbeddedTestServer* test_server)
    : InProcessBrowserTestMixin(host), test_server_(test_server) {}

FakeRecoveryServiceMixin::~FakeRecoveryServiceMixin() = default;

void FakeRecoveryServiceMixin::SetUp() {
  test_server_->RegisterRequestHandler(base::BindRepeating(
      &FakeRecoveryServiceMixin::HandleRequest, base::Unretained(this)));
}

void FakeRecoveryServiceMixin::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Retrieve the URL from the embedded test server and override the recovery
  // service URL.
  command_line->AppendSwitchASCII(switches::kCryptohomeRecoveryServiceBaseUrl,
                                  test_server_->base_url().spec());
}

void FakeRecoveryServiceMixin::SetErrorResponse(
    std::string request_path,
    net::HttpStatusCode http_status_code) {
  error_responses_[request_path] = http_status_code;
}

std::unique_ptr<HttpResponse> FakeRecoveryServiceMixin::HandleRequest(
    const HttpRequest& request) {
  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
  const std::string request_path = request_url.path();
  std::unique_ptr<BasicHttpResponse> http_response =
      std::make_unique<BasicHttpResponse>();

  ErrorResponseMap::const_iterator error_response =
      error_responses_.find(request_path);
  if (error_response != error_responses_.end() &&
      error_response->second != net::HTTP_OK) {
    http_response->set_code(error_response->second);
    return std::move(http_response);
  }

  if (request_path == GetRecoveryServiceReauthTokenURL().path()) {
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("application/json");
    http_response->set_content(
        R"({
          "encodedReauthRequestToken": "fake-reauth-request-token"
        })");
    return std::move(http_response);
  } else if (request_path == GetRecoveryServiceEpochURL().path()) {
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("application/json");
    http_response->set_content(
        R"({
          "epochPubKey": "fake-epoch-pub-key",
          "epochMetaData": "fake-epoch-metadata",
        })");
    return std::move(http_response);
  } else if (request_path == GetRecoveryServiceMediateURL().path()) {
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("application/json");
    http_response->set_content(
        R"({
          "cborCryptoRecoveryResponse": "fake-recovery-response",
        })");
    return std::move(http_response);
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

}  // namespace ash
