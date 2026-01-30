// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/values.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

namespace {

// Verifies that the command result does not contain an "error" field.
testing::Matcher<const base::DictValue*> IsSuccess() {
  return testing::Pointee(testing::ResultOf(
      "FindDict('error')",
      [](const base::DictValue& dict) { return dict.FindDict("error"); },
      testing::IsNull()));
}

}  // namespace

class SmartCardEmulationBrowserTest : public IsolatedWebAppBrowserTestHarness {
 public:
  SmartCardEmulationBrowserTest() {
    feature_list_.InitAndEnableFeature(blink::features::kSmartCard);
  }
  ~SmartCardEmulationBrowserTest() override = default;

  void SetUpOnMainThread() override {
    IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();
    InstallAndLaunchIWA();
    AttachDevTools();
  }

  void TearDownOnMainThread() override {
    if (devtools_client_) {
      devtools_client_->DetachProtocolClient();
      devtools_client_.reset();
    }
    app_frame_ = nullptr;
    IsolatedWebAppBrowserTestHarness::TearDownOnMainThread();
  }

  const base::DictValue* SendCommand(const std::string& method,
                                     base::DictValue params = {}) {
    return devtools_client_->SendCommandSync(method, std::move(params));
  }

  content::TestDevToolsProtocolClient* client() {
    return devtools_client_.get();
  }

 private:
  void InstallAndLaunchIWA() {
    auto app = IsolatedWebAppBuilder(
                   ManifestBuilder().AddPermissionsPolicyWildcard(
                       network::mojom::PermissionsPolicyFeature::kSmartCard))
                   .BuildBundle();

    app->TrustSigningKey();
    auto install_result = app->Install(profile());
    ASSERT_TRUE(install_result.has_value());

    auto url_info = install_result.value();
    app_frame_ = OpenApp(url_info.app_id());
    ASSERT_TRUE(app_frame_);
  }

  void AttachDevTools() {
    devtools_client_ = std::make_unique<content::TestDevToolsProtocolClient>();
    devtools_client_->AttachToWebContents(
        content::WebContents::FromRenderFrameHost(app_frame_));
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::TestDevToolsProtocolClient> devtools_client_;
  raw_ptr<content::RenderFrameHost> app_frame_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest, EnableDisableEmulation) {
  // Disable (Should be successful even if already disabled - Idempotency).
  EXPECT_THAT(SendCommand("SmartCardEmulation.disable"), IsSuccess());

  // Enable (Should activate the override).
  EXPECT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());

  // Re-Enable (Should be successful - Idempotency).
  EXPECT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());

  // Disable (Should clean up).
  EXPECT_THAT(SendCommand("SmartCardEmulation.disable"), IsSuccess());
}

}  // namespace web_app
