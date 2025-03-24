// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_server_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/host_port_pair.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

using kiosk::test::WaitKioskLaunched;

namespace {

const web_package::SignedWebBundleId kTestWebBundleId =
    web_app::test::GetDefaultEd25519WebBundleId();

KioskMixin::Config GetKioskIwaAutolaunchConfig(
    const GURL& update_manifest_url) {
  KioskMixin::IsolatedWebAppOption iwa_option(
      /*account_id=*/"simple-iwa@localhost",
      /*web_bundle_id=*/kTestWebBundleId,
      /*update_manifest_url=*/update_manifest_url);

  KioskMixin::Config kiosk_iwa_config = {
      /*name=*/"IsolatedWebApp",
      KioskMixin::AutoLaunchAccount{iwa_option.account_id},
      {iwa_option}};
  return kiosk_iwa_config;
}

std::string QueryPermission(const content::ToRenderFrameHost& target,
                            const std::string& permission) {
  const std::string descriptor = "{name: '" + permission + "'}";
  const std::string script =
      "navigator.permissions.query(" + descriptor +
      ").then(permission => permission.state).catch(e => e.name);";
  return EvalJs(target, script, content::EXECUTE_SCRIPT_NO_USER_GESTURE)
      .ExtractString();
}

// Waits until a `js_name` is defined. Is used to prevent API calls before the
// test web page has loaded.
void WaitForJsObject(content::WebContents* web_contents,
                     const std::string& js_name) {
  ash::test::TestPredicateWaiter(
      base::BindRepeating(
          [](content::WebContents* web_contents, const std::string& js_name) {
            return content::EvalJs(
                       web_contents,
                       base::ReplaceStringPlaceholders(
                           "typeof $1 !== 'undefined'", {js_name}, nullptr))
                .ExtractBool();
          },
          web_contents, js_name))
      .Wait();
}

std::unique_ptr<web_app::BundledIsolatedWebApp> CreateTestIwa() {
  return web_app::IsolatedWebAppBuilder(
             web_app::ManifestBuilder()
                 .AddPermissionsPolicy(
                     network::mojom::PermissionsPolicyFeature::kBluetooth,
                     /*self=*/true, /*origins=*/{})
                 .AddPermissionsPolicy(
                     network::mojom::PermissionsPolicyFeature::kMicrophone,
                     /*self=*/true, /*origins=*/{}))
      .BuildBundle(web_app::test::GetDefaultEd25519KeyPair());
}

}  // namespace

class KioskIwaPermissionsTest : public MixinBasedInProcessBrowserTest {
 public:
  KioskIwaPermissionsTest() { iwa_server_mixin_.AddBundle(CreateTestIwa()); }

  ~KioskIwaPermissionsTest() override = default;
  KioskIwaPermissionsTest(const KioskIwaPermissionsTest&) = delete;
  KioskIwaPermissionsTest& operator=(const KioskIwaPermissionsTest&) = delete;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(WaitKioskLaunched());

    SelectFirstBrowser();
    ASSERT_NE(web_contents(), nullptr);
    ASSERT_EQ(web_contents()->GetVisibleURL(), kExpectedOrigin.GetURL());
  }

 protected:
  content::WebContents* web_contents() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view ? browser_view->GetActiveWebContents() : nullptr;
  }

 private:
  const url::Origin kExpectedOrigin =
      url::Origin::CreateFromNormalizedTuple(chrome::kIsolatedAppScheme,
                                             kTestWebBundleId.id(),
                                             /*port=*/0);

  base::test::ScopedFeatureList feature_list_{
      ash::features::kIsolatedWebAppKiosk};
  web_app::IsolatedWebAppUpdateServerMixin iwa_server_mixin_{&mixin_host_};
  KioskMixin kiosk_{
      &mixin_host_,
      GetKioskIwaAutolaunchConfig(
          iwa_server_mixin_.GetUpdateManifestUrl(kTestWebBundleId))};
};

IN_PROC_BROWSER_TEST_F(KioskIwaPermissionsTest,
                       AutograntsMicrophoneWhenRequested) {
  WaitForJsObject(web_contents(), "navigator.mediaDevices");

  constexpr char kRequestMicrophone[] = R"(
    new Promise((resolve) => {
      navigator.mediaDevices.getUserMedia({ audio: true })
        .then((stream) => { resolve(true); })
        .catch((err) => { resolve(false); });
    })
  )";
  EXPECT_TRUE(
      content::EvalJs(web_contents(), kRequestMicrophone).ExtractBool());
  EXPECT_EQ(QueryPermission(web_contents(), "microphone"), "granted");
}

IN_PROC_BROWSER_TEST_F(KioskIwaPermissionsTest,
                       AutograntsBluetoothWhenRequested) {
  WaitForJsObject(web_contents(), "navigator.bluetooth");

  constexpr char kRequestBluetooth[] = R"(
    new Promise((resolve) => {
      navigator.bluetooth.getAvailability()
        .then((available) => { resolve(available); })
        .catch((err) => { resolve(false); });
    })
  )";
  EXPECT_TRUE(content::EvalJs(web_contents(), kRequestBluetooth).ExtractBool());
}

IN_PROC_BROWSER_TEST_F(KioskIwaPermissionsTest,
                       DeniesMidiAsNotInPermissionsPolicy) {
  WaitForJsObject(web_contents(), "navigator.requestMIDIAccess");

  constexpr char kRequestMidi[] = R"(
    new Promise((resolve) => {
      navigator.requestMIDIAccess()
        .then((access) => { resolve(true); })
        .catch((err) => { resolve(false); });
    })
  )";
  EXPECT_FALSE(content::EvalJs(web_contents(), kRequestMidi).ExtractBool());
  EXPECT_EQ(QueryPermission(web_contents(), "midi"), "denied");
}

IN_PROC_BROWSER_TEST_F(KioskIwaPermissionsTest,
                       DeniesCameraAsNotInPermissionsPolicy) {
  WaitForJsObject(web_contents(), "navigator.mediaDevices");

  constexpr char kRequestCamera[] = R"(
    new Promise((resolve) => {
      navigator.mediaDevices.getUserMedia({ video: true })
        .then((stream) => { resolve(true); })
        .catch((err) => { resolve(false); });
    })
  )";
  EXPECT_FALSE(content::EvalJs(web_contents(), kRequestCamera).ExtractBool());
  EXPECT_EQ(QueryPermission(web_contents(), "camera"), "denied");
}

}  // namespace ash
