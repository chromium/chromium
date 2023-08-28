// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/apps/app_service/publishers/shortcut_publisher.h"

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_constants.h"

namespace apps {

class FakeShortcutPublisher : public ShortcutPublisher {
 public:
  FakeShortcutPublisher(AppServiceProxy* proxy,
                        AppType app_type,
                        const Shortcuts& initial_shortcuts)
      : ShortcutPublisher(proxy) {
    RegisterShortcutPublisher(app_type);
    CreateInitialShortcuts(initial_shortcuts);
  }

  void CreateInitialShortcuts(const Shortcuts& initial_shortcuts) {
    for (const auto& shortcut : initial_shortcuts) {
      ShortcutPublisher::PublishShortcut(shortcut->Clone());
    }
  }

  void LaunchShortcut(const std::string& host_app_id,
                      const std::string& local_id,
                      int64_t display_id) override {
    shortcut_launched_ = true;
    host_app_id_ = host_app_id;
    local_id_ = local_id;
    display_id_ = display_id;
  }

  void RemoveShortcut(const std::string& host_app_id,
                      const std::string& local_shortcut_id,
                      UninstallSource uninstall_source) override {
    ShortcutPublisher::ShortcutRemoved(
        apps::GenerateShortcutId(host_app_id, local_shortcut_id));
  }

  void ClearPreviousLaunch() {
    // Clear previous launch;
    shortcut_launched_ = false;
    host_app_id_ = "";
    local_id_ = "";
    display_id_ = display::kDefaultDisplayId;
  }

  void VerifyShortcutLaunch(const std::string& expected_host_app_id,
                            const std::string& expected_local_id,
                            int64_t expected_display_id) {
    EXPECT_TRUE(shortcut_launched_);
    EXPECT_EQ(expected_host_app_id, host_app_id_);
    EXPECT_EQ(expected_local_id, local_id_);
    EXPECT_EQ(expected_display_id, display_id_);
  }

  void ShortcutRemoved(const ShortcutId& shortcut_id) {
    ShortcutPublisher::ShortcutRemoved(shortcut_id);
  }

 private:
  bool shortcut_launched_ = false;
  std::string host_app_id_;
  std::string local_id_;
  int64_t display_id_;
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
  AppServiceProxy* proxy() {
    return AppServiceProxyFactory::GetForProfile(profile());
  }

