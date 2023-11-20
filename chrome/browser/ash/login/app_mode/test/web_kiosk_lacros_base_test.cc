// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/test/web_kiosk_lacros_base_test.h"

#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace ash {

namespace {
std::unique_ptr<net::test_server::HttpResponse> ServeSimpleHtmlPage(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content(
      "<!DOCTYPE html>"
      "<html lang=\"en\">"
      "<head><title>Test Page</title></head>"
      "<body>A simple kiosk web page.</body>"
      "</html>");
  return response;
}

}  // namespace

void WebKioskLacrosBaseTest::SetUpOnMainThread() {
  InitializeWebAppServer();
  WebKioskBaseTest::SetUpOnMainThread();
}

void WebKioskLacrosBaseTest::SetUpInProcessBrowserTestFixture() {
  if (kiosk_ash_starter_.HasLacrosArgument()) {
    kiosk_ash_starter_.PrepareEnvironmentForKioskLacros();
  }
  WebKioskBaseTest::SetUpInProcessBrowserTestFixture();
}

void WebKioskLacrosBaseTest::PreRunTestOnMainThread() {
  if (kiosk_ash_starter_.HasLacrosArgument()) {
    kiosk_ash_starter_.SetLacrosAvailabilityPolicy();
    kiosk_ash_starter_.SetUpBrowserManager();
  }
  WebKioskBaseTest::PreRunTestOnMainThread();
}

void WebKioskLacrosBaseTest::InitializeWebAppServer() {
  web_app_server_.RegisterRequestHandler(
      base::BindRepeating(&ServeSimpleHtmlPage));
  ASSERT_TRUE(web_app_server_handle_ = web_app_server_.StartAndReturnHandle());
  SetAppInstallUrl(web_app_server_.base_url().spec());
}

}  // namespace ash
