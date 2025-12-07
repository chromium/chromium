// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/values.h"
#include "chrome/browser/app_mode/test/fake_origin_test_server_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

using kiosk::test::WaitKioskLaunched;

namespace {

constexpr char kInstallOrigin[] = "https://kiosk.com/";
constexpr char kInstallURL[] = "https://kiosk.com/title3.html";
constexpr char kNonInstallOrigin[] = "https://example.com/";
constexpr char kNonInstallURL[] = "https://example.com/title3.html";
constexpr char kSuccessMessage[] = "SUCCESS";
constexpr char kFailureMessage[] = "FAIL: 1 User denied Geolocation";
constexpr char kPathToBeServed[] = "chrome/test/data";
constexpr char kKioskAccountId[] = "kiosk_account_id";

struct PermissionParam {
  // If the origin should be allowed by policy.
  bool allow_origin_by_policy;

  // Origin under test for having access to the browser permission.
  std::string origin;

  // Appropriate failure or success message for the browser permission
  // availability.
  std::string result_message;
};

}  // namespace

class WebKioskBrowserPermissionsTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<PermissionParam> {
 public:
  WebKioskBrowserPermissionsTest() = default;

  WebKioskBrowserPermissionsTest(const WebKioskBrowserPermissionsTest&) =
      delete;
  WebKioskBrowserPermissionsTest& operator=(
      const WebKioskBrowserPermissionsTest&) = delete;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ui_test_utils::BrowserCreatedObserver browser_created_observer;
    ASSERT_TRUE(WaitKioskLaunched());
    SetBrowser(browser_created_observer.Wait());
  }

  content::WebContents* GetKioskAppWebContents() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view ? browser_view->GetActiveWebContents() : nullptr;
  }

  void AllowBrowserPermissionsForOrigin(const std::string& origin) {
    browser()->profile()->GetPrefs()->SetList(
        prefs::kKioskBrowserPermissionsAllowedForOrigins,
        base::Value::List().Append(origin));
  }

 private:
  FakeOriginTestServerMixin install_origin_server_mixin_{
      &mixin_host_, GURL(kInstallOrigin), FILE_PATH_LITERAL(kPathToBeServed)};

  FakeOriginTestServerMixin non_install_origin_server_mixin_{
      &mixin_host_, GURL(kNonInstallOrigin),
      FILE_PATH_LITERAL(kPathToBeServed)};

  KioskMixin kiosk_{
      &mixin_host_,
      KioskMixin::Config{
          /*name=*/{},
          KioskMixin::AutoLaunchAccount{kKioskAccountId},
          {KioskMixin::WebAppOption{kKioskAccountId, GURL(kInstallURL)}}}};
};

class WebKioskGeolocationBrowserPermissionTest
    : public WebKioskBrowserPermissionsTest {
 public:
  WebKioskGeolocationBrowserPermissionTest()
      : geolocation_overrider_(
            std::make_unique<device::ScopedGeolocationOverrider>(
                /*latitude=*/0,
                /*longitude=*/0)) {}

  content::EvalJsResult CallPermission(content::WebContents* web_contents) {
    return content::EvalJs(web_contents, R"(
    (async function() {
      try {
        return await new Promise((resolve, reject) => {
          navigator.geolocation.getCurrentPosition(
            (position) => {
              resolve('SUCCESS');
            },
            (error) => {
              const errorMessage = 'FAIL: ' + error.code + ' ' + error.message;
              resolve(errorMessage);
            }
          );
        });
      } catch (err) {
        return 'FAIL: ' + err.name + ': ' + err.message;
      }
    })();)");
  }

 private:
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
};

IN_PROC_BROWSER_TEST_P(WebKioskGeolocationBrowserPermissionTest,
                       CheckOriginAccess) {
  if (GetParam().allow_origin_by_policy) {
    AllowBrowserPermissionsForOrigin(GetParam().origin);
  }

  content::WebContents* web_contents = GetKioskAppWebContents();
  ASSERT_NE(web_contents, nullptr);
  ASSERT_TRUE(content::NavigateToURL(web_contents, GURL(GetParam().origin)));
  ASSERT_TRUE(WaitForLoadStop(web_contents));

  content::EvalJsResult result = CallPermission(web_contents);
  EXPECT_EQ(result, GetParam().result_message);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebKioskGeolocationBrowserPermissionTest,
    testing::Values(PermissionParam{/*allow_origin_by_policy=*/false,
                                    kInstallURL, kSuccessMessage},
                    PermissionParam{/*allow_origin_by_policy=*/true,
                                    kInstallURL, kSuccessMessage},
                    PermissionParam{/*allow_origin_by_policy=*/false,
                                    kNonInstallURL, kFailureMessage},
                    PermissionParam{/*allow_origin_by_policy=*/true,
                                    kNonInstallURL, kSuccessMessage}));

}  // namespace ash
