// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "chrome/browser/app_mode/test/fake_origin_test_server_mixin.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kInstallOrigin[] = "https://kiosk.com/";
constexpr char kInstallURL[] = "https://kiosk.com/title3.html";
constexpr char kNonInstallOrigin[] = "https://example.com/";
constexpr char kNonInstallURL[] = "https://example.com/title3.html";
constexpr char kSuccessMessage[] = "SUCCESS";
constexpr char kFailureMessage[] = "FAIL: 1 User denied Geolocation";

}  // namespace

class WebKioskBrowserPermissionsTest : public WebKioskBaseTest {
 public:
  WebKioskBrowserPermissionsTest()
      : geolocation_overrider_(
            std::make_unique<device::ScopedGeolocationOverrider>(0, 0)) {}

  WebKioskBrowserPermissionsTest(const WebKioskBrowserPermissionsTest&) =
      delete;
  WebKioskBrowserPermissionsTest& operator=(
      const WebKioskBrowserPermissionsTest&) = delete;

  void SetUpOnMainThread() override {
    WebKioskBaseTest::SetUpOnMainThread();
    SetAppInstallUrl(GURL(kInstallURL));
  }

  content::WebContents* GetKioskAppWebContents() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(initial_browser());
    return browser_view ? browser_view->GetActiveWebContents() : nullptr;
  }

  void WaitForGeoLocationDefined(content::WebContents* web_contents) {
    ash::test::TestPredicateWaiter(
        base::BindRepeating(
            [](content::WebContents* web_contents) {
              return content::EvalJs(
                         web_contents,
                         "typeof navigator.geolocation !== 'undefined'")
                  .ExtractBool();
            },
            web_contents))
        .Wait();
  }

  content::EvalJsResult CallGeoLocationPermission(
      content::WebContents* web_contents) {
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

  void AllowBrowserPermissionsForOrigin(const std::string& origin) {
    initial_browser()->profile()->GetPrefs()->SetList(
        prefs::kKioskBrowserPermissionsAllowedForOrigins,
        base::Value::List().Append(origin));
  }

  Browser* initial_browser() {
    Browser* initial_browser = BrowserList::GetInstance()->get(0);
    CHECK(initial_browser);
    return initial_browser;
  }

 private:
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;

  FakeOriginTestServerMixin server_mixin_{
      &mixin_host_,
      /*origin=*/GURL(kInstallOrigin),
      /*path_to_be_served=*/FILE_PATH_LITERAL("chrome/test/data")};
};

IN_PROC_BROWSER_TEST_F(WebKioskBrowserPermissionsTest,
                       InstallOriginCanAccessGeolocationPermission) {
  InitializeRegularOnlineKiosk();
  content::WebContents* web_contents = GetKioskAppWebContents();
  ASSERT_NE(web_contents, nullptr);

  WaitForGeoLocationDefined(web_contents);

  content::EvalJsResult result = CallGeoLocationPermission(web_contents);
  EXPECT_EQ(result.value.GetString(), kSuccessMessage);
}

class WebKioskBrowserPermissionsNonInstallOriginTest
    : public WebKioskBrowserPermissionsTest {
 private:
  FakeOriginTestServerMixin server_mixin_{
      &mixin_host_,
      /*origin=*/GURL(kNonInstallOrigin),
      /*path_to_be_served=*/FILE_PATH_LITERAL("chrome/test/data")};
};

IN_PROC_BROWSER_TEST_F(WebKioskBrowserPermissionsNonInstallOriginTest,
                       OriginCannotAccessBrowserPermissions) {
  InitializeRegularOnlineKiosk();
  content::WebContents* web_contents = GetKioskAppWebContents();
  ASSERT_NE(web_contents, nullptr);
  ASSERT_TRUE(content::NavigateToURL(web_contents, GURL(kNonInstallURL)));

  WaitForGeoLocationDefined(web_contents);

  content::EvalJsResult result = CallGeoLocationPermission(web_contents);
  EXPECT_TRUE(base::Contains(result.value.GetString(), kFailureMessage));
}

IN_PROC_BROWSER_TEST_F(WebKioskBrowserPermissionsNonInstallOriginTest,
                       OriginCanAccessBrowserPermissionsIfAllowedByPref) {
  InitializeRegularOnlineKiosk();
  AllowBrowserPermissionsForOrigin(kNonInstallOrigin);
  content::WebContents* web_contents = GetKioskAppWebContents();
  ASSERT_NE(web_contents, nullptr);
  ASSERT_TRUE(content::NavigateToURL(web_contents, GURL(kNonInstallURL)));

  WaitForGeoLocationDefined(web_contents);

  content::EvalJsResult result = CallGeoLocationPermission(web_contents);
  EXPECT_EQ(result.value.GetString(), kSuccessMessage);
}

}  // namespace ash
