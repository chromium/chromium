// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/repeating_test_future.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_install/app_install.pb.h"
#include "chrome/browser/apps/app_service/app_install/app_install_navigation_throttle.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/chromeos/crosapi/test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/ui/web_applications/web_app_launch_process.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

namespace apps {

class AppInstallNavigationThottleBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  static std::string ParamToString(testing::TestParamInfo<bool> param) {
    return param.param ? "AshDialogEnabled" : "AshDialogDisabled";
  }

  AppInstallNavigationThottleBrowserTest() = default;

  bool is_ash_dialog_enabled() const { return GetParam(); }

  void SetUpOnMainThread() override {
    if (!crosapi::AshSupportsCapabilities({"b/304680258"})) {
      GTEST_SKIP() << "Unsupported Ash version.";
    }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    if (is_ash_dialog_enabled() &&
        chromeos::LacrosService::Get()
                ->GetInterfaceVersion<crosapi::mojom::TestController>() <
            int{crosapi::mojom::TestController::MethodMinVersions::
                    kSetAppInstallDialogAutoAcceptMinVersion}) {
      GTEST_SKIP() << "Unsupported Ash version.";
    }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &AppInstallNavigationThottleBrowserTest::HandleRequest,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    crosapi::mojom::TestControllerAsyncWaiter(crosapi::GetTestController())
        .SetAlmanacEndpointUrlForTesting(
            embedded_test_server()->GetURL("/").spec());
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto it = response_map_.find(request.GetURL());
    if (it == response_map_.end()) {
      return nullptr;
    }
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(it->second);
    return std::move(http_response);
  }

  void SetInstallDialogAutoAccept(bool auto_accept) {
    if (is_ash_dialog_enabled()) {
      crosapi::mojom::TestControllerAsyncWaiter(crosapi::GetTestController())
          .SetAppInstallDialogAutoAccept(auto_accept);
    } else {
      web_app::SetAutoAcceptPWAInstallConfirmationForTesting(auto_accept);
    }
  }

  std::map<GURL, std::string> response_map_;
  base::AutoReset<bool> feature_scope_ =
      chromeos::features::SetAppInstallServiceUriEnabledForTesting();
};

IN_PROC_BROWSER_TEST_P(AppInstallNavigationThottleBrowserTest,
                       UrlTriggeredInstallation) {
  GURL start_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  GURL manifest_url = embedded_test_server()->GetURL("/web_apps/basic.json");
  webapps::ManifestId manifest_id = start_url;
  webapps::AppId app_id = web_app::GenerateAppIdFromManifestId(manifest_id);
  PackageId package_id(apps::AppType::kWeb, manifest_id.spec());

  // Set Almanac server payload.
  response_map_[embedded_test_server()->GetURL("/v1/app-install")] = [&] {
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
    return response.SerializeAsString();
  }();

  // Make install prompts auto accept.
  SetInstallDialogAutoAccept(true);

  // Open install-app URI.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::StringPrintf("window.open('almanac://install-app?package_id=%s');",
                         package_id.ToString().c_str())));

  // This should trigger the sequence:
  // - AppInstallNavigationThrottle
  // - AppInstallServiceAsh
  // - NavigateAndTriggerInstallDialogCommand

  // Await install to complete.
  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({app_id});

  // Check that window.open() didn't leave an extra about:blank tab lying
  // around, there should only be the original about:blank tab and the install
  // page tab / install dialog.
  EXPECT_EQ(browser()->tab_strip_model()->count(),
            is_ash_dialog_enabled() ? 1 : 2);

  // Disable install prompt auto accept.
  SetInstallDialogAutoAccept(false);

  // Test whether already installed apps launch instead of going through the
  // install flow again.
  if (crosapi::AshSupportsCapabilities({"b/326167458"})) {
    base::test::RepeatingTestFuture<apps::AppLaunchParams> future;
    web_app::WebAppLaunchProcess::SetOpenApplicationCallbackForTesting(
        future.GetCallback());

    // Open install-app URI again.
    EXPECT_TRUE(content::ExecJs(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::StringPrintf(
            "window.open('almanac://install-app?package_id=%s');",
            package_id.ToString().c_str())));

    // This should launch the app instead of triggering installation.
    EXPECT_EQ(future.Take().app_id, app_id);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         AppInstallNavigationThottleBrowserTest,
                         testing::Values(true, false),
                         AppInstallNavigationThottleBrowserTest::ParamToString);

}  // namespace apps
