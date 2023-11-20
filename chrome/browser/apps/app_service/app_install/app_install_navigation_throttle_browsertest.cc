// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_install/app_install.pb.h"
#include "chrome/browser/apps/app_service/app_install/app_install_navigation_throttle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/almanac_api_client/almanac_api_util.h"
#else
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace apps {

class AppInstallNavigationThottleBrowserTest : public InProcessBrowserTest {
 public:
  AppInstallNavigationThottleBrowserTest() = default;

  void SetUpOnMainThread() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &AppInstallNavigationThottleBrowserTest::HandleRequest,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    // Override Almanac server URL.
    std::string test_endpoint = embedded_test_server()->GetURL("/").spec();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    apps::SetAlmanacEndpointUrlForTesting(std::move(test_endpoint));
#else
    const absl::optional<std::vector<std::string>>& capabilities =
        chromeos::BrowserParamsProxy::Get()->AshCapabilities();
    if (!capabilities || !base::Contains(*capabilities, "b/304680258")) {
      GTEST_SKIP() << "Unsupported Ash version.";
    }
    base::RunLoop run_loop;
    chromeos::LacrosService::Get()
        ->GetRemote<crosapi::mojom::TestController>()
        ->SetAlmanacEndpointUrlForTesting(test_endpoint,
                                          run_loop.QuitClosure());
    run_loop.Run();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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

  std::map<GURL, std::string> response_map_;
  base::AutoReset<bool> feature_scope_ =
      chromeos::features::SetAppInstallServiceUriEnabledForTesting();
};

IN_PROC_BROWSER_TEST_F(AppInstallNavigationThottleBrowserTest,
                       UrlTriggeredInstallation) {
  GURL start_url = embedded_test_server()->GetURL("/web_apps/basic.html");
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
    web_extras.set_original_manifest_url(start_url.spec());
    web_extras.set_scs_url(start_url.spec());
    return response.SerializeAsString();
  }();

  // Make install prompts auto accept.
  web_app::SetAutoAcceptPWAInstallConfirmationForTesting(/*auto_accept=*/true);

  // Open GIOC URI.
  NavigateParams params(browser(),
                        GURL(base::StrCat({"almanac://install-app?package_id=",
                                           package_id.ToString()})),
                        ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  // This should trigger the sequence:
  // - AppInstallNavigationThrottle
  // - AppInstallServiceAsh
  // - NavigateAndTriggerInstallDialogCommand

  // Await install to complete.
  web_app::WebAppTestInstallObserver(browser()->profile())
      .BeginListeningAndWait({app_id});
}

}  // namespace apps
