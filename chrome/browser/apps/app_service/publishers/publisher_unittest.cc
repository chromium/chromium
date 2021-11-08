// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/publishers/arc_apps.h"
#include "chrome/browser/apps/app_service/publishers/arc_apps_factory.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
scoped_refptr<extensions::Extension> MakeApp(const std::string& name,
                                             const std::string& version,
                                             const std::string& url,
                                             const std::string& id) {
  std::string err;
  base::DictionaryValue value;
  value.SetString("name", name);
  value.SetString("version", version);
  value.SetString("app.launch.web_url", url);
  scoped_refptr<extensions::Extension> app = extensions::Extension::Create(
      base::FilePath(), extensions::mojom::ManifestLocation::kInternal, value,
      extensions::Extension::WAS_INSTALLED_BY_DEFAULT, id, &err);
  EXPECT_EQ(err, "");
  return app;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

namespace apps {

class PublisherTest : public extensions::ExtensionServiceTestBase {
 public:
  PublisherTest() = default;

  PublisherTest(const PublisherTest&) = delete;
  PublisherTest& operator=(const PublisherTest&) = delete;

  ~PublisherTest() override = default;

  // ExtensionServiceTestBase:
  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    InitializeExtensionService(ExtensionServiceInitParams());
    service_->Init();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)

  void RemoveArcApp(const std::string& app_id) {
    ArcApps* arc_apps = ArcAppsFactory::GetForProfile(profile());
    ASSERT_TRUE(arc_apps);
    arc_apps->OnAppRemoved(app_id);
  }

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  void VerifyApp(const std::string& app_id,
                 const std::string& name,
                 apps::Readiness readiness) {
    AppRegistryCache& cache =
        AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();

    ASSERT_NE(cache.states_.end(), cache.states_.find(app_id));
    ASSERT_TRUE(cache.states_[app_id]->name.has_value());
    EXPECT_EQ(name, cache.states_[app_id]->name.value());
    EXPECT_EQ(readiness, cache.states_[app_id]->readiness);
    ASSERT_TRUE(cache.states_[app_id]->icon_key.has_value());
  }

  void VerifyAppIsRemoved(const std::string& app_id) {
    AppRegistryCache& cache =
        AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();
    ASSERT_NE(cache.states_.end(), cache.states_.find(app_id));
    EXPECT_EQ(apps::Readiness::kUninstalledByUser,
              cache.states_[app_id]->readiness);
  }
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PublisherTest, ArcAppsOnApps) {
  ArcAppTest arc_test;
  arc_test.SetUp(profile());

  // Install fake apps.
  std::vector<arc::mojom::AppInfo> fake_apps = arc_test.fake_apps();
  arc_test.app_instance()->SendRefreshAppList(fake_apps);

  // Verify ARC apps are added to AppRegistryCache.
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile());
  ASSERT_TRUE(prefs);
  for (const auto& app_id : prefs->GetAppIds()) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    if (app_info) {
      VerifyApp(app_id, app_info->name, Readiness::kReady);

      // Simulate the app is removed.
      RemoveArcApp(app_id);
      VerifyAppIsRemoved(app_id);
    }
  }

  // Verify the initialization process again with a new ArcApps object.
  std::unique_ptr<ArcApps> arc_apps = std::make_unique<ArcApps>(
      AppServiceProxyFactory::GetForProfile(profile()));
  ASSERT_TRUE(arc_apps.get());
  arc_apps->Initialize();

  for (const auto& app_id : prefs->GetAppIds()) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    if (app_info) {
      VerifyApp(app_id, app_info->name, Readiness::kReady);
    }
  }

  arc_apps->Shutdown();
}

TEST_F(PublisherTest, BuiltinAppsOnApps) {
  // Verify Builtin apps are added to AppRegistryCache.
  for (const auto& internal_app : app_list::GetInternalAppList(profile())) {
    if ((internal_app.app_id == nullptr) ||
        (internal_app.name_string_resource_id == 0) ||
        (internal_app.icon_resource_id <= 0)) {
      continue;
    }
    VerifyApp(internal_app.app_id,
              l10n_util::GetStringUTF8(internal_app.name_string_resource_id),
              Readiness::kReady);
  }
}

class StandaloneBrowserPublisherTest : public PublisherTest {
 public:
  StandaloneBrowserPublisherTest() {
    crosapi::browser_util::SetLacrosEnabledForTest(true);
  }

  StandaloneBrowserPublisherTest(const StandaloneBrowserPublisherTest&) =
      delete;
  StandaloneBrowserPublisherTest& operator=(
      const StandaloneBrowserPublisherTest&) = delete;
  ~StandaloneBrowserPublisherTest() override = default;

  // PublisherTest:
  void SetUp() override {
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    auto* fake_user_manager = user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    // Login a user. The "email" must match the TestingProfile's
    // GetProfileUserName() so that profile() will be the primary profile.
    const AccountId account_id = AccountId::FromUserEmail("testing_profile");
    fake_user_manager->AddUser(account_id);
    fake_user_manager->LoginUser(account_id);

    PublisherTest::SetUp();
  }

 private:
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

TEST_F(StandaloneBrowserPublisherTest, StandaloneBrowserAppsOnApps) {
  VerifyApp(extension_misc::kLacrosAppId, "Lacros", Readiness::kReady);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(PublisherTest, ExtensionAppsOnApps) {
  // Install a "web store" app.
  scoped_refptr<extensions::Extension> store =
      MakeApp("webstore", "0.0", "http://google.com",
              std::string(extensions::kWebStoreAppId));
  service_->AddExtension(store.get());

  // Re-init AppService to verify the init process.
  AppServiceTest app_service_test;
  app_service_test.SetUp(profile());
  VerifyApp(store->id(), store->name(), Readiness::kReady);

  // Uninstall the extension.
  service_->UninstallExtension(
      store->id(), extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  VerifyApp(store->id(), store->name(), Readiness::kUninstalledByUser);

  // Reinstall the extension.
  service_->AddExtension(store.get());
  VerifyApp(store->id(), store->name(), Readiness::kReady);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace apps
