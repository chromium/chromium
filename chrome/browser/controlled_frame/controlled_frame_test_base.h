// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_TEST_BASE_H_
#define CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_TEST_BASE_H_

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/version_info/version_info.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
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

enum class FeatureSetting {
  UNINITIALIZED = 0,
  NONE = 1,
  DISABLED = 2,
  ENABLED = 3,
};

enum class FlagSetting {
  UNINITIALIZED = 0,
  NONE = 1,
  EXPERIMENTAL = 2,
  CONTROLLED_FRAME = 3,
};

class ControlledFrameTestBase
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  ControlledFrameTestBase();
  ControlledFrameTestBase(const version_info::Channel&,
                          const FeatureSetting&,
                          const FlagSetting&);
  ControlledFrameTestBase(const ControlledFrameTestBase&) = delete;
  ControlledFrameTestBase& operator=(const ControlledFrameTestBase&) = delete;
  ~ControlledFrameTestBase() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

  std::string ConfigToString();

 protected:
  void StartContentServer(std::string_view chrome_test_data_relative_dir);

  web_app::IsolatedWebAppUrlInfo CreateAndInstallEmptyApp(
      const web_app::ManifestBuilder& manifest_builder);

  [[nodiscard]] bool CreateControlledFrame(content::RenderFrameHost* frame,
                                           const GURL& src);

  // If |controlled_frame_host_name| is null, then uses the default hostname of
  // |embedded_https_test_server|
  std::pair<content::RenderFrameHost*, content::RenderFrameHost*>
  InstallAndOpenIwaThenCreateControlledFrame(
      std::optional<std::string_view> controlled_frame_host_name,
      std::string_view controlled_frame_src_relative_url,
      web_app::ManifestBuilder manfest_builder = web_app::ManifestBuilder());

  extensions::WebViewGuest* GetWebViewGuest(
      content::RenderFrameHost* embedder_frame);

  version_info::Channel channel() { return channel_.channel(); }
  FeatureSetting feature_setting() { return feature_setting_; }
  FlagSetting flag_setting() { return flag_setting_; }

  // |ConfigureEnvironment| ensures that a specific set of environment
  // parameters are configured before this test run starts. The parameters cover
  // the channel, feature, and flag configuration for the test. The channel is
  // configured using ScopedCurrentChannel. The rest of the settings are
  // configured using other parts of the feature system and must be run during
  // the constructor.
  //
  // Most tests will be able to use the overloaded ctor. However, any tests
  // built using testing::Combine() won't be able to use the overloaded ctor.
  // Instead, they use their own ctor, pull the test values apart from the
  // testing parameter, and then call |ConfigureEnvironment| directly.
  void ConfigureEnvironment();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app_;

  extensions::ScopedCurrentChannel channel_;
  FeatureSetting feature_setting_{FeatureSetting::UNINITIALIZED};
  FlagSetting flag_setting_{FlagSetting::UNINITIALIZED};
};

}  // namespace controlled_frame

#endif  // CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_TEST_BASE_H_
