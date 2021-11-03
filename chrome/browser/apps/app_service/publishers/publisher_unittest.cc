// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/publishers/arc_apps.h"
#include "chrome/browser/apps/app_service/publishers/arc_apps_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "components/arc/test/fake_app_instance.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace apps
