// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/web_apps_chromeos.h"

#include <memory>

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/display/display.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace {

void CheckShortcut(const ui::SimpleMenuModel& model,
                   int index,
                   int shortcut_index,
                   const std::u16string& label,
                   base::Optional<SkColor> color) {
  EXPECT_EQ(model.GetTypeAt(index), ui::MenuModel::TYPE_COMMAND);
  EXPECT_EQ(model.GetCommandIdAt(index),
            ash::LAUNCH_APP_SHORTCUT_FIRST + shortcut_index);
  EXPECT_EQ(model.GetLabelAt(index), label);

  ui::ImageModel icon = model.GetIconAt(index);
  if (color.has_value()) {
    EXPECT_FALSE(icon.GetImage().IsEmpty());
    EXPECT_EQ(icon.GetImage().AsImageSkia().bitmap()->getColor(15, 15), color);
  } else {
    EXPECT_TRUE(icon.IsEmpty());
  }
}

void CheckSeparator(const ui::SimpleMenuModel& model, int index) {
  EXPECT_EQ(model.GetTypeAt(index), ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(model.GetCommandIdAt(index), -1);
}

}  // namespace

class WebAppsWebAppsChromeOsBrowserTest
    : public web_app::WebAppControllerBrowserTest {
 public:
  WebAppsWebAppsChromeOsBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kAppServiceAdaptiveIcon,
         features::kDesktopPWAsAppIconShortcutsMenuUI},
        {});
  }
  ~WebAppsWebAppsChromeOsBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppsWebAppsChromeOsBrowserTest, ShortcutIcons) {
  const GURL app_url =
      https_server()->GetURL("/web_app_shortcuts/shortcuts.html");
  const web_app::AppId app_id =
      web_app::InstallWebAppFromPage(browser(), app_url);
  LaunchWebAppBrowser(app_id);

  // Wait for app service to see the newly installed app.
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->FlushMojoCallsForTesting();

  std::unique_ptr<ui::SimpleMenuModel> result;
  {
    ash::ShelfModel* const shelf_model = ash::ShelfModel::Get();
    shelf_model->PinAppWithID(app_id);
    ash::ShelfItemDelegate* const delegate =
        shelf_model->GetShelfItemDelegate(ash::ShelfID(app_id));
    base::RunLoop run_loop;
    delegate->GetContextMenu(
        display::Display::GetDefaultDisplay().id(),
        base::BindLambdaForTesting(
            [&run_loop, &result](std::unique_ptr<ui::SimpleMenuModel> model) {
              result = std::move(model);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Shortcuts appear last in the context menu.
  // See /web_app_shortcuts/shortcuts.json for shortcut icon definitions.
  int index = result->GetItemCount() - 11;

  // Purpose |any| by default.
  CheckShortcut(*result, index++, 0, u"One", SK_ColorGREEN);
  CheckSeparator(*result, index++);
  // Purpose |maskable| takes precedence over |any|.
  CheckShortcut(*result, index++, 1, u"Two", SK_ColorBLUE);
  CheckSeparator(*result, index++);
  // Purpose |any|.
  CheckShortcut(*result, index++, 2, u"Three", SK_ColorYELLOW);
  CheckSeparator(*result, index++);
  // Purpose |any| and |maskable|.
  CheckShortcut(*result, index++, 3, u"Four", SK_ColorCYAN);
  CheckSeparator(*result, index++);
  // Purpose |maskable|.
  CheckShortcut(*result, index++, 4, u"Five", SK_ColorMAGENTA);
  CheckSeparator(*result, index++);
  // No icons.
  CheckShortcut(*result, index++, 5, u"Six", base::nullopt);
  EXPECT_EQ(index, result->GetItemCount());
}
