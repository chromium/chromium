// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/arc_ghost_window_view.h"
#include "base/memory/raw_ptr.h"

#include "ash/components/arc/arc_features.h"
#include "base/functional/callback_forward.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/arc/window_predictor/window_predictor_utils.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/test/views_test_base.h"

namespace ash::full_restore {

namespace {

constexpr char kTestProfileName[] = "user@gmail.com";
constexpr char16_t kTestProfileName16[] = u"user@gmail.com";

apps::AppPtr MakeApp(const char* app_id,
                     apps::AppType app_type,
                     apps::InstallReason install_reason) {
  auto app = std::make_unique<apps::App>(app_type, app_id);
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
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    const user_manager::User* user =
        fake_user_manager_->AddUser(AccountId::FromUserEmail(kTestProfileName));
    fake_user_manager_->LoginUser(user->GetAccountId());
    fake_user_manager_->SwitchActiveUser(user->GetAccountId());

    // Note that user profiles are created after user login in reality.
    profile_ = profile_manager_->CreateTestingProfile(
        kTestProfileName, /*prefs=*/{}, kTestProfileName16,
        /*avatar_id=*/0,
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories());

    feature_list_.InitAndEnableFeature(arc::kGhostWindowNewStyle);
  }

  void InstallApp(const std::string& app_id) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
    std::vector<apps::AppPtr> deltas;
    deltas.push_back(MakeApp(app_id.c_str(), apps::AppType::kArc,
                             apps::InstallReason::kUser));
    proxy->OnApps(std::move(deltas), apps::AppType::kUnknown,
                  false /* should_notify_initialized */);
  }

  void CreateView(arc::GhostWindowType type, uint32_t theme_color) {
    view_ = std::make_unique<ArcGhostWindowView>(nullptr, "");
    view_->SetThemeColor(theme_color);
    view_->SetGhostWindowViewType(type);
  }

  void CreateEmptyView() {
    view_ = std::make_unique<ArcGhostWindowView>(nullptr, "");
  }

  ArcGhostWindowView* view() { return view_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  views::ScopedViewsTestHelper test_helper_{
      std::make_unique<ChromeTestViewsDelegate<>>()};

 private:
  std::unique_ptr<ArcGhostWindowView> view_;

  base::test::ScopedFeatureList feature_list_;

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
};

TEST_F(ArcGhostWindowViewTest, IconLoadTest) {
  const uint32_t kThemeColor = SK_ColorWHITE;
  const std::string kAppId = "test_app";
  InstallApp(kAppId);

  int count = 0;
  CreateView(arc::GhostWindowType::kFullRestore, kThemeColor);
  EXPECT_EQ(count, 0);

  view()->icon_loaded_cb_for_testing_ = base::BindLambdaForTesting(
      [&count](apps::IconValuePtr icon_value) { count++; });
  view()->LoadIcon(kAppId);
  EXPECT_EQ(count, 1);
}

TEST_F(ArcGhostWindowViewTest, EmptyViewIconLoadTest) {
  const std::string kAppId = "test_app";
  InstallApp(kAppId);

  int count = 0;
  CreateEmptyView();
  EXPECT_EQ(count, 0);

  view()->icon_loaded_cb_for_testing_ = base::BindLambdaForTesting(
      [&count](apps::IconValuePtr icon_value) { count++; });
  view()->LoadIcon(kAppId);
  EXPECT_EQ(count, 1);
}

TEST_F(ArcGhostWindowViewTest, FixupMessageTest) {
  const uint32_t kThemeColor = SK_ColorWHITE;
  const std::string kAppId = "test_app";
  InstallApp(kAppId);

  CreateView(arc::GhostWindowType::kFixup, kThemeColor);

  auto* message_label = static_cast<views::Label*>(
      view()->GetViewByID(ContentID::ID_MESSAGE_LABEL));
  EXPECT_NE(message_label, nullptr);
  EXPECT_EQ(message_label->GetText(),
            l10n_util::GetStringUTF16(IDS_ARC_GHOST_WINDOW_APP_FIXUP_MESSAGE));
}

}  // namespace ash::full_restore
