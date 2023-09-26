// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/payments/chrome_payment_request_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/payments/core/features.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using base::test::RunOnceCallback;
using testing::_;

constexpr char kTestAppHost[] = "payments.example.com";
constexpr char kTestAppPackageName[] = "com.example.app";
const std::u16string kTestAppTitle = u"Test App";

class MockWebAppService : public crosapi::mojom::WebAppService {
 public:
  MockWebAppService() = default;
  MockWebAppService(const MockWebAppService&) = delete;
  MockWebAppService& operator=(const MockWebAppService&) = delete;
  ~MockWebAppService() override = default;

  // crosapi::mojom::WebAppService:
  MOCK_METHOD(void,
              RegisterWebAppProviderBridge,
              (mojo::PendingRemote<crosapi::mojom::WebAppProviderBridge>
                   web_app_provider_bridge),
              (override));
  MOCK_METHOD(void,
              GetAssociatedAndroidPackage,
              (const std::string& web_app_id,
               GetAssociatedAndroidPackageCallback callback),
              (override));
  MOCK_METHOD(void,
              MigrateLauncherState,
              (const std::string& from_app_id,
               const std::string& to_app_id,
               MigrateLauncherStateCallback callback),
              (override));
};

crosapi::mojom::WebAppAndroidPackagePtr CreateWebAppAndroidPackage() {
  auto result = crosapi::mojom::WebAppAndroidPackage::New();
  result->package_name = kTestAppPackageName;
  return result;
}

}  // namespace

class PaymentRequestLacrosBrowserTest
    : public web_app::WebAppControllerBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  PaymentRequestLacrosBrowserTest() : has_associated_package_(GetParam()) {}
  ~PaymentRequestLacrosBrowserTest() override = default;

  // web_app::WebAppControllerBrowserTest:
  void SetUpOnMainThread() override {
    web_app::WebAppControllerBrowserTest::SetUpOnMainThread();

    chromeos::LacrosService* service = chromeos::LacrosService::Get();
    ASSERT_TRUE(service);
    ASSERT_TRUE(service->IsAvailable<crosapi::mojom::WebAppService>());

    // Replace the production web app service with a mock for testing.
    mojo::Remote<crosapi::mojom::WebAppService>& remote =
        service->GetRemote<crosapi::mojom::WebAppService>();
    remote.reset();
    receiver_.Bind(remote.BindNewPipeAndPassReceiver());

    if (has_associated_package_) {
      EXPECT_CALL(service_, GetAssociatedAndroidPackage(_, _))
          .WillRepeatedly([](const std::string& web_app_id,
                             crosapi::mojom::WebAppService::
                                 GetAssociatedAndroidPackageCallback callback) {
            std::move(callback).Run(CreateWebAppAndroidPackage());
          });
    } else {
      EXPECT_CALL(service_, GetAssociatedAndroidPackage(_, _))
          .WillRepeatedly(RunOnceCallback<1>(nullptr));
    }

    InstallTestApp();
  }

  void TearDownOnMainThread() override {
    UninstallTestApp();
    web_app::WebAppControllerBrowserTest::TearDownOnMainThread();
  }

  GURL GetAppURL() {
    return https_server()->GetURL(kTestAppHost, "/simple.html");
  }

  const webapps::AppId& app_id() const { return app_id_; }

 protected:
  const bool has_associated_package_;

 private:
  void InstallTestApp() {
    auto app_info = std::make_unique<web_app::WebAppInstallInfo>();
    app_info->start_url = GetAppURL();
    app_info->scope = app_info->start_url.GetWithoutFilename();
    app_info->title = kTestAppTitle;
    app_info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;

    app_id_ =
        web_app::test::InstallWebApp(browser()->profile(), std::move(app_info));
    apps::AppReadinessWaiter(browser()->profile(), app_id_).Await();
  }

  void UninstallTestApp() {
    web_app::test::UninstallWebApp(browser()->profile(), app_id_);
    apps::AppReadinessWaiter(browser()->profile(), app_id_,
                             apps::Readiness::kUninstalledByUser)
        .Await();
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      payments::features::kAppStoreBilling};

  testing::NiceMock<MockWebAppService> service_;
  mojo::Receiver<crosapi::mojom::WebAppService> receiver_{&service_};

  webapps::AppId app_id_;
};

IN_PROC_BROWSER_TEST_P(PaymentRequestLacrosBrowserTest, BrowserTab) {
  web_app::NavigateToURLAndWait(browser(), GetAppURL());
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* render_frame_host = contents->GetPrimaryMainFrame();
  payments::ChromePaymentRequestDelegate delegate(render_frame_host);

  base::RunLoop run_loop;
  delegate.GetTwaPackageName(base::BindLambdaForTesting(
      [&run_loop](const std::string& twa_package_name) {
        EXPECT_TRUE(twa_package_name.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(PaymentRequestLacrosBrowserTest, AppWindow) {
  Browser* app_browser =
      web_app::LaunchWebAppBrowserAndWait(browser()->profile(), app_id());
  content::WebContents* contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* render_frame_host = contents->GetPrimaryMainFrame();
  payments::ChromePaymentRequestDelegate delegate(render_frame_host);

  base::RunLoop run_loop;
  delegate.GetTwaPackageName(base::BindLambdaForTesting(
      [this, &run_loop](const std::string& twa_package_name) {
        if (has_associated_package_) {
          EXPECT_EQ(twa_package_name, kTestAppPackageName);
        } else {
          EXPECT_TRUE(twa_package_name.empty());
        }

        run_loop.Quit();
      }));
  run_loop.Run();
  web_app::CloseAndWait(app_browser);
}

IN_PROC_BROWSER_TEST_P(PaymentRequestLacrosBrowserTest, OutOfScope) {
  Browser* app_browser =
      web_app::LaunchWebAppBrowserAndWait(browser()->profile(), app_id());
  web_app::NavigateToURLAndWait(
      app_browser, https_server()->GetURL("another.test.site", "/"));
  content::WebContents* contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* render_frame_host = contents->GetPrimaryMainFrame();
  payments::ChromePaymentRequestDelegate delegate(render_frame_host);

  base::RunLoop run_loop;
  delegate.GetTwaPackageName(base::BindLambdaForTesting(
      [&run_loop](const std::string& twa_package_name) {
        EXPECT_TRUE(twa_package_name.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
  web_app::CloseAndWait(app_browser);
}

INSTANTIATE_TEST_SUITE_P(All, PaymentRequestLacrosBrowserTest, testing::Bool());
