// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/controlled_frame/controlled_frame_test_base.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"

namespace controlled_frame {

ControlledFrameTestBase::ControlledFrameTestBase()
    : channel_(version_info::Channel::DEFAULT),
      feature_setting_(FeatureSetting::ENABLED),
      flag_setting_(FlagSetting::CONTROLLED_FRAME) {
  ConfigureEnvironment();
}

ControlledFrameTestBase::ControlledFrameTestBase(
    const version_info::Channel& channel,
    const FeatureSetting& feature_setting,
    const FlagSetting& flag_setting)
    : channel_(channel),
      feature_setting_(feature_setting),
      flag_setting_(flag_setting) {
  ConfigureEnvironment();
}

ControlledFrameTestBase::~ControlledFrameTestBase() = default;

void ControlledFrameTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  if (flag_setting() == FlagSetting::EXPERIMENTAL) {
    // Enable experimental web platform features as a proxy for enabling
    // Controlled Frame.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  } else if (flag_setting() == FlagSetting::CONTROLLED_FRAME) {
    // Enable just the Controlled Frame API.
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "ControlledFrame");
  }
}

std::string ControlledFrameTestBase::ConfigToString() {
  return "channel=" + base::NumberToString(int(channel())) +
         "; feature=" + base::NumberToString(int(feature_setting())) +
         "; flag=" + base::NumberToString(int(flag_setting()));
}

void ControlledFrameTestBase::ConfigureEnvironment() {
  // Initialize |scoped_feature_list_|. Start by initializing |feature_list|.
  auto feature_list = std::make_unique<base::FeatureList>();
  // IsolatedWebAppBrowserTestHarness enables features::kIsolatedWebApps and
  // features::kIsolatedWebAppDevMode.
  std::vector<base::test::FeatureRef> enabled_features = {
      blink::features::kIsolateSandboxedIframes};
  std::vector<base::test::FeatureRef> disabled_features = {};
  switch (feature_setting()) {
    case FeatureSetting::UNINITIALIZED:
      FAIL() << "FeatureSetting should be initialized.";
    case FeatureSetting::NONE:
      break;
    case FeatureSetting::DISABLED:
      disabled_features.push_back(blink::features::kControlledFrame);
      break;
    case FeatureSetting::ENABLED:
      enabled_features.push_back(blink::features::kControlledFrame);
      break;
  }
  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
}

void ControlledFrameTestBase::StartContentServer(
    std::string_view chrome_test_data_relative_dir) {
  embedded_https_test_server().ServeFilesFromSourceDirectory(
      GetChromeTestDataDir().AppendASCII(chrome_test_data_relative_dir));
  ASSERT_TRUE(embedded_https_test_server().Start());
}

web_app::IsolatedWebAppUrlInfo
ControlledFrameTestBase::CreateAndInstallEmptyApp(
    const web_app::ManifestBuilder& manifest_builder) {
  auto updated_manifest_builder = manifest_builder;
  updated_manifest_builder.AddPermissionsPolicy(
      blink::mojom::PermissionsPolicyFeature::kControlledFrame, /*self=*/true,
      /*origins=*/{});
  app_ = web_app::IsolatedWebAppBuilder(updated_manifest_builder).BuildBundle();
  app_->TrustSigningKey();
  base::expected<web_app::IsolatedWebAppUrlInfo, std::string> url_info =
      app_->Install(profile());
  CHECK(url_info.has_value()) << url_info.error();
  return url_info.value();
}

// Keep this in sync with web_kiosk_base_test.cc.
[[nodiscard]] bool ControlledFrameTestBase::CreateControlledFrame(
    content::RenderFrameHost* frame,
    const GURL& src) {
  static std::string kCreateControlledFrame = R"(
    new Promise((resolve, reject) => {
      const controlledframe = document.createElement('controlledframe');
      if (!('src' in controlledframe)) {
        // Tag is undefined or generates a malformed response.
        reject('FAIL');
        return;
      }
      controlledframe.setAttribute('src', $1);
      controlledframe.addEventListener('loadstop', resolve);
      controlledframe.addEventListener('loadabort', reject);
      document.body.appendChild(controlledframe);
    });
)";
  return ExecJs(frame, content::JsReplace(kCreateControlledFrame, src));
}

std::pair<content::RenderFrameHost*, content::RenderFrameHost*>
ControlledFrameTestBase::InstallAndOpenIwaThenCreateControlledFrame(
    std::optional<std::string_view> controlled_frame_host_name,
    std::string_view controlled_frame_src_relative_url,
    web_app::ManifestBuilder manfest_builder) {
  CHECK(embedded_https_test_server().Started())
      << "Controlled Frame content server has not been started.";

  web_app::IsolatedWebAppUrlInfo url_info =
      CreateAndInstallEmptyApp(manfest_builder);
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());
  CHECK(app_frame);

  GURL controlled_frame_src = controlled_frame_host_name.has_value()
                                  ? embedded_https_test_server().GetURL(
                                        controlled_frame_host_name.value(),
                                        controlled_frame_src_relative_url)
                                  : embedded_https_test_server().GetURL(
                                        controlled_frame_src_relative_url);

  CHECK(CreateControlledFrame(app_frame, controlled_frame_src));

  extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_frame);
  CHECK(web_view_guest);

  content::RenderFrameHost* controlled_frame =
      web_view_guest->GetGuestMainFrame();
  CHECK(controlled_frame);

  return {app_frame, controlled_frame};
}

extensions::WebViewGuest* ControlledFrameTestBase::GetWebViewGuest(
    content::RenderFrameHost* embedder_frame) {
  extensions::WebViewGuest* web_view_guest = nullptr;
  embedder_frame->ForEachRenderFrameHostWithAction(
      [&web_view_guest](content::RenderFrameHost* rfh) {
        if (auto* web_view =
                extensions::WebViewGuest::FromRenderFrameHost(rfh)) {
          web_view_guest = web_view;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
  return web_view_guest;
}

}  // namespace controlled_frame
