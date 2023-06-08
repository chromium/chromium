// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/apps/app_service/publishers/shortcut_publisher.h"

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class FakeShortcutPublisher : public ShortcutPublisher {
 public:
  FakeShortcutPublisher(AppServiceProxy* proxy,
                        AppType app_type,
                        Shortcuts initial_shortcuts)
      : ShortcutPublisher(proxy),
        initial_shortcuts_(std::move(initial_shortcuts)) {
    RegisterShortcutPublisher(app_type);
    CreateInitialShortcuts();
  }

  void CreateInitialShortcuts() {
    for (const auto& shortcut : initial_shortcuts_) {
      ShortcutPublisher::PublishShortcut(shortcut->Clone());
    }
  }

 private:
  Shortcuts initial_shortcuts_;
};

class ShortcutPublisherTest : public testing::Test {
 public:
  // testing::Test implementation.
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kCrosWebAppShortcutUiUpdate);
    profile_ = std::make_unique<TestingProfile>();
  }

  Profile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ShortcutPublisherTest, PublishExistingShortcuts) {
  Shortcuts initial_extension_shortcuts;
  ShortcutPtr shortcut_1 = std::make_unique<Shortcut>("app_id_1", "local_id_1");
  shortcut_1->name = "name1";

  ShortcutPtr shortcut_2 = std::make_unique<Shortcut>("app_id_1", "local_id_2");
  shortcut_2->name = "name2";
  shortcut_2->shortcut_source = ShortcutSource::kDeveloper;

  initial_extension_shortcuts.push_back(std::move(shortcut_1));
  initial_extension_shortcuts.push_back(std::move(shortcut_2));
  AppServiceProxy* proxy = AppServiceProxyFactory::GetForProfile(profile());
  FakeShortcutPublisher fake_extension_publisher(
      proxy, AppType::kExtension, CloneShortcuts(initial_extension_shortcuts));

  Shortcuts initial_web_app_shortcuts;
  ShortcutPtr shortcut_3 = std::make_unique<Shortcut>("app_id_2", "local_id_3");
  shortcut_3->name = "name3";
  shortcut_3->shortcut_source = ShortcutSource::kUser;
  initial_web_app_shortcuts.push_back(std::move(shortcut_3));
  FakeShortcutPublisher fake_web_app_publisher(
      proxy, AppType::kWeb, CloneShortcuts(initial_web_app_shortcuts));

  ShortcutRegistryCache* cache = proxy->ShortcutRegistryCache();
  ASSERT_TRUE(cache);

  ASSERT_EQ(cache->GetAllShortcuts().size(), 3u);

  ASSERT_TRUE(cache->HasShortcut(initial_extension_shortcuts[0]->shortcut_id));
  ShortcutView stored_shortcut1 =
      cache->GetShortcut(initial_extension_shortcuts[0]->shortcut_id);
  EXPECT_EQ(*(stored_shortcut1->Clone()), *initial_extension_shortcuts[0]);

  ASSERT_TRUE(cache->HasShortcut(initial_extension_shortcuts[1]->shortcut_id));
  ShortcutView stored_shortcut2 =
      cache->GetShortcut(initial_extension_shortcuts[1]->shortcut_id);
  EXPECT_EQ(*(stored_shortcut2->Clone()), *initial_extension_shortcuts[1]);

  ASSERT_TRUE(cache->HasShortcut(initial_web_app_shortcuts[0]->shortcut_id));
  ShortcutView stored_shortcut3 =
      cache->GetShortcut(initial_web_app_shortcuts[0]->shortcut_id);
  EXPECT_EQ(*(stored_shortcut3->Clone()), *initial_web_app_shortcuts[0]);
}

}  // namespace apps