  void PublishApp(AppType type, const std::string& app_id) {
    std::vector<apps::AppPtr> app_deltas;
    app_deltas.push_back(apps::AppPublisher::MakeApp(
        type, app_id, apps::Readiness::kReady, "Some App Name",
        apps::InstallReason::kUser, apps::InstallSource::kSystem));
    proxy()->AppRegistryCache().OnApps(std::move(app_deltas), type,
                                       /* should_notify_initialized */ true);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ShortcutPublisherTest, PublishExistingShortcuts) {
  ShortcutPtr shortcut_1 = std::make_unique<Shortcut>("app_id_1", "local_id_1");
  shortcut_1->name = "name1";

  ShortcutPtr shortcut_2 = std::make_unique<Shortcut>("app_id_1", "local_id_2");
  shortcut_2->name = "name2";
  shortcut_2->shortcut_source = ShortcutSource::kDeveloper;

  Shortcuts initial_chrome_shortcuts;
  initial_chrome_shortcuts.push_back(std::move(shortcut_1));
  initial_chrome_shortcuts.push_back(std::move(shortcut_2));

  FakeShortcutPublisher fake_chrome_app_publisher(proxy(), AppType::kChromeApp,
                                                  initial_chrome_shortcuts);

  ShortcutPtr shortcut_3 = std::make_unique<Shortcut>("app_id_2", "local_id_3");
  shortcut_3->name = "name3";
  shortcut_3->shortcut_source = ShortcutSource::kUser;

  Shortcuts initial_web_app_shortcuts;
  initial_web_app_shortcuts.push_back(std::move(shortcut_3));
  FakeShortcutPublisher fake_web_app_publisher(proxy(), AppType::kWeb,
                                               initial_web_app_shortcuts);

  ShortcutRegistryCache* cache = proxy()->ShortcutRegistryCache();
  ASSERT_TRUE(cache);

  ASSERT_EQ(cache->GetAllShortcuts().size(), 3u);

  ASSERT_TRUE(cache->HasShortcut(initial_chrome_shortcuts[0]->shortcut_id));
  ShortcutView stored_shortcut1 =
      cache->GetShortcut(initial_chrome_shortcuts[0]->shortcut_id);
  EXPECT_EQ(*(stored_shortcut1->Clone()), *initial_chrome_shortcuts[0]);

  ASSERT_TRUE(cache->HasShortcut(initial_chrome_shortcuts[1]->shortcut_id));
  ShortcutView stored_shortcut2 =
      cache->GetShortcut(initial_chrome_shortcuts[1]->shortcut_id);
  EXPECT_EQ(*(stored_shortcut2->Clone()), *initial_chrome_shortcuts[1]);

  ASSERT_TRUE(cache->HasShortcut(initial_web_app_shortcuts[0]->shortcut_id));
  ShortcutView stored_shortcut3 =
      cache->GetShortcut(initial_web_app_shortcuts[0]->shortcut_id);
  EXPECT_EQ(*(stored_shortcut3->Clone()), *initial_web_app_shortcuts[0]);
}

TEST_F(ShortcutPublisherTest, LaunchShortcut_CallsCorrectPublisher) {
  // Setup shortcuts in different publishers to verify the launch gets to the
  // correct publisher.
  ShortcutPtr shortcut_1 = std::make_unique<Shortcut>("app_id_1", "local_id_1");

  ShortcutPtr shortcut_2 = std::make_unique<Shortcut>("app_id_1", "local_id_2");

  Shortcuts initial_chrome_shortcuts;
  initial_chrome_shortcuts.push_back(std::move(shortcut_1));
  initial_chrome_shortcuts.push_back(std::move(shortcut_2));

  FakeShortcutPublisher fake_chrome_app_publisher(proxy(), AppType::kChromeApp,
                                                  initial_chrome_shortcuts);

  ShortcutPtr shortcut_3 = std::make_unique<Shortcut>("app_id_2", "local_id_3");

  Shortcuts initial_web_app_shortcuts;
  initial_web_app_shortcuts.push_back(std::move(shortcut_3));
  FakeShortcutPublisher fake_web_app_publisher(proxy(), AppType::kWeb,
                                               initial_web_app_shortcuts);

  // Add parent apps with corresponding app type so that correct publisher can
  // be found to launch the shortcut.
  PublishApp(apps::AppType::kChromeApp, "app_id_1");
  PublishApp(apps::AppType::kWeb, "app_id_2");

  int64_t display_id = display::kInvalidDisplayId;

  // Verify that shortcut launch command goes to the correct shortcut publisher
  // based on the parent app app type, with correct host app id and local
  // shortcut id.
  fake_chrome_app_publisher.ClearPreviousLaunch();
  proxy()->LaunchShortcut(initial_chrome_shortcuts[0]->shortcut_id, display_id);
  fake_chrome_app_publisher.VerifyShortcutLaunch(
      initial_chrome_shortcuts[0]->host_app_id,
      initial_chrome_shortcuts[0]->local_id, display_id);

  fake_chrome_app_publisher.ClearPreviousLaunch();
  proxy()->LaunchShortcut(initial_chrome_shortcuts[1]->shortcut_id, display_id);
  fake_chrome_app_publisher.VerifyShortcutLaunch(
      initial_chrome_shortcuts[1]->host_app_id,
      initial_chrome_shortcuts[1]->local_id, display_id);

  fake_web_app_publisher.ClearPreviousLaunch();
  proxy()->LaunchShortcut(initial_web_app_shortcuts[0]->shortcut_id,
                          display_id);
  fake_web_app_publisher.VerifyShortcutLaunch(
      initial_web_app_shortcuts[0]->host_app_id,
      initial_web_app_shortcuts[0]->local_id, display_id);
}

TEST_F(ShortcutPublisherTest, ShortcutRemoved) {
  ShortcutPtr shortcut_1 = std::make_unique<Shortcut>("app_id_1", "local_id_1");
  shortcut_1->name = "name1";

  ShortcutPtr shortcut_2 = std::make_unique<Shortcut>("app_id_1", "local_id_2");
  shortcut_2->name = "name2";
  shortcut_2->shortcut_source = ShortcutSource::kDeveloper;

  Shortcuts initial_chrome_shortcuts;
  initial_chrome_shortcuts.push_back(std::move(shortcut_1));
  initial_chrome_shortcuts.push_back(std::move(shortcut_2));

  FakeShortcutPublisher fake_chrome_app_publisher(proxy(), AppType::kChromeApp,
                                                  initial_chrome_shortcuts);

  ShortcutRegistryCache* cache = proxy()->ShortcutRegistryCache();
  ASSERT_TRUE(cache);

  ASSERT_EQ(cache->GetAllShortcuts().size(), 2u);

  EXPECT_TRUE(cache->HasShortcut(initial_chrome_shortcuts[0]->shortcut_id));
  fake_chrome_app_publisher.ShortcutRemoved(
      initial_chrome_shortcuts[0]->shortcut_id);
  EXPECT_EQ(cache->GetAllShortcuts().size(), 1u);
  EXPECT_FALSE(cache->HasShortcut(initial_chrome_shortcuts[0]->shortcut_id));

  EXPECT_TRUE(cache->HasShortcut(initial_chrome_shortcuts[1]->shortcut_id));
  fake_chrome_app_publisher.ShortcutRemoved(
      initial_chrome_shortcuts[1]->shortcut_id);
  EXPECT_EQ(cache->GetAllShortcuts().size(), 0u);
  EXPECT_FALSE(cache->HasShortcut(initial_chrome_shortcuts[1]->shortcut_id));
}

TEST_F(ShortcutPublisherTest, RemoveShortcut_CallsCorrectPublisher) {
  // Setup shortcuts in different publishers to verify the remove gets to the
  // correct publisher.
  ShortcutPtr shortcut_1 = std::make_unique<Shortcut>("app_id_1", "local_id_1");

  ShortcutPtr shortcut_2 = std::make_unique<Shortcut>("app_id_1", "local_id_2");

  Shortcuts initial_chrome_shortcuts;
  initial_chrome_shortcuts.push_back(std::move(shortcut_1));
  initial_chrome_shortcuts.push_back(std::move(shortcut_2));

  FakeShortcutPublisher fake_chrome_app_publisher(proxy(), AppType::kChromeApp,
                                                  initial_chrome_shortcuts);

  ShortcutPtr shortcut_3 = std::make_unique<Shortcut>("app_id_2", "local_id_3");

  Shortcuts initial_web_app_shortcuts;
  initial_web_app_shortcuts.push_back(std::move(shortcut_3));
  FakeShortcutPublisher fake_web_app_publisher(proxy(), AppType::kWeb,
                                               initial_web_app_shortcuts);

  // Add parent apps with corresponding app type so that correct publisher can
  // be found to remove the shortcut.
  PublishApp(apps::AppType::kChromeApp, "app_id_1");
  PublishApp(apps::AppType::kWeb, "app_id_2");

  ShortcutRegistryCache* cache = proxy()->ShortcutRegistryCache();
  ASSERT_TRUE(cache);
  ASSERT_EQ(cache->GetAllShortcuts().size(), 3u);

  // Verify that shortcut remove command goes to the correct shortcut publisher
  // based on the parent app app type.
  UninstallSource uninstall_source = UninstallSource::kUnknown;
  ASSERT_TRUE(cache->HasShortcut(initial_chrome_shortcuts[0]->shortcut_id));
  proxy()->RemoveShortcut(initial_chrome_shortcuts[0]->shortcut_id,
                          uninstall_source, nullptr);
  EXPECT_FALSE(cache->HasShortcut(initial_chrome_shortcuts[0]->shortcut_id));
  EXPECT_EQ(cache->GetAllShortcuts().size(), 2u);

  ASSERT_TRUE(cache->HasShortcut(initial_chrome_shortcuts[1]->shortcut_id));
  proxy()->RemoveShortcut(initial_chrome_shortcuts[1]->shortcut_id,
                          uninstall_source, nullptr);
  EXPECT_FALSE(cache->HasShortcut(initial_chrome_shortcuts[1]->shortcut_id));
  EXPECT_EQ(cache->GetAllShortcuts().size(), 1u);

  ASSERT_TRUE(cache->HasShortcut(initial_web_app_shortcuts[0]->shortcut_id));
  proxy()->RemoveShortcut(initial_web_app_shortcuts[0]->shortcut_id,
                          uninstall_source, nullptr);
  EXPECT_FALSE(cache->HasShortcut(initial_web_app_shortcuts[0]->shortcut_id));
  EXPECT_EQ(cache->GetAllShortcuts().size(), 0u);
}

}  // namespace apps
