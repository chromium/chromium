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
constexpr char kPathToBeServed[] = "chrome/test/data";

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
    : public WebKioskBaseTest,
      public testing::WithParamInterface<PermissionParam> {
 public:
  WebKioskBrowserPermissionsTest() = default;

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
        BrowserView::GetBrowserViewForBrowser(kiosk_app_browser());
    return browser_view ? browser_view->GetActiveWebContents() : nullptr;
  }

  void AllowBrowserPermissionsForOrigin(const std::string& origin) {
    kiosk_app_browser()->profile()->GetPrefs()->SetList(
        prefs::kKioskBrowserPermissionsAllowedForOrigins,
        base::Value::List().Append(origin));
  }

 private:
  FakeOriginTestServerMixin install_origin_server_mixin_{
      &mixin_host_, GURL(kInstallOrigin), FILE_PATH_LITERAL(kPathToBeServed)};

  FakeOriginTestServerMixin non_install_origin_server_mixin_{
      &mixin_host_, GURL(kNonInstallOrigin),
      FILE_PATH_LITERAL(kPathToBeServed)};
};

class WebKioskGeolocationBrowserPermissionTest
    : public WebKioskBrowserPermissionsTest {
 public:
  WebKioskGeolocationBrowserPermissionTest()
      : geolocation_overrider_(
            std::make_unique<device::ScopedGeolocationOverrider>(
                /*latitude=*/0,
                /*longitude=*/0)) {}

  void WaitForPermissionDefined(content::WebContents* web_contents) {
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
  InitializeRegularOnlineKiosk();
  if (GetParam().allow_origin_by_policy) {
    AllowBrowserPermissionsForOrigin(GetParam().origin);
  }

  content::WebContents* web_contents = GetKioskAppWebContents();
  ASSERT_NE(web_contents, nullptr);
  ASSERT_TRUE(content::NavigateToURL(web_contents, GURL(GetParam().origin)));

  WaitForPermissionDefined(web_contents);

  content::EvalJsResult result = CallPermission(web_contents);
  EXPECT_EQ(result.value.GetString(), GetParam().result_message);
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
