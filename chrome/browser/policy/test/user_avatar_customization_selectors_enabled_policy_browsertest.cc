// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_registry.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_test_util.h"
#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"

namespace policy {

// Browser tests for UserAvatarCustomizationSelectorsEnabled policy. These tests
// set the policy value and then attempt to set the user image (avatar) using
// various sources such as a local image (simulating camera), local image file,
// and profile image.
class UserAvatarCustomizationSelectorsEnabledPolicyTest : public PolicyTest {
 public:
  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();

    // Fetch User, which can be used to check the currently set user image
    user_ = user_manager::UserManager::Get()->GetActiveUser();
    // Fetch UserImageManager, which can be used to save a new user image
    user_image_manager_ =
        ash::UserImageManagerRegistry::Get()->GetManager(user_->GetAccountId());

    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_));
  }

 protected:
  // Set boolean value of UserAvatarCustomizationSelectorsEnabled policy
  void SetPolicy(bool value) {
    PolicyMap policies;
    policies.Set(key::kUserAvatarCustomizationSelectorsEnabled,
                 POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(value), nullptr);
    provider_.UpdateChromePolicy(policies);
  }

  raw_ptr<ash::UserImageManagerImpl, DanglingUntriaged> user_image_manager_;
  raw_ptr<const user_manager::User, DanglingUntriaged> user_;
  base::FilePath test_data_dir_;
};

class UserImageChangedWaiter : public user_manager::UserManager::Observer {
 public:
  UserImageChangedWaiter() {
    run_loop_ = std::make_unique<base::RunLoop>();
    user_manager::UserManager::Get()->AddObserver(this);
  }
  ~UserImageChangedWaiter() override {
    user_manager::UserManager::Get()->RemoveObserver(this);
  }

  // UserManager::Observer override
  // Observes user image changes, which allows tests to wait for the image to
  // change using Wait()
  void OnUserImageChanged(const user_manager::User& _) override {
    run_loop_->Quit();
  }

  void Wait() { run_loop_->Run(); }

  void Reset() { run_loop_ = std::make_unique<base::RunLoop>(); }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
};

// Verifies that the `kUserAvatarCustomizationSelectorsEnabled` pref disables:
//   1. Save custom local image
//   2. Save custom local image file
//   3. Save profile image
IN_PROC_BROWSER_TEST_F(UserAvatarCustomizationSelectorsEnabledPolicyTest,
                       Disabled) {
  SetPolicy(false);

  // Verify user starts with default image
  EXPECT_TRUE(ash::default_user_image::IsValidIndex(user_->image_index()));

  // Attempt to save custom local image
  const gfx::ImageSkia& image = ash::default_user_image::GetStubDefaultImage();
  user_image_manager_->SaveUserImage(user_manager::UserImage::CreateAndEncode(
      image, user_manager::UserImage::FORMAT_JPEG));
  EXPECT_TRUE(ash::default_user_image::IsValidIndex(user_->image_index()));

  // Attempt to save custom local image from file
  const base::FilePath custom_image_path =
      test_data_dir_.Append(ash::test::kUserAvatarImage1RelativePath);
  const gfx::ImageSkia custom_image =
      ash::test::ImageLoader(custom_image_path).Load();
  ASSERT_FALSE(custom_image.isNull());
  user_image_manager_->SaveUserImageFromFile(custom_image_path);
  EXPECT_TRUE(ash::default_user_image::IsValidIndex(user_->image_index()));

  // Attempt to save image from profile
  user_image_manager_->SaveUserImageFromProfileImage();
  EXPECT_TRUE(ash::default_user_image::IsValidIndex(user_->image_index()));

  // Save default image. This should not be affected by policy
  int index = ash::default_user_image::GetRandomDefaultImageIndex();
  UserImageChangedWaiter waiter;
  user_image_manager_->SaveUserDefaultImageIndex(index);
  waiter.Wait();
  EXPECT_EQ(index, user_->image_index());
}

IN_PROC_BROWSER_TEST_F(UserAvatarCustomizationSelectorsEnabledPolicyTest,
                       Enabled) {
  SetPolicy(true);

  // Save custom local image
  const gfx::ImageSkia& image = ash::default_user_image::GetStubDefaultImage();
  UserImageChangedWaiter waiter;
  user_image_manager_->SaveUserImage(user_manager::UserImage::CreateAndEncode(
      image, user_manager::UserImage::FORMAT_JPEG));
  waiter.Wait();
  EXPECT_EQ(user_manager::UserImage::Type::kExternal, user_->image_index());
  EXPECT_TRUE(ash::test::AreImagesEqual(image, user_->GetImage()));

  // Save custom local image from file
  const base::FilePath custom_image_path =
      test_data_dir_.Append(ash::test::kUserAvatarImage1RelativePath);
  const gfx::ImageSkia custom_image =
      ash::test::ImageLoader(custom_image_path).Load();
  ASSERT_FALSE(custom_image.isNull());
  waiter.Reset();
  user_image_manager_->SaveUserImageFromFile(custom_image_path);
  waiter.Wait();
  EXPECT_EQ(user_manager::UserImage::Type::kExternal, user_->image_index());
  EXPECT_TRUE(ash::test::AreImagesEqual(custom_image, user_->GetImage()));

  // Save profile image
  waiter.Reset();
  user_image_manager_->SaveUserImageFromProfileImage();
  waiter.Wait();
  EXPECT_EQ(user_manager::UserImage::Type::kProfile, user_->image_index());

  // Save default image. This should not be affected by policy
  int index = ash::default_user_image::GetRandomDefaultImageIndex();
  waiter.Reset();
  user_image_manager_->SaveUserDefaultImageIndex(index);
  waiter.Wait();
  EXPECT_EQ(index, user_->image_index());
}
}  // namespace policy
