// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/arc_ghost_window_view.h"

#include "base/callback_forward.h"
#include "base/test/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/test/views_test_base.h"

namespace ash {
namespace full_restore {

namespace {

constexpr char kTestProfileName[] = "user@gmail.com";
constexpr char16_t kTestProfileName16[] = u"user@gmail.com";

apps::mojom::AppPtr MakeApp(const char* app_id,
                            apps::mojom::AppType app_type,
                            apps::mojom::InstallReason install_reason) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_id = app_id;
  app->app_type = app_type;
  app->install_reason = install_reason;
  return app;
}

}  // namespace

class ArcGhostWindowViewTest : public testing::Test {
 public:
  ArcGhostWindowViewTest() = default;
  ArcGhostWindowViewTest(const ArcGhostWindowViewTest&) = delete;
  ArcGhostWindowViewTest& operator=(const ArcGhostWindowViewTest&) = delete;
  ~ArcGhostWindowViewTest() override = default;

  void SetUp() override {
    user_manager_ = new ash::FakeChromeUserManager;
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::unique_ptr<user_manager::UserManager>(user_manager_));

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    const user_manager::User* user =
        user_manager_->AddUser(AccountId::FromUserEmail(kTestProfileName));
    user_manager_->LoginUser(user->GetAccountId());
    user_manager_->SwitchActiveUser(user->GetAccountId());

    // Note that user profiles are created after user login in reality.
    profile_ = profile_manager_->CreateTestingProfile(
        kTestProfileName, /*prefs=*/{}, kTestProfileName16,
        /*avatar_id=*/0,
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories());
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                                 profile_);
  }

  void InstallApp(const std::string& app_id) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
    std::vector<apps::mojom::AppPtr> deltas;
    apps::AppRegistryCache& cache = proxy->AppRegistryCache();
    deltas.push_back(MakeApp(app_id.c_str(), apps::mojom::AppType::kArc,
                             apps::mojom::InstallReason::kUser));
    cache.OnApps(std::move(deltas), apps::mojom::AppType::kUnknown,
                 false /* should_notify_initialized */);
  }

  void CreateView(int throbber_diameter, uint32_t theme_color) {
    view_ =
        std::make_unique<ArcGhostWindowView>(throbber_diameter, theme_color);
  }

  ArcGhostWindowView* view() { return view_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  views::ScopedViewsTestHelper test_helper_{
      std::make_unique<ChromeTestViewsDelegate<>>()};

 private:
  std::unique_ptr<ArcGhostWindowView> view_;

  ash::FakeChromeUserManager* user_manager_;  // Not own.
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;

  TestingProfile* profile_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
};

TEST_F(ArcGhostWindowViewTest, IconLoadTest) {
  const int kDiameter = 24;
  const uint32_t kThemeColor = SK_ColorWHITE;
  const std::string kAppId = "test_app";
  InstallApp(kAppId);

  int count = 0;
  CreateView(kDiameter, kThemeColor);
  EXPECT_EQ(count, 0);

  view()->icon_loaded_cb_for_testing_ = base::BindLambdaForTesting(
      [&count](apps::IconValuePtr icon_value) { count++; });
  view()->LoadIcon(kAppId);
  EXPECT_EQ(count, 1);
}

}  // namespace full_restore
}  // namespace ash
