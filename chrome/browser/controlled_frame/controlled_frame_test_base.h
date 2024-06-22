// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_TEST_BASE_H_
#define CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_TEST_BASE_H_

#include <memory>
#include <string_view>

#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "third_party/blink/public/common/features.h"

class GURL;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace extensions {
class WebViewGuest;
}

namespace web_app {
class ManifestBuilder;
class ScopedBundledIsolatedWebApp;
}  // namespace web_app

namespace controlled_frame {

class ControlledFrameTestBase
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  ControlledFrameTestBase();
  ControlledFrameTestBase(const ControlledFrameTestBase&) = delete;
  ControlledFrameTestBase& operator=(const ControlledFrameTestBase&) = delete;
  ~ControlledFrameTestBase() override;

 protected:
  void StartContentServer(std::string_view chrome_test_data_relative_dir);

  web_app::IsolatedWebAppUrlInfo CreateAndInstallEmptyApp(
      const web_app::ManifestBuilder& manifest_builder);

  [[nodiscard]] bool CreateControlledFrame(content::RenderFrameHost* frame,
                                           const GURL& src);

  extensions::WebViewGuest* GetWebViewGuest(
      content::RenderFrameHost* embedder_frame);

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kIsolateSandboxedIframes};
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app_;
};

}  // namespace controlled_frame

#endif  // CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_TEST_BASE_H_
