// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace web_app {

namespace {

constexpr char kCrossOriginIframeHtml[] = R"(
<!DOCTYPE html>
<html>
<head>
  <title>Attacker Cross-Origin iframe</title>
</head>
<body>
  <script>
    function runPoC() {
      if (typeof Mojo !== 'undefined') {
        try {
          const pipe = Mojo.createMessagePipe();
          Mojo.bindInterface('blink.mojom.MediaStreamDispatcherHost',
                             pipe.handle0);
          window.domAutomationController &&
              window.domAutomationController.send('MOJO_IPC_INJECTED');
        } catch (err) {
          window.domAutomationController &&
              window.domAutomationController.send('ERROR_MOJO');
        }
      } else {
        window.domAutomationController &&
            window.domAutomationController.send('BLOCKED_NO_MOJO');
      }
    }
    window.onload = () => setTimeout(runPoC, 300);
  </script>
</body>
</html>
)";

}  // namespace

class MultiCaptureIframeBrowserTest : public IsolatedWebAppBrowserTestHarness {
 public:
  MultiCaptureIframeBrowserTest() = default;
  ~MultiCaptureIframeBrowserTest() override = default;

  void SetUpOnMainThread() override {
    embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
        [](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url == "/cross_origin_iframe.html") {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content_type("text/html");
            response->set_content(kCrossOriginIframeHtml);
            return response;
          }
          return nullptr;
        }));
    IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();
  }

  void SetAllowedOriginsPolicy(
      const std::vector<std::string>& allow_listed_origins) {
    policy::PolicyMap policies;
    base::ListValue allowed_origins;
    for (const auto& origin : allow_listed_origins) {
      allowed_origins.Append(base::Value(origin));
    }
    policies.Set(policy::key::kMultiScreenCaptureAllowedForUrls,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 base::Value(std::move(allowed_origins)), nullptr);
    provider_.UpdateChromePolicy(policies);
  }

  void SetUpInProcessBrowserTestFixture() override {
    IsolatedWebAppBrowserTestHarness::SetUpInProcessBrowserTestFixture();
    provider_.SetDefaultReturns(/*is_initialization_complete_return=*/true,
                                /*is_first_policy_load_complete_return=*/true);
    SetAllowedOriginsPolicy(
        {"isolated-app://" +
         web_app::test::GetDefaultEd25519WebBundleId().id()});
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

IN_PROC_BROWSER_TEST_F(MultiCaptureIframeBrowserTest,
                       CrossOriginIframeCaptureAttempt) {
  // 1. Build Isolated Web App bundle with all-screens-capture permission policy
  auto manifest_builder =
      ManifestBuilder().SetName("Test IWA").SetVersion("1.0.0");
  manifest_builder.AddPermissionsPolicy(
      network::mojom::PermissionsPolicyFeature::kAllScreensCapture,
      /*self=*/true,
      /*origins=*/{embedded_https_test_server().GetOrigin()});
  manifest_builder.AddPermissionsPolicy(
      network::mojom::PermissionsPolicyFeature::kDisplayCapture, /*self=*/true,
      /*origins=*/{embedded_https_test_server().GetOrigin()});

  auto builder = IsolatedWebAppBuilder(std::move(manifest_builder));
  builder.AddHtml(
      "/", R"(
    <!DOCTYPE html>
    <html>
    <head><title>IWA Trusted Main Frame</title></head>
    <body>
      <h1>Trusted IWA Main Frame</h1>
      <iframe id="attacker_frame" src=")" +
               embedded_https_test_server()
                   .GetURL("/cross_origin_iframe.html")
                   .spec() +
               R"(" allow="all-screens-capture *; display-capture *"></iframe>
    </body>
    </html>
  )");

  auto app = builder.BuildBundle(web_app::test::GetDefaultEd25519KeyPair());
  app->TrustSigningKey();
  auto url_info = app->Install(profile()).value();

  // 3. Open the IWA and wait for navigation
  content::RenderFrameHost* main_rfh = OpenApp(url_info.app_id());
  ASSERT_TRUE(main_rfh);

  // 4. Find the cross-origin iframe
  content::RenderFrameHost* child_rfh = content::ChildFrameAt(main_rfh, 0);
  ASSERT_TRUE(child_rfh);
  EXPECT_NE(main_rfh->GetLastCommittedOrigin(),
            child_rfh->GetLastCommittedOrigin());

  EXPECT_FALSE(
      content::GetContentClientForTesting()->browser()->IsMultiCaptureAllowed(
          child_rfh))
      << "Expected patched IsMultiCaptureAllowed to block cross-origin iframe.";
}

}  // namespace web_app
