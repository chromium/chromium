// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service.h"

#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_factory.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search/ntp_features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockMicrosoftAuthServiceObserver : public MicrosoftAuthServiceObserver {
 public:
  MOCK_METHOD0(OnAuthStateUpdated, void());
};

std::unique_ptr<TestingProfile> MakeTestingProfile() {
  TestingProfile::Builder profile_builder;
  return profile_builder.Build();
}

}  // namespace

class MicrosoftAuthServiceTest : public testing::Test {
 public:
  MicrosoftAuthServiceTest() : profile_(MakeTestingProfile()) {
    profile_->GetTestingPrefService()->SetManagedPref(
        prefs::kNtpSharepointModuleVisible, base::Value(true));
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kNtpMicrosoftAuthenticationModule,
                              ntp_features::kNtpSharepointModule},
        /*disabled_features=*/{});
    auth_service_ = MicrosoftAuthServiceFactory::GetForProfile(profile_.get());
  }

  ~MicrosoftAuthServiceTest() override = default;

  void SetUp() override { scoped_observation_.Observe(auth_service_); }

  void TearDown() override { scoped_observation_.Reset(); }

  MicrosoftAuthService& auth_service() { return *auth_service_; }
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }
  MockMicrosoftAuthServiceObserver& observer() { return observer_; }

 private:
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<MicrosoftAuthService> auth_service_;
  MockMicrosoftAuthServiceObserver observer_;
  base::ScopedObservation<MicrosoftAuthService, MicrosoftAuthServiceObserver>
      scoped_observation_{&observer_};
};

TEST_F(MicrosoftAuthServiceTest, ClearAuthData) {
  // Set auth data.
  EXPECT_CALL(observer(), OnAuthStateUpdated).Times(2);
  new_tab_page::mojom::AccessTokenPtr access_token =
      new_tab_page::mojom::AccessToken::New();
  access_token->token = "1234";
  access_token->expiration = base::Time::Now() + base::Minutes(5);
  auth_service().SetAccessToken(std::move(access_token));

  // Clear auth data.
  auth_service().ClearAuthData();

  EXPECT_TRUE(auth_service().GetAccessToken().empty());
  EXPECT_EQ(auth_service().GetAuthState(),
            MicrosoftAuthService::AuthState::kNone);
}

TEST_F(MicrosoftAuthServiceTest, SetAccessToken) {
  EXPECT_CALL(observer(), OnAuthStateUpdated).Times(1);
  new_tab_page::mojom::AccessTokenPtr access_token =
      new_tab_page::mojom::AccessToken::New();
  access_token->token = "1234";
  access_token->expiration = base::Time::Now() + base::Minutes(5);
  auth_service().SetAccessToken(std::move(access_token));

  EXPECT_EQ(auth_service().GetAccessToken(), "1234");
  EXPECT_EQ(auth_service().GetAuthState(),
            MicrosoftAuthService::AuthState::kSuccess);
}

TEST_F(MicrosoftAuthServiceTest, GetExpiredAccessToken) {
  EXPECT_CALL(observer(), OnAuthStateUpdated).Times(2);
  new_tab_page::mojom::AccessTokenPtr access_token =
      new_tab_page::mojom::AccessToken::New();
  access_token->token = "1234";
  access_token->expiration = base::Time::Now() + base::Minutes(5);
  auth_service().SetAccessToken(std::move(access_token));

  // Wait for 30 seconds before expiration.
  task_environment().AdvanceClock(base::Minutes(4.5));

  EXPECT_TRUE(auth_service().GetAccessToken().empty());
  EXPECT_EQ(auth_service().GetAuthState(),
            MicrosoftAuthService::AuthState::kNone);
}

TEST_F(MicrosoftAuthServiceTest, GetEmptyAccessToken) {
  EXPECT_TRUE(auth_service().GetAccessToken().empty());
}

TEST_F(MicrosoftAuthServiceTest, SetAuthStateError) {
  EXPECT_CALL(observer(), OnAuthStateUpdated).Times(1);
  auth_service().SetAuthStateError();

  EXPECT_EQ(auth_service().GetAuthState(),
            MicrosoftAuthService::AuthState::kError);
}

TEST_F(MicrosoftAuthServiceTest, GetAuthState) {
  EXPECT_CALL(observer(), OnAuthStateUpdated).Times(2);
  EXPECT_EQ(auth_service().GetAuthState(),
            MicrosoftAuthService::AuthState::kNone);

  auth_service().SetAuthStateError();

  EXPECT_EQ(auth_service().GetAuthState(),
            MicrosoftAuthService::AuthState::kError);

  new_tab_page::mojom::AccessTokenPtr access_token =
      new_tab_page::mojom::AccessToken::New();
  access_token->token = "1234";
  access_token->expiration = base::Time::Now() + base::Minutes(5);
  auth_service().SetAccessToken(std::move(access_token));

  EXPECT_EQ(auth_service().GetAuthState(),
            MicrosoftAuthService::AuthState::kSuccess);
}
