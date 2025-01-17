// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service.h"

#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<TestingProfile> MakeTestingProfile() {
  TestingProfile::Builder profile_builder;
  return profile_builder.Build();
}

}  // namespace

class MicrosoftAuthServiceTest : public testing::Test {
 public:
  MicrosoftAuthServiceTest()
      : profile_(MakeTestingProfile()),
        auth_service_(
            MicrosoftAuthServiceFactory::GetForProfile(profile_.get())) {}

  ~MicrosoftAuthServiceTest() override = default;

  MicrosoftAuthService& auth_service() { return *auth_service_; }
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<MicrosoftAuthService> auth_service_;
};

TEST_F(MicrosoftAuthServiceTest, SetAccessToken) {
  new_tab_page::mojom::AccessTokenPtr access_token =
      new_tab_page::mojom::AccessToken::New();
  access_token->token = "1234";
  access_token->expiration = base::Time::Now() + base::Minutes(5);
  auth_service().SetAccessToken(std::move(access_token));

  EXPECT_EQ(auth_service().GetAccessToken(), "1234");

  // Wait for 30 seconds before expiration.
  task_environment().AdvanceClock(base::Minutes(4.5));

  EXPECT_TRUE(auth_service().GetAccessToken().empty());
}

TEST_F(MicrosoftAuthServiceTest, GetEmptyAccessToken) {
  EXPECT_TRUE(auth_service().GetAccessToken().empty());
}
