// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_user_provider_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/users/scoped_test_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image_skia.h"

namespace {

constexpr char kFakeTestEmail[] = "fakeemail@personalization";
constexpr char kFakeTestName[] = "Fake Name";
constexpr char kTestGaiaId[] = "1234567890";

void AddAndLoginUser(const AccountId& account_id,
                     const std::string& display_name) {
  ash::FakeChromeUserManager* user_manager =
      static_cast<ash::FakeChromeUserManager*>(
          user_manager::UserManager::Get());

  user_manager->AddUser(account_id);
  user_manager->SaveUserDisplayName(account_id,
                                    base::UTF8ToUTF16(display_name));
  user_manager->LoginUser(account_id);
  user_manager->SwitchActiveUser(account_id);
}
}  // namespace

class PersonalizationAppUserProviderImplTest : public testing::Test {
 public:
  PersonalizationAppUserProviderImplTest()
      : scoped_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  PersonalizationAppUserProviderImplTest(
      const PersonalizationAppUserProviderImplTest&) = delete;
  PersonalizationAppUserProviderImplTest& operator=(
      const PersonalizationAppUserProviderImplTest&) = delete;
  ~PersonalizationAppUserProviderImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kPersonalizationHub);
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kFakeTestEmail);

    AddAndLoginUser(AccountId::FromUserEmailGaiaId(kFakeTestEmail, kTestGaiaId),
                    kFakeTestName);

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_.set_web_contents(web_contents_.get());

    user_provider_ =
        std::make_unique<PersonalizationAppUserProviderImpl>(&web_ui_);

    user_provider_->BindInterface(
        user_provider_remote_.BindNewPipeAndPassReceiver());
  }

  TestingProfile* profile() { return profile_; }

  mojo::Remote<ash::personalization_app::mojom::UserProvider>*
  user_provider_remote() {
    return &user_provider_remote_;
  }

  PersonalizationAppUserProviderImpl* user_provider() {
    return user_provider_.get();
  }

  gfx::ImageSkia user_image() {
    return user_manager::UserManager::Get()->GetActiveUser()->GetImage();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  user_manager::ScopedUserManager scoped_user_manager_;
  TestingProfileManager profile_manager_;
  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;
  TestingProfile* profile_;
  mojo::Remote<ash::personalization_app::mojom::UserProvider>
      user_provider_remote_;
  std::unique_ptr<PersonalizationAppUserProviderImpl> user_provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PersonalizationAppUserProviderImplTest, GetsUserInfo) {
  user_provider_remote()->get()->GetUserInfo(base::BindLambdaForTesting(
      [this](ash::personalization_app::UserDisplayInfo user_display_info) {
        EXPECT_EQ(kFakeTestEmail, user_display_info.email);
        EXPECT_EQ(kFakeTestName, user_display_info.name);
        EXPECT_EQ(webui::GetBitmapDataUrl(*user_image().bitmap()),
                  user_display_info.avatar);
      }));
  user_provider_remote()->FlushForTesting();
}
