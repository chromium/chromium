// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/apps/app_service/publishers/browser_shortcuts_crosapi_publisher.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/app_service/web_apps_with_shortcuts_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class BrowserShortcutsCrosapiPublisherTest
    : public testing::Test,
      public web_app::WebAppsWithShortcutsTest {
 public:
  void SetUp() override {
    EnableCrosWebAppShortcutUiUpdate(true);
    profile_ = std::make_unique<TestingProfile>();
    auto* app_service_proxy =
        apps::AppServiceProxyFactory::GetForProfile(profile());
    browser_shortcuts_crosapi_publisher_ =
        std::make_unique<apps::BrowserShortcutsCrosapiPublisher>(
            app_service_proxy);

    browser_shortcuts_crosapi_publisher_->RegisterCrosapiHost(
        app_shortcut_publisher_remote_.BindNewPipeAndPassReceiver());
  }
  Profile* profile() { return profile_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  mojo::Remote<crosapi::mojom::AppShortcutPublisher>
      app_shortcut_publisher_remote_;
  std::unique_ptr<apps::BrowserShortcutsCrosapiPublisher>
      browser_shortcuts_crosapi_publisher_;
};

TEST_F(BrowserShortcutsCrosapiPublisherTest, PublishShortcuts) {
  apps::ShortcutRegistryCache* cache =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->ShortcutRegistryCache();
  ASSERT_EQ(cache->GetAllShortcuts().size(), 0u);

  std::vector<ShortcutPtr> shortcuts;
  ShortcutPtr shortcut_1 = std::make_unique<Shortcut>("app_id_1", "local_id_1");
  shortcut_1->shortcut_source = ShortcutSource::kUser;
  ShortcutPtr shortcut_2 = std::make_unique<Shortcut>("app_id_1", "local_id_2");
  shortcut_2->shortcut_source = ShortcutSource::kUser;
  shortcuts.push_back(shortcut_1->Clone());
  shortcuts.push_back(shortcut_2->Clone());
  base::RunLoop runloop;
  app_shortcut_publisher_remote_->PublishShortcuts(std::move(shortcuts),
                                                   runloop.QuitClosure());
  runloop.Run();
  ASSERT_EQ(cache->GetAllShortcuts().size(), 2u);

  ASSERT_TRUE(cache->HasShortcut(shortcut_1->shortcut_id));
  ShortcutView stored_shortcut1 = cache->GetShortcut(shortcut_1->shortcut_id);
  EXPECT_EQ(*(stored_shortcut1->Clone()), *shortcut_1);

  ShortcutView stored_shortcut2 = cache->GetShortcut(shortcut_2->shortcut_id);
  EXPECT_EQ(*(stored_shortcut2->Clone()), *shortcut_2);
}

}  // namespace apps
