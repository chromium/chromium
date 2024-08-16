// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/test_app_install_server.h"

#include "chrome/browser/apps/app_service/app_install/app_install.pb.h"
#include "chrome/browser/chromeos/crosapi/test_util.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

TestAppInstallServer::TestAppInstallServer() = default;
TestAppInstallServer::~TestAppInstallServer() = default;

bool TestAppInstallServer::SetUp() {
  server_.RegisterRequestHandler(base::BindRepeating(
      &TestAppInstallServer::HandleRequest, base::Unretained(this)));
  server_.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));

  if (!server_.Start()) {
    return false;
  }

  std::string server_url = server_.GetURL("/").spec();

  crosapi::mojom::TestControllerAsyncWaiter(crosapi::GetTestController())
      .SetAlmanacEndpointUrlForTesting(server_url);

  return true;
}

TestAppInstallServer::SetupIds TestAppInstallServer::SetUpWebAppResponse() {
  return SetUpWebAppInstallInternal(PackageType::kWeb);
}

TestAppInstallServer::SetupIds TestAppInstallServer::SetUpWebsiteResponse() {
  return SetUpWebAppInstallInternal(PackageType::kWebsite);
}

void TestAppInstallServer::SetUpInstallUrlResponse(PackageId package_id,
                                                   GURL install_url) {
  proto::AppInstallResponse response;
  proto::AppInstallResponse_AppInstance& instance =
      *response.mutable_app_instance();
  instance.set_package_id(package_id.ToString());
  instance.set_name("Test");
  instance.set_install_url(install_url.spec());
  SetUpResponse(package_id.ToString(), response);
}

void TestAppInstallServer::SetUpResponse(
    std::string_view package_id,
    const apps::proto::AppInstallResponse& response) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(response.SerializeAsString());
  response_map_[std::string(package_id)] = std::move(http_response);
}

void TestAppInstallServer::SetUpResponseCode(
    PackageId package_id,
    net::HttpStatusCode response_code) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(response_code);
  response_map_[package_id.ToString()] = std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse>
TestAppInstallServer::HandleRequest(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL() != server_.GetURL("/v1/app-install")) {
    return nullptr;
  }

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  proto::AppInstallRequest app_request;
  if (!app_request.ParseFromString(request.content)) {
    http_response->set_code(net::HTTP_BAD_REQUEST);
    return std::move(http_response);
  }

  auto it = response_map_.find(app_request.package_id());
  if (it == response_map_.end()) {
    http_response->set_code(net::HTTP_NOT_FOUND);
    return std::move(http_response);
  }

  return std::move(it->second);
}

TestAppInstallServer::SetupIds TestAppInstallServer::SetUpWebAppInstallInternal(
    PackageType package_type) {
  GURL start_url = server_.GetURL("/web_apps/basic.html");
  GURL manifest_url = server_.GetURL("/web_apps/basic.json");
  webapps::ManifestId manifest_id = start_url;
  webapps::AppId app_id = web_app::GenerateAppIdFromManifestId(manifest_id);
  PackageId package_id(package_type, manifest_id.spec());

  // Set Almanac server payload.
  proto::AppInstallResponse response;
  proto::AppInstallResponse_AppInstance& instance =
      *response.mutable_app_instance();
  instance.set_package_id(package_id.ToString());
  instance.set_name("Test");
  proto::AppInstallResponse_WebExtras& web_extras =
      *instance.mutable_web_extras();
  web_extras.set_document_url(start_url.spec());
  web_extras.set_original_manifest_url(manifest_url.spec());
  web_extras.set_scs_url(manifest_url.spec());
  SetUpResponse(package_id.ToString(), response);

  return {app_id, package_id};
}

}  // namespace apps
