// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_mode/test/fake_origin_test_server_mixin.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/network/public/cpp/network_switches.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash {

using net::test_server::EmbeddedTestServer;

namespace {

EmbeddedTestServer::Type SchemeTypeOf(const GURL& origin) {
  CHECK(origin.SchemeIs("http") || origin.SchemeIs("https"));
  return origin.SchemeIs("http") ? EmbeddedTestServer::TYPE_HTTP
                                 : EmbeddedTestServer::TYPE_HTTPS;
}

// Returns "MAP example.com 127.0.0.1:1234", where "example.com" is the host in
// `origin` and "127.0.0.1:1234" is the host and port of `server`.
std::string MapOriginToServer(const GURL& origin,
                              const EmbeddedTestServer& server) {
  return base::StrCat(
      {"MAP ", origin.host_piece(), " ", server.host_port_pair().ToString()});
}

void LogUrl(const net::test_server::HttpRequest& request) {
  LOG(INFO) << "Received request for url '" << request.GetURL() << "'";
}

}  // namespace

FakeOriginTestServerMixin::FakeOriginTestServerMixin(
    InProcessBrowserTestMixinHost* host,
    GURL origin,
    base::FilePath::StringPieceType path_to_be_served)
    : InProcessBrowserTestMixin(host),
      origin_(std::move(origin)),
      path_to_be_served_(path_to_be_served),
      server_(SchemeTypeOf(origin_)) {
  CHECK(origin_.is_valid());
  CHECK(origin_.has_scheme());
  CHECK(origin_.has_host());
  CHECK(!origin_.has_path() || origin_.path_piece() == "/");
  CHECK(!origin_.has_query());
  CHECK(!origin_.has_username());
  CHECK(!origin_.has_password());
  CHECK(!path_to_be_served_.value().starts_with("/"))
      << "path_to_be_served_ must be relative to Chrome's src/";

  // Generate SSL certificates for `origin_` on HTTPS.
  if (SchemeTypeOf(origin_) == EmbeddedTestServer::TYPE_HTTPS) {
    server_.SetCertHostnames({origin_.host()});
  }
}

FakeOriginTestServerMixin::~FakeOriginTestServerMixin() = default;

void FakeOriginTestServerMixin::SetUp() {
  auto src_path = base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT);
  server_.ServeFilesFromDirectory(src_path.Append(path_to_be_served_));
  server_.RegisterRequestMonitor(base::BindRepeating(&LogUrl));
  CHECK(server_.InitializeAndListen());
}

void FakeOriginTestServerMixin::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Set the `kHostResolverRules` switch so Chrome forwards request made to
  // `origin_` to the given `server_`.
  auto rule = MapOriginToServer(origin_, server_);
  command_line->AppendSwitchASCII(network::switches::kHostResolverRules, rule);
  LOG(INFO) << "Configured host resolver rule '" << rule << "'";
}

void FakeOriginTestServerMixin::SetUpOnMainThread() {
  server_handle_ = server_.StartAcceptingConnectionsAndReturnHandle();
}

GURL FakeOriginTestServerMixin::GetUrl(std::string_view url_suffix) const {
  CHECK(url_suffix.starts_with("/"))
      << "URL suffix must start with '/': " << url_suffix;
  return origin_.Resolve(url_suffix);
}

}  // namespace ash
