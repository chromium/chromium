// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/strings/string_util.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test_update_server.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/key_pair.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/host_port_pair.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-data-view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

using kiosk::test::WaitKioskLaunched;

namespace {

web_package::SignedWebBundleId GetTestWebBundleId() {
  return web_app::test::GetDefaultEd25519WebBundleId();
}

web_package::test::KeyPair GetTestKeyPair() {
  return web_app::test::GetDefaultEd25519KeyPair();
}

KioskMixin::Config GetKioskIwaAutolaunchConfig(
    const GURL& update_manifest_url) {
  KioskMixin::IsolatedWebAppOption iwa_option(
      /*account_id=*/"simple-iwa@localhost",
      /*web_bundle_id=*/GetTestWebBundleId(),
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

bool IsControlledFrameElementCreated(content::WebContents* web_contents) {
  return content::EvalJs(web_contents,
                         "'src' in document.createElement('controlledframe')")
      .ExtractBool();
}

void ExpectJsErrorDisabledByPermissionsPolicy(
    content::WebContents* web_contents,
    const std::string& js_code) {
  constexpr std::string_view kExpectedError =
      "Permissions-Policy: direct-sockets are disabled.";
  EXPECT_THAT(
      EvalJs(web_contents, js_code),
      content::EvalJsResult::ErrorIs(testing::HasSubstr(kExpectedError)));
}

void ExpectDirectSocketsDisabledByPermissionsPolicy(
    content::WebContents* web_contents) {
  ExpectJsErrorDisabledByPermissionsPolicy(
      web_contents, "new UDPSocket({localAddress:'127.0.0.1'})");
  ExpectJsErrorDisabledByPermissionsPolicy(web_contents,
                                           "new TCPSocket('127.0.0.1', 0)");
  ExpectJsErrorDisabledByPermissionsPolicy(web_contents,
                                           "new TCPServerSocket('127.0.0.1')");
}

void ExpectUDPSocketOk(content::WebContents* web_contents) {
  constexpr std::string_view kScriptUDP = R"(
    (async () => {
      const socket = new UDPSocket({localAddress:'127.0.0.1'});
      await socket.opened;
    })();
  )";

  ASSERT_THAT(EvalJs(web_contents, kScriptUDP), content::EvalJsResult::IsOk());
}

void ExpectTCPSocketOk(content::WebContents* web_contents) {
  constexpr std::string_view kScriptTCP = R"(
    (async () => {
      const serverSocket = new TCPServerSocket('127.0.0.1');
      const { localPort: serverSocketPort } = await serverSocket.opened;

      const socket = new TCPSocket('127.0.0.1', serverSocketPort);
      await socket.opened;
    })();
  )";

  ASSERT_THAT(EvalJs(web_contents, kScriptTCP), content::EvalJsResult::IsOk());
}

void WaitForPageLoad(content::WebContents* contents) {
  EXPECT_TRUE(WaitForLoadStop(contents));
  EXPECT_TRUE(WaitForRenderFrameReady(contents->GetPrimaryMainFrame()));
}

std::unique_ptr<web_app::BundledIsolatedWebApp>
CreateTestIwaWithoutPermissions() {
  return web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder())
      .BuildBundle(GetTestKeyPair());
}

std::unique_ptr<web_app::BundledIsolatedWebApp>
CreateTestIwaWithCommonPermissions() {
  return web_app::IsolatedWebAppBuilder(
             web_app::ManifestBuilder()
                 .AddPermissionsPolicy(
                     network::mojom::PermissionsPolicyFeature::kBluetooth,
                     /*self=*/true, /*origins=*/{})
                 .AddPermissionsPolicy(
                     network::mojom::PermissionsPolicyFeature::kMicrophone,
                     /*self=*/true, /*origins=*/{}))
      .BuildBundle(GetTestKeyPair());
}

std::unique_ptr<web_app::BundledIsolatedWebApp>
CreateTestIwaWithDirectSockets() {
  return web_app::IsolatedWebAppBuilder(
             web_app::ManifestBuilder()
                 .AddPermissionsPolicy(
                     network::mojom::PermissionsPolicyFeature::kDirectSockets,
                     /*self=*/true, /*origins=*/{})
                 .AddPermissionsPolicy(
                     network::mojom::PermissionsPolicyFeature::
                         kDirectSocketsPrivate,
                     /*self=*/true, /*origins=*/{}))
      .BuildBundle(GetTestKeyPair());
}

std::unique_ptr<web_app::BundledIsolatedWebApp>
CreateTestIwaWithControlledFrame() {
  return web_app::IsolatedWebAppBuilder(
             web_app::ManifestBuilder().AddPermissionsPolicy(
                 network::mojom::PermissionsPolicyFeature::kControlledFrame,
                 /*self=*/true, /*origins=*/{}))
      .BuildBundle(GetTestKeyPair());
}

}  // namespace

class KioskIwaPermissionsBaseTest : public MixinBasedInProcessBrowserTest {
 public:
  explicit KioskIwaPermissionsBaseTest(
      std::unique_ptr<web_app::BundledIsolatedWebApp> test_app) {
    iwa_test_update_server_.AddBundle(std::move(test_app));
  }

