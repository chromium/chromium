// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/apps/app_service/publishers/browser_shortcuts_crosapi_publisher.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/app_service/web_apps_with_shortcuts_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_constants.h"

namespace {

class FakeAppShortcutController : public crosapi::mojom::AppShortcutController {
 public:
  struct LaunchInfo {
    std::string host_app_id;
    std::string local_shortcut_id;
    int64_t display_id;
    LaunchInfo(const std::string& host_app_id,
               const std::string& local_shortcut_id,
               int64_t display_id)
        : host_app_id(host_app_id),
          local_shortcut_id(local_shortcut_id),
          display_id(display_id) {}
  };
  struct GetCompressedIconInfo {
    std::string host_app_id;
    std::string local_shortcut_id;
    int32_t size_in_dip;
    ui::ResourceScaleFactor scale_factor;
    GetCompressedIconInfo(const std::string& host_app_id,
                          const std::string& local_shortcut_id,
                          int32_t size_in_dip,
                          ui::ResourceScaleFactor scale_factor)
        : host_app_id(host_app_id),
          local_shortcut_id(local_shortcut_id),
          size_in_dip(size_in_dip),
          scale_factor(scale_factor) {}
  };
  struct RemoveShortcutInfo {
    std::string host_app_id;
    std::string local_shortcut_id;
    apps::UninstallSource uninstall_source;
    RemoveShortcutInfo(const std::string& host_app_id,
                       const std::string& local_shortcut_id,
                       apps::UninstallSource uninstall_source)
        : host_app_id(host_app_id),
          local_shortcut_id(local_shortcut_id),
          uninstall_source(uninstall_source) {}
  };

  // crosapi::mojom::AppController:
  void LaunchShortcut(const std::string& host_app_id,
                      const std::string& local_shortcut_id,
                      int64_t display_id,
                      LaunchShortcutCallback callback) override {
    launch_info_.emplace_back(host_app_id, local_shortcut_id, display_id);
    std::move(callback).Run();
  }
  void GetCompressedIcon(const std::string& host_app_id,
                         const std::string& local_shortcut_id,
                         int32_t size_in_dip,
                         ui::ResourceScaleFactor scale_factor,
                         apps::LoadIconCallback callback) override {
    load_icon_info_.emplace_back(host_app_id, local_shortcut_id, size_in_dip,
                                 scale_factor);
    std::move(callback).Run(std::make_unique<apps::IconValue>());
  }
  void RemoveShortcut(const std::string& host_app_id,
                      const std::string& local_shortcut_id,
                      apps::UninstallSource uninstall_source,
                      RemoveShortcutCallback callback) override {
    remove_shortcut_info_.emplace_back(host_app_id, local_shortcut_id,
                                       uninstall_source);
    std::move(callback).Run();
  }

  const std::vector<LaunchInfo>& get_launch_info() const {
    return launch_info_;
  }

  const std::vector<GetCompressedIconInfo>& get_load_icon_info() const {
    return load_icon_info_;
  }

  const std::vector<RemoveShortcutInfo>& remove_shortcut_info() const {
    return remove_shortcut_info_;
  }

  mojo::Receiver<crosapi::mojom::AppShortcutController> receiver_{this};

