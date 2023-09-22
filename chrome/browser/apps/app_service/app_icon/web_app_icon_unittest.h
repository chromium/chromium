// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_WEB_APP_ICON_UNITTEST_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_WEB_APP_ICON_UNITTEST_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
enum ResourceScaleFactor : int;
}

namespace web_app {
class WebApp;
class WebAppIconManager;
class WebAppProvider;
class WebAppSyncBridge;
}  // namespace web_app

class Profile;

namespace apps {
class WebAppIconFactoryTest : public testing::Test {
 public:
  // TODO(crbug.com/1462253): Also test with Lacros flags enabled.
  WebAppIconFactoryTest();

  ~WebAppIconFactoryTest() override;

  void SetUp() override;

  void WriteIcons(const std::string& app_id,
                  const std::vector<IconPurpose>& purposes,
                  const std::vector<int>& sizes_px,
                  const std::vector<SkColor>& colors);

  void RegisterApp(std::unique_ptr<web_app::WebApp> web_app);

  gfx::ImageSkia GenerateWebAppIcon(const std::string& app_id,
                                    IconPurpose purpose,
                                    const std::vector<int>& sizes_px,
                                    apps::ScaleToSize scale_to_size_in_px,
                                    bool skip_icon_effects = false);

  std::vector<uint8_t> GenerateWebAppCompressedIcon(
      const std::string& app_id,
      IconPurpose purpose,
      const std::vector<int>& sizes_px,
      apps::ScaleToSize scale_to_size_in_px);

  std::vector<uint8_t> GenerateWebAppCompressedIcon(
      const std::string& app_id,
      IconPurpose purpose,
      IconEffects icon_effects,
      const std::vector<int>& sizes_px,
      apps::ScaleToSize scale_to_size_in_px,
      float scale);

  gfx::ImageSkia LoadIconFromWebApp(const std::string& app_id,
                                    apps::IconEffects icon_effects);

  apps::IconValuePtr LoadCompressedIconBlockingFromWebApp(
      const std::string& app_id,
      apps::IconEffects icon_effects);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  apps::IconValuePtr GetWebAppCompressedIconData(
      const std::string& app_id,
      ui::ResourceScaleFactor scale_factor);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  web_app::WebAppIconManager& icon_manager() { return *icon_manager_; }

  web_app::WebAppProvider& web_app_provider() { return *web_app_provider_; }

  web_app::WebAppSyncBridge& sync_bridge() { return *sync_bridge_; }

  Profile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<web_app::WebAppProvider> web_app_provider_;
  raw_ptr<web_app::WebAppIconManager> icon_manager_;
  raw_ptr<web_app::WebAppSyncBridge> sync_bridge_;
};
}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_WEB_APP_ICON_UNITTEST_H_
