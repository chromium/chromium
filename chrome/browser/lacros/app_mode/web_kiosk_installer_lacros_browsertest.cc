// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_registration_task.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/origin.h"

namespace web_app {

namespace {

class WebKioskInstallerLacrosBrowserTest : public WebAppBrowserTestBase {
 public:
  std::unique_ptr<net::test_server::HttpResponse> SimulateRedirectHandler(
      const net::test_server::HttpRequest& request) {
    if (!simulate_redirect_) {
      // Fall back to default handlers.
      return nullptr;
    }

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    if (base::Contains(request.GetURL().spec(), "redirected")) {
      response->set_code(net::HTTP_MOVED_PERMANENTLY);
      response->set_content(
          "<!doctype html><html><body><p>Redirect "
          "successful</p></body></html>");
      return response;
    }

    std::string destination = request.GetURL().spec() + "/redirected";
    response->set_code(net::HTTP_TEMPORARY_REDIRECT);
    response->set_content_type("text/html");
    response->AddCustomHeader("Location", destination);
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    response->set_content(base::StringPrintf(
        "<!doctype html><html><body><p>Redirecting to %s</p></body></html>",
        destination.c_str()));
    return response;
  }

 protected:
  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    // Allow different origins to be handled by the embedded_test_server.
    host_resolver()->AddRule("*", "127.0.0.1");
    test::WaitUntilWebAppProviderAndSubsystemsReady(provider());
  }

  Profile* profile() { return browser()->profile(); }

  WebAppRegistrar& registrar() { return provider()->registrar_unsafe(); }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  void InstallApp(ExternalInstallOptions install_options) {
    auto result = ExternallyManagedAppManagerInstall(
        profile(), std::move(install_options));
    result_code_ = result.code;
  }

  std::optional<webapps::InstallResultCode> result_code_;
  bool simulate_redirect_ = false;
};

IN_PROC_BROWSER_TEST_F(WebKioskInstallerLacrosBrowserTest,
                       UpdatePlaceholderSucceedsSameAppId) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &WebKioskInstallerLacrosBrowserTest::SimulateRedirectHandler,
      base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());

  simulate_redirect_ = true;
  GURL url = embedded_test_server()->GetURL("/banners/manifest_test_page.html");
  ExternalInstallOptions options =
      CreateInstallOptions(url, ExternalInstallSource::kKiosk);
  options.install_placeholder = true;
  options.add_to_applications_menu = true;
  options.add_to_desktop = true;
  InstallApp(options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> app_id = registrar().LookupExternalAppId(url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_TRUE(
      registrar().IsPlaceholderApp(app_id.value(), WebAppManagement::kKiosk));

  simulate_redirect_ = false;
  InstallApp(options);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
            result_code_.value());
  std::optional<webapps::AppId> final_app_id =
      registrar().LookupExternalAppId(url);
  ASSERT_TRUE(final_app_id.has_value());
  EXPECT_FALSE(registrar().IsPlaceholderApp(final_app_id.value(),
                                            WebAppManagement::kKiosk));
  EXPECT_EQ(0, registrar().CountUserInstalledApps());
  EXPECT_EQ(1u, registrar()
                    .GetExternallyInstalledApps(ExternalInstallSource::kKiosk)
                    .size());
}

}  // namespace

}  // namespace web_app
