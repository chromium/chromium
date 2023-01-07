// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/webapk/webapk_test_server.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/common/chrome_switches.h"

namespace {
constexpr char kServerPath[] = "/webapk";
constexpr char kToken[] = "opaque token";

std::unique_ptr<net::test_server::HttpResponse> BuildValidWebApkResponse(
    std::string package_name) {
  auto webapk_response = std::make_unique<webapk::WebApkResponse>();
  webapk_response->set_package_name(std::move(package_name));
  webapk_response->set_version("1");
  webapk_response->set_token(kToken);

  std::string response_content;
  webapk_response->SerializeToString(&response_content);

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content(response_content);

  return response;
}

std::unique_ptr<net::test_server::HttpResponse> BuildFailedResponse() {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_BAD_REQUEST);
  return response;
}

}  // namespace

namespace apps {

WebApkTestServer::WebApkTestServer() = default;
WebApkTestServer::~WebApkTestServer() = default;

bool WebApkTestServer::SetUpAndStartServer(
    net::test_server::EmbeddedTestServer* server) {
  server->RegisterRequestHandler(base::BindRepeating(
      &WebApkTestServer::HandleWebApkRequest, base::Unretained(this)));
  bool result = server->Start();
  if (result) {
    GURL server_url = server->GetURL(kServerPath);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kWebApkServerUrl, server_url.spec());
  }

  return result;
}

void WebApkTestServer::RespondWithSuccess(const std::string& package_name) {
  webapk_response_builder_ =
      base::BindRepeating(&BuildValidWebApkResponse, package_name);
}

void WebApkTestServer::RespondWithError() {
  webapk_response_builder_ = base::BindRepeating(&BuildFailedResponse);
}

std::unique_ptr<net::test_server::HttpResponse>
WebApkTestServer::HandleWebApkRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url == kServerPath) {
    last_webapk_request_ = std::make_unique<webapk::WebApk>();
    last_webapk_request_->ParseFromString(request.content);
    return webapk_response_builder_.Run();
  }

  return nullptr;
}
}  // namespace apps
