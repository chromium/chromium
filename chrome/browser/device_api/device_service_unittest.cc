// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_api/device_service_impl.h"

#include <utility>

#include "chrome/browser/web_applications/components/policy/web_app_policy_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "url/gurl.h"

namespace {

constexpr char kDefaultAppInstallUrl[] = "https://example.com/";
constexpr char kTrustedUrl[] = "https://example.com/sample";
constexpr char kUntrustedUrl[] = "https://non-example.com/sample";

}  // namespace

class DeviceAPIServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    InstallTrustedApp();
  }

  void InstallTrustedApp() {
    ListPrefUpdate update(profile()->GetPrefs(),
                          prefs::kWebAppInstallForceList);
    base::DictionaryValue app_policy;
    app_policy.SetString(web_app::kUrlKey, kDefaultAppInstallUrl);
    update->Append(std::move(app_policy));
  }

  void RemoveTrustedApp() {
    ListPrefUpdate update(profile()->GetPrefs(),
                          prefs::kWebAppInstallForceList);
    update->ClearList();
  }

  void TryCreatingService(const GURL& url) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               url);
    DeviceServiceImpl::Create(main_rfh(), remote_.BindNewPipeAndPassReceiver());
  }

  mojo::Remote<blink::mojom::DeviceAPIService>* remote() { return &remote_; }

 private:
  mojo::Remote<blink::mojom::DeviceAPIService> remote_;
};

TEST_F(DeviceAPIServiceTest, FlagOffByDefault) {
  TryCreatingService(GURL(kTrustedUrl));
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

class DeviceAPIServiceWithFeatureFlagTest : public DeviceAPIServiceTest {
 public:
  DeviceAPIServiceWithFeatureFlagTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kEnableRestrictedWebApis);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DeviceAPIServiceWithFeatureFlagTest, ConnectsForTrustedApps) {
  TryCreatingService(GURL(kTrustedUrl));
  remote()->FlushForTesting();
  ASSERT_TRUE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceWithFeatureFlagTest, DoesNotConnectForUntrustedApps) {
  TryCreatingService(GURL(kUntrustedUrl));
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

TEST_F(DeviceAPIServiceWithFeatureFlagTest, DisconnectWhenTrustRevoked) {
  TryCreatingService(GURL(kTrustedUrl));
  remote()->FlushForTesting();
  RemoveTrustedApp();
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}

class DeviceAPIServiceWithoutFeatureFlagTest : public DeviceAPIServiceTest {
 public:
  DeviceAPIServiceWithoutFeatureFlagTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kEnableRestrictedWebApis);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DeviceAPIServiceWithoutFeatureFlagTest, DoesNotConnectWhenFlagOff) {
  TryCreatingService(GURL(kTrustedUrl));
  remote()->FlushForTesting();
  ASSERT_FALSE(remote()->is_connected());
}
