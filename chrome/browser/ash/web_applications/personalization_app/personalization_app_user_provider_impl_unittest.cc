// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_user_provider_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/login/users/avatar/fake_user_image_file_selector.h"
#include "chrome/browser/ash/login/users/avatar/mock_user_image_manager.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/users/scoped_test_user_manager.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_utils.h"
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
#include "url/gurl.h"

namespace {

using ash::personalization_app::GetAccountId;

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

gfx::ImageSkia CreateImage(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  return image;
}

class TestUserImageObserver
    : public ash::personalization_app::mojom::UserImageObserver {
 public:
  void OnUserImageChanged(const GURL& image) override {
    current_user_image_ = image;
  }

  void OnUserProfileImageUpdated(const GURL& profile_image) override {
    current_profile_image_ = profile_image;
  }

  void OnCameraPresenceCheckDone(bool is_camera_present) override {}

  mojo::PendingRemote<ash::personalization_app::mojom::UserImageObserver>
  pending_remote() {
    DCHECK(!user_image_observer_receiver_.is_bound());
    return user_image_observer_receiver_.BindNewPipeAndPassRemote();
  }

  const GURL& current_user_image() {
    if (user_image_observer_receiver_.is_bound())
      user_image_observer_receiver_.FlushForTesting();
    return current_user_image_;
  }

  const GURL& current_profile_image() {
    if (user_image_observer_receiver_.is_bound())
      user_image_observer_receiver_.FlushForTesting();
    return current_profile_image_;
  }

 private:
  mojo::Receiver<ash::personalization_app::mojom::UserImageObserver>
      user_image_observer_receiver_{this};

  GURL current_user_image_;
  GURL current_profile_image_;
};

}  // namespace