 private:
  std::vector<LaunchInfo> launch_info_;
  std::vector<GetCompressedIconInfo> load_icon_info_;
  std::vector<RemoveShortcutInfo> remove_shortcut_info_;
};

}  // namespace

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

    fake_controller_ = std::make_unique<FakeAppShortcutController>();
    base::test::TestFuture<crosapi::mojom::ControllerRegistrationResult> future;
    app_shortcut_publisher_remote_->RegisterAppShortcutController(
        fake_controller_->receiver_.BindNewPipeAndPassRemoteWithVersion(),
        future.GetCallback());
    EXPECT_EQ(future.Get(),
              crosapi::mojom::ControllerRegistrationResult::kSuccess);
  }
  Profile* profile() { return profile_.get(); }
  FakeAppShortcutController* fake_controller() {
    return fake_controller_.get();
  }

  void PublishApp(AppType type, const std::string& app_id) {
    std::vector<apps::AppPtr> app_deltas;
    app_deltas.push_back(apps::AppPublisher::MakeApp(
        type, app_id, apps::Readiness::kReady, "Some App Name",
        apps::InstallReason::kUser, apps::InstallSource::kSystem));
    apps::AppServiceProxyFactory::GetForProfile(profile())->OnApps(
        std::move(app_deltas), type,
        /* should_notify_initialized */ true);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  mojo::Remote<crosapi::mojom::AppShortcutPublisher>
      app_shortcut_publisher_remote_;
  std::unique_ptr<apps::BrowserShortcutsCrosapiPublisher>
      browser_shortcuts_crosapi_publisher_;
  std::unique_ptr<FakeAppShortcutController> fake_controller_;
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

TEST_F(BrowserShortcutsCrosapiPublisherTest, LaunchShortcut) {
  std::vector<ShortcutPtr> shortcuts;
  ShortcutPtr shortcut_1 = std::make_unique<Shortcut>("app_id_1", "local_id_1");
  shortcut_1->shortcut_source = ShortcutSource::kUser;
  shortcuts.push_back(shortcut_1->Clone());
  base::RunLoop runloop;
  app_shortcut_publisher_remote_->PublishShortcuts(std::move(shortcuts),
                                                   runloop.QuitClosure());
  runloop.Run();
  PublishApp(AppType::kStandaloneBrowser, "app_id_1");

  ASSERT_EQ(fake_controller()->get_launch_info().size(), 0u);
  base::RunLoop launch_shortcut;
  browser_shortcuts_crosapi_publisher_->SetLaunchShortcutCallbackForTesting(
      launch_shortcut.QuitClosure());
  apps::AppServiceProxyFactory::GetForProfile(profile())->LaunchShortcut(
      shortcut_1->shortcut_id, display::kDefaultDisplayId);
  launch_shortcut.Run();
  ASSERT_EQ(fake_controller()->get_launch_info().size(), 1u);
  EXPECT_EQ(fake_controller()->get_launch_info().back().host_app_id,
            shortcut_1->host_app_id);
  EXPECT_EQ(fake_controller()->get_launch_info().back().local_shortcut_id,
            shortcut_1->local_id);
  EXPECT_EQ(fake_controller()->get_launch_info().back().display_id,
            display::kDefaultDisplayId);
}

TEST_F(BrowserShortcutsCrosapiPublisherTest, LoadIcon) {
  std::vector<ShortcutPtr> shortcuts;
  ShortcutPtr shortcut = std::make_unique<Shortcut>("app_id", "local_id");
  shortcut->shortcut_source = ShortcutSource::kUser;
  shortcut->icon_key = IconKey(0, 0);
  shortcut->icon_key->update_version = false;
  shortcuts.push_back(shortcut->Clone());
  base::RunLoop runloop;
  app_shortcut_publisher_remote_->PublishShortcuts(std::move(shortcuts),
                                                   runloop.QuitClosure());
  runloop.Run();
  PublishApp(AppType::kStandaloneBrowser, "app_id");

  ASSERT_EQ(fake_controller()->get_load_icon_info().size(), 0u);
  base::test::TestFuture<IconValuePtr> future;
  auto* shortcut_publisher =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->GetShortcutPublisherForTesting(apps::AppType::kStandaloneBrowser);
  shortcut_publisher->GetCompressedIconData(
      shortcut->shortcut_id.value(), /*size_hint_in_dip*/ 32,
      ui::ResourceScaleFactor::k100Percent, future.GetCallback());
  EXPECT_TRUE(future.Wait());
  ASSERT_EQ(fake_controller()->get_load_icon_info().size(), 1u);

  EXPECT_EQ(fake_controller()->get_load_icon_info()[0].host_app_id,
            shortcut->host_app_id);
  EXPECT_EQ(fake_controller()->get_load_icon_info()[0].local_shortcut_id,
            shortcut->local_id);
  EXPECT_EQ(fake_controller()->get_load_icon_info()[0].size_in_dip, 32);
  EXPECT_EQ(fake_controller()->get_load_icon_info()[0].scale_factor,
            ui::ResourceScaleFactor::k100Percent);
}

TEST_F(BrowserShortcutsCrosapiPublisherTest, RemoveShortcut) {
  std::vector<ShortcutPtr> shortcuts;
  ShortcutPtr shortcut = std::make_unique<Shortcut>("app_id_1", "local_id_1");
  shortcut->shortcut_source = ShortcutSource::kUser;
  shortcuts.push_back(shortcut->Clone());
  base::RunLoop runloop;
  app_shortcut_publisher_remote_->PublishShortcuts(std::move(shortcuts),
                                                   runloop.QuitClosure());
  runloop.Run();
  PublishApp(AppType::kStandaloneBrowser, "app_id_1");

  apps::ShortcutRegistryCache* cache =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->ShortcutRegistryCache();
  ASSERT_TRUE(cache->HasShortcut(shortcut->shortcut_id));

  base::RunLoop remove_shortcut;
  browser_shortcuts_crosapi_publisher_->SetRemoveShortcutCallbackForTesting(
      remove_shortcut.QuitClosure());
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->RemoveShortcutSilently(shortcut->shortcut_id,
                               apps::UninstallSource::kUnknown);
  remove_shortcut.Run();
  ASSERT_EQ(fake_controller()->remove_shortcut_info().size(), 1u);
  EXPECT_EQ(fake_controller()->remove_shortcut_info().back().host_app_id,
            shortcut->host_app_id);
  EXPECT_EQ(fake_controller()->remove_shortcut_info().back().local_shortcut_id,
            shortcut->local_id);
  EXPECT_EQ(fake_controller()->remove_shortcut_info().back().uninstall_source,
            apps::UninstallSource::kUnknown);

  base::RunLoop removed_runloop;
  app_shortcut_publisher_remote_->ShortcutRemoved(
      shortcut->shortcut_id.value(), removed_runloop.QuitClosure());
  removed_runloop.Run();

  EXPECT_FALSE(cache->HasShortcut(shortcut->shortcut_id));
}

}  // namespace apps
