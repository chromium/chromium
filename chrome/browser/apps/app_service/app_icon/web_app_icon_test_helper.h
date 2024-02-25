// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_WEB_APP_ICON_TEST_HELPER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_WEB_APP_ICON_TEST_HELPER_H_

#include <string>
#include <vector>

#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "components/services/app_service/public/cpp/icon_types.h"

namespace ui {
enum ResourceScaleFactor : int;
}

namespace web_app {
class WebApp;
class WebAppIconManager;
}  // namespace web_app

namespace apps {

class WebAppIconTestHelper {
 public:
  explicit WebAppIconTestHelper(Profile* profile);
  WebAppIconTestHelper(const WebAppIconTestHelper&) = delete;
  WebAppIconTestHelper& operator=(const WebAppIconTestHelper&) = delete;
  ~WebAppIconTestHelper();

  void WriteIcons(const std::string& app_id,
                  const std::vector<web_app::IconPurpose>& purposes,
                  const std::vector<int>& sizes_px,
                  const std::vector<SkColor>& colors);

  gfx::ImageSkia GenerateWebAppIcon(const std::string& app_id,
                                    web_app::IconPurpose purpose,
                                    const std::vector<int>& sizes_px,
                                    apps::ScaleToSize scale_to_size_in_px,
                                    bool skip_icon_effects = false);

  std::vector<uint8_t> GenerateWebAppCompressedIcon(
      const std::string& app_id,
      web_app::IconPurpose purpose,
      const std::vector<int>& sizes_px,
      apps::ScaleToSize scale_to_size_in_px);

  std::vector<uint8_t> GenerateWebAppCompressedIcon(
      const std::string& app_id,
      web_app::IconPurpose purpose,
      IconEffects icon_effects,
      const std::vector<int>& sizes_px,
      apps::ScaleToSize scale_to_size_in_px,
      float scale);

  void RegisterApp(std::unique_ptr<web_app::WebApp> web_app);

 private:
  web_app::WebAppIconManager& icon_manager();

  raw_ptr<Profile> profile_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_WEB_APP_ICON_TEST_HELPER_H_