class PersonalizationAppUserProviderImplTest : public testing::Test {
 public:
  PersonalizationAppUserProviderImplTest()
      : scoped_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kPersonalizationHub);
  }
  PersonalizationAppUserProviderImplTest(
      const PersonalizationAppUserProviderImplTest&) = delete;
  PersonalizationAppUserProviderImplTest& operator=(
      const PersonalizationAppUserProviderImplTest&) = delete;
  ~PersonalizationAppUserProviderImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kFakeTestEmail);
    AddAndLoginUser(AccountId::FromUserEmailGaiaId(kFakeTestEmail, kTestGaiaId),
                    kFakeTestName);

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_.set_web_contents(web_contents_.get());
    ash::UserImageManagerImpl::SkipProfileImageDownloadForTesting();
    user_provider_ =
        std::make_unique<PersonalizationAppUserProviderImpl>(&web_ui_);

    user_provider_->BindInterface(
        user_provider_remote_.BindNewPipeAndPassReceiver());
  }

  TestingProfile* profile() { return profile_; }

  content::TestWebUI* web_ui() { return &web_ui_; }

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

  ash::UserImageManagerImpl* user_image_manager() {
    return static_cast<ash::UserImageManagerImpl*>(
        ash::ChromeUserManager::Get()->GetUserImageManager(
            GetAccountId(profile_)));
  }

  ash::FakeChromeUserManager* GetFakeUserManager() {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void SetUserImageObserver() {
    user_provider_remote_->SetUserImageObserver(
        test_user_image_observer_.pending_remote());
  }

  const GURL& current_user_image() {
    if (user_provider_remote_.is_bound())
      user_provider_remote_.FlushForTesting();
    return test_user_image_observer_.current_user_image();
  }

  const GURL& current_profile_image() {
    if (user_provider_remote_.is_bound())
      user_provider_remote_.FlushForTesting();
    return test_user_image_observer_.current_profile_image();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  user_manager::ScopedUserManager scoped_user_manager_;
  TestingProfileManager profile_manager_;
  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;
  TestingProfile* profile_;
  TestUserImageObserver test_user_image_observer_;
  mojo::Remote<ash::personalization_app::mojom::UserProvider>
      user_provider_remote_;
  std::unique_ptr<PersonalizationAppUserProviderImpl> user_provider_;
};

TEST_F(PersonalizationAppUserProviderImplTest, GetsUserInfo) {
  user_provider_remote()->get()->GetUserInfo(base::BindLambdaForTesting(
      [](ash::personalization_app::UserDisplayInfo user_display_info) {
        EXPECT_EQ(kFakeTestEmail, user_display_info.email);
        EXPECT_EQ(kFakeTestName, user_display_info.name);
      }));
  user_provider_remote()->FlushForTesting();
}

TEST_F(PersonalizationAppUserProviderImplTest, ObservesUserAvatarImage) {
  // Observer has not received any images yet because it is not bound.
  EXPECT_EQ(GURL(), current_user_image());

  SetUserImageObserver();

  // Observer received current user's avatar image as data url.
  EXPECT_EQ(webui::GetBitmapDataUrl(*user_image().bitmap()),
            current_user_image());

  // Select a default image.
  int image_index = ash::default_user_image::GetRandomDefaultImageIndex();
  user_image_manager()->SaveUserDefaultImageIndex(image_index);

  // Observer received the updated image url. Because it is a default image,
  // receives the chrome://theme url.
  EXPECT_EQ(base::StringPrintf("chrome://theme/IDR_LOGIN_DEFAULT_USER_%d",
                               image_index),
            current_user_image());
}

TEST_F(PersonalizationAppUserProviderImplTest, SelectDefaultImage) {
  SetUserImageObserver();

  // Select a default image.
  int image_index = ash::default_user_image::GetRandomDefaultImageIndex();
  user_provider_remote()->get()->SelectDefaultImage(image_index);
  user_provider_remote()->FlushForTesting();

  // Observer received the updated image url. Because it is a default image,
  // receives the chrome://theme url.
  EXPECT_EQ(base::StringPrintf("chrome://theme/IDR_LOGIN_DEFAULT_USER_%d",
                               image_index),
            current_user_image());
}

TEST_F(PersonalizationAppUserProviderImplTest, ObservesUserProfileImage) {
  // Observer has not received any images yet because it is not bound.
  EXPECT_EQ(GURL(), current_profile_image());

  SetUserImageObserver();

  // Select a profile image.
  const gfx::ImageSkia& profile_image = CreateImage(50, 50);
  user_image_manager()->SetDownloadedProfileImageForTesting(profile_image);
  user_image_manager()->SaveUserImageFromProfileImage();

  // Observer received the updated profile image url.
  EXPECT_EQ(GURL(webui::GetBitmapDataUrl(*profile_image.bitmap())),
            current_profile_image());
}

TEST_F(PersonalizationAppUserProviderImplTest, SelectProfileImage) {
  SetUserImageObserver();

  const gfx::ImageSkia& profile_image = CreateImage(50, 50);
  user_image_manager()->SetDownloadedProfileImageForTesting(profile_image);
  user_provider_remote()->get()->SelectProfileImage();
  user_provider_remote()->FlushForTesting();

  EXPECT_EQ(GURL(webui::GetBitmapDataUrl(*profile_image.bitmap())),
            current_profile_image());
}

class PersonalizationAppUserProviderImplWithMockTest
    : public PersonalizationAppUserProviderImplTest {
 protected:
  void SetUp() override {
    GetFakeUserManager()->SetMockUserImageManagerForTesting();
    PersonalizationAppUserProviderImplTest::SetUp();
  }

  ash::MockUserImageManager* mock_user_image_manager() {
    return static_cast<ash::MockUserImageManager*>(
        ash::ChromeUserManager::Get()->GetUserImageManager(
            GetAccountId(profile())));
  }
};

TEST_F(PersonalizationAppUserProviderImplWithMockTest, SelectImageFromDisk) {
  const base::FilePath base_file_path("/this/is/a/test/directory/Base Name");
  const base::FilePath dir_path = base_file_path.AppendASCII("dir1");
  const base::FilePath file_path = dir_path.AppendASCII("file1.txt");

  EXPECT_CALL(*mock_user_image_manager(), SaveUserImageFromFile(file_path));

  auto fake_file_selector =
      std::make_unique<ash::FakeUserImageFileSelector>(web_ui());
  fake_file_selector->SetFilePath(file_path);
  user_provider()->SetUserImageFileSelectorForTesting(
      std::move(fake_file_selector));
  user_provider_remote()->get()->SelectImageFromDisk();
  user_provider_remote()->FlushForTesting();
}