  ~KioskIwaPermissionsBaseTest() override = default;
  KioskIwaPermissionsBaseTest(const KioskIwaPermissionsBaseTest&) = delete;
  KioskIwaPermissionsBaseTest& operator=(const KioskIwaPermissionsBaseTest&) =
      delete;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ui_test_utils::BrowserCreatedObserver browser_created_observer;
    ASSERT_TRUE(WaitKioskLaunched());
    SetBrowser(browser_created_observer.Wait());

    ASSERT_NE(web_contents(), nullptr);
    ASSERT_EQ(web_contents()->GetVisibleURL(), kExpectedOrigin.GetURL());
    WaitForPageLoad(web_contents());
  }

 protected:
  content::WebContents* web_contents() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view ? browser_view->GetActiveWebContents() : nullptr;
  }

 private:
  const url::Origin kExpectedOrigin =
      url::Origin::CreateFromNormalizedTuple(webapps::kIsolatedAppScheme,
                                             GetTestWebBundleId().id(),
                                             /*port=*/0);

  web_app::IsolatedWebAppTestUpdateServer iwa_test_update_server_;
  KioskMixin kiosk_{
      &mixin_host_,
      GetKioskIwaAutolaunchConfig(
          iwa_test_update_server_.GetUpdateManifestUrl(GetTestWebBundleId()))};
};

class KioskIwaCommonPermissionsTest : public KioskIwaPermissionsBaseTest {
 public:
  KioskIwaCommonPermissionsTest()
      : KioskIwaPermissionsBaseTest(CreateTestIwaWithCommonPermissions()) {}
};

IN_PROC_BROWSER_TEST_F(KioskIwaCommonPermissionsTest,
                       AutograntsMicrophoneWhenRequested) {
  constexpr std::string_view kRequestMicrophone = R"(
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

IN_PROC_BROWSER_TEST_F(KioskIwaCommonPermissionsTest,
                       AutograntsBluetoothWhenRequested) {
  constexpr std::string_view kRequestBluetooth = R"(
    new Promise((resolve) => {
      navigator.bluetooth.getAvailability()
        .then((available) => { resolve(available); })
        .catch((err) => { resolve(false); });
    })
  )";
  EXPECT_TRUE(content::EvalJs(web_contents(), kRequestBluetooth).ExtractBool());
}

IN_PROC_BROWSER_TEST_F(KioskIwaCommonPermissionsTest,
                       DeniesMidiAsNotInPermissionsPolicy) {
  constexpr std::string_view kRequestMidi = R"(
    new Promise((resolve) => {
      navigator.requestMIDIAccess()
        .then((access) => { resolve(true); })
        .catch((err) => { resolve(false); });
    })
  )";
  EXPECT_FALSE(content::EvalJs(web_contents(), kRequestMidi).ExtractBool());
  EXPECT_EQ(QueryPermission(web_contents(), "midi"), "denied");
}

IN_PROC_BROWSER_TEST_F(KioskIwaCommonPermissionsTest,
                       DeniesCameraAsNotInPermissionsPolicy) {
  constexpr std::string_view kRequestCamera = R"(
    new Promise((resolve) => {
      navigator.mediaDevices.getUserMedia({ video: true })
        .then((stream) => { resolve(true); })
        .catch((err) => { resolve(false); });
    })
  )";
  EXPECT_FALSE(content::EvalJs(web_contents(), kRequestCamera).ExtractBool());
  EXPECT_EQ(QueryPermission(web_contents(), "camera"), "denied");
}

class KioskIwaNoPermissionsTest : public KioskIwaPermissionsBaseTest {
 public:
  KioskIwaNoPermissionsTest()
      : KioskIwaPermissionsBaseTest(CreateTestIwaWithoutPermissions()) {}
};

IN_PROC_BROWSER_TEST_F(KioskIwaNoPermissionsTest, PrivilegedApisNotAvailable) {
  ExpectDirectSocketsDisabledByPermissionsPolicy(web_contents());
  EXPECT_FALSE(IsControlledFrameElementCreated(web_contents()));
}

class KioskIwaDirectSocketsTest : public KioskIwaPermissionsBaseTest {
 public:
  KioskIwaDirectSocketsTest()
      : KioskIwaPermissionsBaseTest(CreateTestIwaWithDirectSockets()) {}
};

IN_PROC_BROWSER_TEST_F(KioskIwaDirectSocketsTest, ApiAvailable) {
  ExpectUDPSocketOk(web_contents());
  ExpectTCPSocketOk(web_contents());
}

class KioskIwaControlledFrameTest : public KioskIwaPermissionsBaseTest {
 public:
  KioskIwaControlledFrameTest()
      : KioskIwaPermissionsBaseTest(CreateTestIwaWithControlledFrame()) {}
};

IN_PROC_BROWSER_TEST_F(KioskIwaControlledFrameTest, ApiAvailable) {
  EXPECT_TRUE(IsControlledFrameElementCreated(web_contents()));
}

}  // namespace ash
