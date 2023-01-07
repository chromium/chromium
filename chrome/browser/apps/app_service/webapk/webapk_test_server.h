// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_TEST_SERVER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_TEST_SERVER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "components/webapk/webapk.pb.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace apps {

// A fake WebAPK server for use in tests.
class WebApkTestServer {
  using WebApkResponseBuilder =
      base::RepeatingCallback<std::unique_ptr<net::test_server::HttpResponse>(
          void)>;

 public:
  WebApkTestServer();
  ~WebApkTestServer();
  WebApkTestServer(const WebApkTestServer&) = delete;
  WebApkTestServer& operator=(const WebApkTestServer&) = delete;

  // Adds the WebAPK handler to the |server|, starts the |server|, and adds
  // the appropriate --webapk-server-url to the command line.
  bool SetUpAndStartServer(net::test_server::EmbeddedTestServer* server);

  // Configures how the server responds to future WebAPK requests. One of these
  // methods must be called before any requests are served.
  void RespondWithSuccess(const std::string& package_name);
  void RespondWithError();

  webapk::WebApk* last_webapk_request() { return last_webapk_request_.get(); }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleWebApkRequest(
      const net::test_server::HttpRequest& request);

  std::string response_package_name_;
  WebApkResponseBuilder webapk_response_builder_;
  std::unique_ptr<webapk::WebApk> last_webapk_request_;
};
}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_WEBAPK_WEBAPK_TEST_SERVER_H_
