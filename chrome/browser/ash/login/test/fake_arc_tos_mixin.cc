// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/fake_arc_tos_mixin.h"

#include "ash/constants/ash_switches.h"
#include "base/functional/bind.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace ash {
namespace {

using ::net::test_server::BasicHttpResponse;

constexpr char kArcTosPath[] = "/about/play-terms/embedded/";
constexpr char kPrivacyPolicyPath[] = "/policies/privacy/embedded";

}  // namespace

FakeArcTosMixin::FakeArcTosMixin(InProcessBrowserTestMixinHost* host,
                                 net::EmbeddedTestServer* test_server)
    : InProcessBrowserTestMixin(host), test_server_(test_server) {}

FakeArcTosMixin::~FakeArcTosMixin() = default;

void FakeArcTosMixin::SetUp() {
  test_server_->RegisterRequestHandler(base::BindRepeating(
      &FakeArcTosMixin::HandleRequest, base::Unretained(this)));
}

void FakeArcTosMixin::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(switches::kArcTosHostForTests,
                                  TestServerBaseUrl() + kArcTosPath);
  command_line->AppendSwitchASCII(switches::kPrivacyPolicyHostForTests,
                                  TestServerBaseUrl() + kPrivacyPolicyPath);
}

// Returns the base URL of the embedded test server.
// The string will have the format "http://127.0.0.1:${PORT_NUMBER}" where
// PORT_NUMBER is a randomly assigned port number.
std::string FakeArcTosMixin::TestServerBaseUrl() {
  return std::string(base::TrimString(
      test_server_->base_url().DeprecatedGetOriginAsURL().spec(), "/",
      base::TrimPositions::TRIM_TRAILING));
}

std::unique_ptr<HttpResponse> FakeArcTosMixin::HandleRequest(
    const HttpRequest& request) {
  if (request.relative_url != kArcTosPath &&
      request.relative_url != kPrivacyPolicyPath) {
    return nullptr;
  }

  std::string content;
  if (request.relative_url == kArcTosPath) {
    content = kArcTosContent;
  } else {
    content = kPrivacyPolicyContent;
  }

  std::unique_ptr<BasicHttpResponse> http_response =
      std::make_unique<BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("text/html");
  http_response->set_content(content);
  return std::move(http_response);
}

}  // namespace ash
