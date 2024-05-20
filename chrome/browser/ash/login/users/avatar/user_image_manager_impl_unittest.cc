// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/avatar/user_image_manager_impl.h"

#include <memory>
#include <string_view>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/users/avatar/mock_user_image_loader_delegate.h"
#include "chrome/browser/ash/login/users/avatar/user_image_loader.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_registry.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_test_util.h"
#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr std::string_view kFakeRegularUserEmail = "test@example.com";

}  // namespace

class UserImageManagerImplTest : public testing::Test {
 public:
  UserImageManagerImplTest() = default;
  UserImageManagerImplTest(const UserImageManagerImplTest&) = delete;
  UserImageManagerImplTest& operator=(const UserImageManagerImplTest&) = delete;
  ~UserImageManagerImplTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

    ASSERT_TRUE(profile_manager_.SetUp());

    auto mock_user_image_loader_delegate = std::make_unique<
        testing::StrictMock<test::MockUserImageLoaderDelegate>>();
    mock_user_image_loader_delegate_ = mock_user_image_loader_delegate.get();

    user_image_manager_registry_ = std::make_unique<UserImageManagerRegistry>(
        fake_chrome_user_manager(), std::move(mock_user_image_loader_delegate));
  }

  void TearDown() override {
    mock_user_image_loader_delegate_ = nullptr;
    testing::Test::TearDown();
  }

  user_manager::User* AddUser(const AccountId& account_id) {
    user_manager::User* user = fake_chrome_user_manager()->AddUser(account_id);
    TestingProfile* profile =
        profile_manager_.CreateTestingProfile(account_id.GetUserEmail());
    fake_chrome_user_manager()->OnUserProfileCreated(account_id,
                                                     profile->GetPrefs());
    return user;
  }

  FakeChromeUserManager* fake_chrome_user_manager() {
    return fake_chrome_user_manager_.Get();
  }

  UserImageManagerRegistry* user_image_manager_registry() {
    return user_image_manager_registry_.get();
  }

  UserImageManagerImpl* user_image_manager_impl(const AccountId& account_id) {
    return user_image_manager_registry()->GetManager(account_id);
  }

  testing::StrictMock<test::MockUserImageLoaderDelegate>*
  mock_user_image_loader_delegate() {
    return mock_user_image_loader_delegate_.get();
  }

  bool is_random_image_set(const AccountId& account_id) {
    return user_image_manager_impl(account_id)->is_random_image_set_;
  }

  bool is_downloading_profile_image(const AccountId& account_id) {
    return user_image_manager_impl(account_id)->downloading_profile_image_;
  }

  bool NeedProfileImage(const AccountId& account_id) {
    return user_image_manager_impl(account_id)->NeedProfileImage();
  }

 private:
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal(),
                                         &local_state_};
  user_manager::TypedScopedUserManager<FakeChromeUserManager>
      fake_chrome_user_manager_{std::make_unique<FakeChromeUserManager>()};
  raw_ptr<testing::StrictMock<test::MockUserImageLoaderDelegate>>
      mock_user_image_loader_delegate_;
  std::unique_ptr<UserImageManagerRegistry> user_image_manager_registry_;
};

// TODO(b/339503132) should set to google profile image.
TEST_F(UserImageManagerImplTest, SetsRandomDefaultInitialImageForNewUsers) {
  const AccountId account_id = AccountId::FromUserEmailGaiaId(
      std::string(kFakeRegularUserEmail), std::string(kFakeRegularUserEmail));
  user_manager::User* user = AddUser(account_id);

  GURL requested_url;

  EXPECT_CALL(*mock_user_image_loader_delegate(), FromGURLAnimated)
      .WillOnce(testing::DoAll(
          testing::SaveArg<0>(&requested_url),
          testing::Invoke([](const GURL& default_image_url,
                             user_image_loader::LoadedCallback loaded_cb) {
            base::SequencedTaskRunner::GetCurrentDefault()
                ->PostTaskAndReplyWithResult(
                    FROM_HERE, base::BindLambdaForTesting([]() {
                      return std::make_unique<user_manager::UserImage>();
                    }),
                    std::move(loaded_cb));
          })));

  fake_chrome_user_manager()->SetIsCurrentUserNew(true);
  fake_chrome_user_manager()->UserLoggedIn(
      account_id, /*user_id_hash=*/
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id),
      /*browser_restart=*/false,
      /*is_child=*/false);

  test::UserImageChangeWaiter user_image_change_waiter;
  user_image_change_waiter.Wait();

  EXPECT_TRUE(default_user_image::IsInCurrentImageSet(user->image_index()));
  EXPECT_EQ(requested_url,
            default_user_image::GetDefaultImageUrl(user->image_index()));

  EXPECT_FALSE(is_downloading_profile_image(account_id));
  EXPECT_FALSE(NeedProfileImage(account_id));
  EXPECT_TRUE(
      user_image_manager_impl(account_id)->DownloadedProfileImage().isNull());
  EXPECT_TRUE(is_random_image_set(account_id));
}

TEST_F(UserImageManagerImplTest, RecordsUserImageLoggedInHistogram) {
  constexpr int kDefaultImageIndex = 85;
  ASSERT_TRUE(default_user_image::IsInCurrentImageSet(kDefaultImageIndex));

  base::HistogramTester histogram_tester;

  const AccountId account_id = AccountId::FromUserEmailGaiaId(
      std::string(kFakeRegularUserEmail), std::string(kFakeRegularUserEmail));
  AddUser(account_id);

  EXPECT_CALL(*mock_user_image_loader_delegate(), FromGURLAnimated);

  user_image_manager_impl(account_id)
      ->SaveUserDefaultImageIndex(kDefaultImageIndex);

  fake_chrome_user_manager()->SetIsCurrentUserNew(false);
  fake_chrome_user_manager()->UserLoggedIn(
      account_id, /*user_id_hash=*/
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id),
      /*browser_restart=*/false,
      /*is_child=*/false);

  histogram_tester.ExpectUniqueSample(
      UserImageManagerImpl::kUserImageLoggedInHistogramName,
      UserImageManagerImpl::ImageIndexToHistogramIndex(kDefaultImageIndex), 1);
}

}  // namespace ash
