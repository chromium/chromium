// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_TEST_APP_INSTALL_SERVER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_TEST_APP_INSTALL_SERVER_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "components/services/app_service/public/cpp/package_id.h"
#include "components/webapps/common/web_app_id.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace apps {

class TestAppInstallServer {
 public:
  TestAppInstallServer();
  ~TestAppInstallServer();

  [[nodiscard]] bool SetUp();

  struct SetupIds {
    webapps::AppId app_id;
    PackageId package_id;
  };

  // Sets up a response for a test web app installation, and returns the Package
  // ID and App ID for the app.
  SetupIds SetupDefaultServerResponse();

  // Sets up a response for the app with `package_id` to install through a given
  // URL.
  void SetUpInstallUrlResponse(PackageId package_id, GURL install_url);

  // Set up an arbitrary response to the App Install endpoint.
  void SetUpResponse(std::string_view package_id, std::string_view response);

  GURL GetUrl(std::string_view relative_url) {
    return server_.GetURL(relative_url);
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  net::EmbeddedTestServer server_;
  std::map<std::string, std::string> response_map_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_TEST_APP_INSTALL_SERVER_H_
