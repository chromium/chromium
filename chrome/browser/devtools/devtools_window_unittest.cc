// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_window.h"

#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/ui/webui/devtools_ui.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class Profile;

class DevToolsWindowTest : public testing::Test {
 public:
  GURL GetURL(version_info::Channel channel) {
    Profile* profile = nullptr;
    std::string frontend_url;
    std::string panel;
    bool can_dock = false;
    bool has_other_clients = false;
    bool browser_connection = false;
    return DevToolsWindow::GetDevToolsURL(
        profile, DevToolsWindow::FrontendType::kFrontendDefault, channel,
        frontend_url, can_dock, panel, has_other_clients, browser_connection);
  }
};

TEST_F(DevToolsWindowTest, GetDevtoolsURL) {
  std::string expected =
      "devtools://devtools/bundled/devtools_app.html?remoteBase=" +
      DevToolsUI::GetRemoteBaseURL().spec();
  if (base::FeatureList::IsEnabled(::features::kDevToolsTabTarget)) {
    expected += "&targetType=tab";
  }

  EXPECT_EQ(expected, GetURL(version_info::Channel::UNKNOWN));
  EXPECT_EQ(expected, GetURL(version_info::Channel::CANARY));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOS in dev channel and above will set consolePaste=blockwebui.
  expected += "&consolePaste=blockwebui";
#endif
  EXPECT_EQ(expected, GetURL(version_info::Channel::DEV));
  EXPECT_EQ(expected, GetURL(version_info::Channel::BETA));
  EXPECT_EQ(expected, GetURL(version_info::Channel::STABLE));
}
