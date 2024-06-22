// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/controlled_frame/controlled_frame_test_base.h"

#include <string>
#include <string_view>

#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"

namespace controlled_frame {

ControlledFrameTestBase::ControlledFrameTestBase() = default;
ControlledFrameTestBase::~ControlledFrameTestBase() = default;

void ControlledFrameTestBase::StartContentServer(
    std::string_view chrome_test_data_relative_dir) {
  embedded_https_test_server().ServeFilesFromSourceDirectory(
      GetChromeTestDataDir().AppendASCII(chrome_test_data_relative_dir));
  ASSERT_TRUE(embedded_https_test_server().Start());
}

web_app::IsolatedWebAppUrlInfo
ControlledFrameTestBase::CreateAndInstallEmptyApp(
    const web_app::ManifestBuilder& manifest_builder) {
  app_ = web_app::IsolatedWebAppBuilder(manifest_builder).BuildBundle();
  app_->TrustSigningKey();
  base::expected<web_app::IsolatedWebAppUrlInfo, std::string> url_info =
      app_->Install(profile());
  CHECK(url_info.has_value()) << url_info.error();
  return url_info.value();
}

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
