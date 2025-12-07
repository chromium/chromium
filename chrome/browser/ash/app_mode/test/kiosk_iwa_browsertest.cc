// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test_update_server.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "content/public/test/browser_test.h"
#include "net/base/host_port_pair.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash {

using kiosk::test::CurrentProfile;
using kiosk::test::WaitKioskLaunched;

namespace {

const web_package::SignedWebBundleId kTestWebBundleId =
    web_app::test::GetDefaultEd25519WebBundleId();

KioskMixin::Config GetKioskIwaConfig(const GURL& update_manifest_url) {
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

}  // namespace

class KioskIwaTest : public MixinBasedInProcessBrowserTest {
 public:
  KioskIwaTest() {
    iwa_test_server_.AddBundle(
        web_app::IsolatedWebAppBuilder(
            web_app::ManifestBuilder().SetVersion("1.0.0"))
            .BuildBundle(web_app::test::GetDefaultEd25519KeyPair()));
  }

  ~KioskIwaTest() override = default;
  KioskIwaTest(const KioskIwaTest&) = delete;
  KioskIwaTest& operator=(const KioskIwaTest&) = delete;

 protected:
  web_app::IsolatedWebAppTestUpdateServer iwa_test_server_;
  KioskMixin kiosk_{&mixin_host_,
                    GetKioskIwaConfig(iwa_test_server_.GetUpdateManifestUrl(
                        kTestWebBundleId))};
};

IN_PROC_BROWSER_TEST_F(KioskIwaTest, InstallsAndLaunchesApp) {
  ASSERT_TRUE(WaitKioskLaunched());
}

IN_PROC_BROWSER_TEST_F(KioskIwaTest, OriginHasUnlimitedStorage) {
  ASSERT_TRUE(WaitKioskLaunched());

  ExtensionSpecialStoragePolicy* storage_policy =
      CurrentProfile().GetExtensionSpecialStoragePolicy();
  ASSERT_NE(storage_policy, nullptr);

  const url::Origin kExpectedOrigin = url::Origin::CreateFromNormalizedTuple(
      webapps::kIsolatedAppScheme, kTestWebBundleId.id(), /*port=*/0);
  EXPECT_TRUE(storage_policy->IsStorageUnlimited(kExpectedOrigin.GetURL()));
}

}  // namespace ash
