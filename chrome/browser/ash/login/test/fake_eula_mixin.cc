// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/fake_eula_mixin.h"

#include "ash/constants/ash_switches.h"
#include "base/functional/bind.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

constexpr char kFakeOnlineEulaPath[] = "/intl/en-US/chrome/eula_text.html";

}  // namespace

const char* FakeEulaMixin::kFakeOnlineEula = "No obligations at all";
const char* FakeEulaMixin::kOfflineEULAWarning =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    // See IDS_TERMS_HTML for the complete text.
    "Google Chrome and ChromeOS Additional Terms of Service";
#else
    // Placeholder text in terms_chromium.html.
    "In official builds this space will show the terms of service.";
#endif

FakeEulaMixin::FakeEulaMixin(InProcessBrowserTestMixinHost* host,
                             net::EmbeddedTestServer* test_server)
    : InProcessBrowserTestMixin(host), test_server_(test_server) {}

FakeEulaMixin::~FakeEulaMixin() = default;

void FakeEulaMixin::SetUp() {
  test_server_->RegisterRequestHandler(base::BindRepeating(
      &FakeEulaMixin::HandleRequest, base::Unretained(this)));
}

void FakeEulaMixin::SetUpCommandLine(base::CommandLine* command_line) {
  // Retrieve the URL from the embedded test server and override EULA URL.
  std::string fake_eula_url =
      test_server_->base_url().Resolve(kFakeOnlineEulaPath).spec();
  command_line->AppendSwitchASCII(switches::kOobeEulaUrlForTests,
                                  fake_eula_url);
}

std::unique_ptr<HttpResponse> FakeEulaMixin::HandleRequest(
    const HttpRequest& request) {
  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
  const std::string request_path = request_url.path();
  if (!base::EndsWith(request_path, "/eula_text.html",
                      base::CompareCase::SENSITIVE)) {
    return nullptr;
  }

  std::unique_ptr<BasicHttpResponse> http_response =
      std::make_unique<BasicHttpResponse>();

  if (force_http_unavailable_) {
    http_response->set_code(net::HTTP_SERVICE_UNAVAILABLE);
  } else {
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    http_response->set_content(kFakeOnlineEula);
  }

  return std::move(http_response);
}

}  // namespace ash
