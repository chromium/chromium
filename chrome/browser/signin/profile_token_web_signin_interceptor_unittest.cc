// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/profile_token_web_signin_interceptor.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Invoke;
using testing::WithArgs;

namespace {

class MockDelegate : public ProfileTokenWebSigninInterceptor::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(void,
              ShowCreateNewProfileBubble,
              (const ProfileAttributesEntry*, base::OnceCallback<void(bool)>),
              (override));
};

}  // namespace

class ProfileTokenWebSigninInterceptorTest : public BrowserWithTestWindowTest {
 public:
  ProfileTokenWebSigninInterceptorTest() = default;

  ~ProfileTokenWebSigninInterceptorTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    auto delegate = std::make_unique<MockDelegate>();
    delegate_ = delegate.get();
    interceptor_ = std::make_unique<ProfileTokenWebSigninInterceptor>(
        profile(), std::move(delegate));
    interceptor_->SetDisableBrowserCreationAfterInterceptionForTesting(true);

    // Create the first tab so that web_contents() exists.
    AddTab(browser(), GURL("http://foo/1"));
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  std::unique_ptr<ProfileTokenWebSigninInterceptor> interceptor_;
  raw_ptr<MockDelegate> delegate_ = nullptr;  // Owned by `interceptor_`
};

TEST_F(ProfileTokenWebSigninInterceptorTest, NoInterceptionWithInvalidToken) {
  EXPECT_CALL(*delegate_, ShowCreateNewProfileBubble(_, _)).Times(0);
  interceptor_->MaybeInterceptSigninProfile(web_contents(), "id",
                                            /*enrollment_token=*/std::string());
}

TEST_F(ProfileTokenWebSigninInterceptorTest, NoInterceptionWithNoWebContents) {
  EXPECT_CALL(*delegate_, ShowCreateNewProfileBubble(_, _)).Times(0);
  interceptor_->MaybeInterceptSigninProfile(nullptr, "id", "token");
}

TEST_F(ProfileTokenWebSigninInterceptorTest, NoInterceptionWithSameProfile) {
  EXPECT_CALL(*delegate_, ShowCreateNewProfileBubble(_, _)).Times(0);

  auto* entry = TestingBrowserProcess::GetGlobal()
                    ->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(profile()->GetPath());
  entry->SetProfileManagementId("id");
  entry->SetProfileManagementEnrollmentToken("token");

  interceptor_->MaybeInterceptSigninProfile(web_contents(), "id", "token");
}

TEST_F(ProfileTokenWebSigninInterceptorTest,
       InterceptionCreatesNoProfileIfDeclined) {
  const int num_profiles_before = TestingBrowserProcess::GetGlobal()
                                      ->profile_manager()
                                      ->GetNumberOfProfiles();
  EXPECT_CALL(*delegate_, ShowCreateNewProfileBubble(nullptr, _))
      .Times(1)
      .WillOnce(Invoke([](const ProfileAttributesEntry* switch_to_profile,
                          base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(false);
      }));
  interceptor_->MaybeInterceptSigninProfile(web_contents(), "id", "token");

  base::RunLoop().RunUntilIdle();

  const int num_profiles_after = TestingBrowserProcess::GetGlobal()
                                     ->profile_manager()
                                     ->GetNumberOfProfiles();
  EXPECT_EQ(num_profiles_before, num_profiles_after);
}

TEST_F(ProfileTokenWebSigninInterceptorTest,
       InterceptionCreatesNewProfileIfAccepted) {
  const int num_profiles_before = TestingBrowserProcess::GetGlobal()
                                      ->profile_manager()
                                      ->GetNumberOfProfiles();
  EXPECT_CALL(*delegate_, ShowCreateNewProfileBubble(nullptr, _))
      .Times(1)
      .WillOnce(Invoke([](const ProfileAttributesEntry* switch_to_profile,
                          base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      }));
  interceptor_->MaybeInterceptSigninProfile(web_contents(), "id", "token");

  base::RunLoop().RunUntilIdle();

  const int num_profiles_after = TestingBrowserProcess::GetGlobal()
                                     ->profile_manager()
                                     ->GetNumberOfProfiles();
  EXPECT_EQ(num_profiles_before + 1, num_profiles_after);
}

TEST_F(ProfileTokenWebSigninInterceptorTest,
       InterceptionCreatesEphemeralProfileIfAcceptedWithNoId) {
  const int num_profiles_before = TestingBrowserProcess::GetGlobal()
                                      ->profile_manager()
                                      ->GetNumberOfProfiles();
  EXPECT_CALL(*delegate_, ShowCreateNewProfileBubble(nullptr, _))
      .Times(1)
      .WillOnce(Invoke([](const ProfileAttributesEntry* switch_to_profile,
                          base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      }));
  interceptor_->MaybeInterceptSigninProfile(web_contents(), std::string(),
                                            "token");

  base::RunLoop().RunUntilIdle();

  const int num_profiles_after = TestingBrowserProcess::GetGlobal()
                                     ->profile_manager()
                                     ->GetNumberOfProfiles();
  EXPECT_EQ(num_profiles_before + 1, num_profiles_after);
}

TEST_F(ProfileTokenWebSigninInterceptorTest,
       InterceptionSwitchesToExistingProfileIfAccepted) {
  auto* profile_manager = g_browser_process->profile_manager();
  Profile& new_profile = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());

  auto* entry = profile_manager->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(new_profile.GetPath());
  entry->SetProfileManagementId("id");
  entry->SetProfileManagementEnrollmentToken("token");

  EXPECT_CALL(*delegate_, ShowCreateNewProfileBubble(entry, _))
      .Times(1)
      .WillOnce(Invoke([](const ProfileAttributesEntry* switch_to_profile,
                          base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      }));

  const int num_profiles_before = TestingBrowserProcess::GetGlobal()
                                      ->profile_manager()
                                      ->GetNumberOfProfiles();

  interceptor_->MaybeInterceptSigninProfile(web_contents(), "id", "token");

  base::RunLoop().RunUntilIdle();

  const int num_profiles_after = TestingBrowserProcess::GetGlobal()
                                     ->profile_manager()
                                     ->GetNumberOfProfiles();
  EXPECT_EQ(num_profiles_before, num_profiles_after);
}
