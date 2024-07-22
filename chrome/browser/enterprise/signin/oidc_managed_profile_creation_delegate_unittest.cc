// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/oidc_managed_profile_creation_delegate.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr char kOAuthToken[] = "fake-oauth-token";
constexpr char kIdToken[] = "fake-id-token";

constexpr char kSampleEmail[] = "email@domain.com";
constexpr char kSampleName[] = "People Person";

class OidcManagedProfileCreationDelegateTest
    : public testing::TestWithParam<bool> {
 public:
  OidcManagedProfileCreationDelegateTest()
      : profile_manager_(std::make_unique<TestingProfileManager>(
            TestingBrowserProcess::GetGlobal())),
        is_dasher_based_(GetParam()) {}

  ~OidcManagedProfileCreationDelegateTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test_profile");
  }

 protected:
  bool is_dasher_based() { return is_dasher_based_; }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<Profile> profile_;
  bool creator_callback_called_ = false;

 private:
  bool is_dasher_based_;
};

TEST_P(OidcManagedProfileCreationDelegateTest,
       CreatesProfileWithManagementInfo) {
  auto delegate = std::make_unique<OidcManagedProfileCreationDelegate>(
      kOAuthToken, kIdToken, is_dasher_based(), kSampleName, kSampleEmail);

  auto* entry = TestingBrowserProcess::GetGlobal()
                    ->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(profile_->GetPath());
  delegate->SetManagedAttributesForProfile(entry);
  ASSERT_TRUE(entry);
  ProfileManagementOidcTokens oidc_tokens =
      entry->GetProfileManagementOidcTokens();
  EXPECT_EQ(kOAuthToken, oidc_tokens.auth_token);
  EXPECT_EQ(kIdToken, oidc_tokens.id_token);
  EXPECT_EQ(base::UTF16ToUTF8(entry->GetGAIAName()), kSampleName);
}

TEST_P(OidcManagedProfileCreationDelegateTest, OnManagedProfileInitialized) {
  auto delegate = std::make_unique<OidcManagedProfileCreationDelegate>(
      kOAuthToken, kIdToken, is_dasher_based(), kSampleName, kSampleEmail);
  Profile* new_profile =
      profile_manager_->CreateTestingProfile("new_test_profile");

  base::RunLoop loop;
  delegate->OnManagedProfileInitialized(
      profile_, new_profile,
      base::BindOnce(
          [&](base::OnceClosure quit_closure, base::WeakPtr<Profile> profile) {
            auto* prefs = profile->GetPrefs();
            EXPECT_EQ(kSampleName,
                      prefs->GetString(
                          enterprise_signin::prefs::kProfileUserDisplayName));
            EXPECT_EQ(
                kSampleEmail,
                prefs->GetString(enterprise_signin::prefs::kProfileUserEmail));
            std::move(quit_closure).Run();
          },
          loop.QuitClosure()));

  loop.Run();
}

INSTANTIATE_TEST_SUITE_P(All,
                         OidcManagedProfileCreationDelegateTest,
                         testing::Bool());
