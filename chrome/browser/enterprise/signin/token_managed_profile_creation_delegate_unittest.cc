// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/token_managed_profile_creation_delegate.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kEnrollmentToken[] = "fake-enrollment-token";

}  // namespace

class TokenManagedProfileCreationDelegateTest : public testing::Test {
 public:
  TokenManagedProfileCreationDelegateTest()
      : profile_manager_(std::make_unique<TestingProfileManager>(
            TestingBrowserProcess::GetGlobal())) {}

  ~TokenManagedProfileCreationDelegateTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test_profile");
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<Profile> profile_;
};

TEST_F(TokenManagedProfileCreationDelegateTest,
       CreatesProfileWithManagementInfo) {
  auto delegate =
      std::make_unique<TokenManagedProfileCreationDelegate>(kEnrollmentToken);

  auto* entry = TestingBrowserProcess::GetGlobal()
                    ->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(profile_->GetPath());
  delegate->SetManagedAttributesForProfile(entry);
  ASSERT_TRUE(entry);
  EXPECT_EQ(kEnrollmentToken, entry->GetProfileManagementEnrollmentToken());
}
