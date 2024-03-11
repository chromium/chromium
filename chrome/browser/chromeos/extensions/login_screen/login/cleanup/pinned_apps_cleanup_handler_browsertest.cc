// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/pinned_apps_cleanup_handler.h"

#include <string>

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/app_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::extensions::Extension;

}  // namespace

class PinnedAppsCleanupHandlerBrowserTest
    : public extensions::ExtensionBrowserTest {
 protected:
  ChromeShelfController* controller() {
    return ChromeShelfController::instance();
  }

  const std::string CreateShortcut(const char* name) {
    const Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII(name));
    const std::string app_id = extension->id();

    // Pin the app.
    controller()->CreateAppItem(
        std::make_unique<AppShortcutShelfItemController>(ash::ShelfID(app_id)),
        ash::STATUS_CLOSED, /*pinned=*/true, /*title=*/std::u16string());

    return app_id;
  }
};

IN_PROC_BROWSER_TEST_F(PinnedAppsCleanupHandlerBrowserTest,
                       UserPinnedAppsCleanup) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("app1"));
  const std::string policy_pinned_id = extension->id();

  // App 1 is pinned by policy.
  base::Value::List policy_value;
  base::Value::Dict entry;
  entry.Set(ChromeShelfPrefs::kPinnedAppsPrefAppIDKey, policy_pinned_id);
  policy_value.Append(std::move(entry));
  profile()->GetPrefs()->SetList(prefs::kPolicyPinnedLauncherApps,
                                 std::move(policy_value));

  // App 2 and App 3 are installed and user pinned.
  const std::string user_pinned_id_1 = CreateShortcut("app2");
  const std::string user_pinned_id_2 = CreateShortcut("app3");

  EXPECT_TRUE(controller()->IsAppPinned(policy_pinned_id));
  EXPECT_TRUE(controller()->IsAppPinned(user_pinned_id_1));
  EXPECT_TRUE(controller()->IsAppPinned(user_pinned_id_2));

  // Do cleanup.
  std::unique_ptr<chromeos::PinnedAppsCleanupHandler>
      pinned_apps_cleanup_handler =
          std::make_unique<chromeos::PinnedAppsCleanupHandler>();
  pinned_apps_cleanup_handler->Cleanup(base::DoNothing());

  // Only the user pinned apps have their pin removed.
  EXPECT_TRUE(controller()->IsAppPinned(policy_pinned_id));
  EXPECT_FALSE(controller()->IsAppPinned(user_pinned_id_1));
  EXPECT_FALSE(controller()->IsAppPinned(user_pinned_id_2));
}
