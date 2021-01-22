// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/drive/drive_service.h"
#include "base/test/mock_callback.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

class DriveServiceTest : public testing::Test {
 public:
  DriveServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    testing::Test::SetUp();

    service_ =
        std::make_unique<DriveService>(identity_test_env.identity_manager());
    identity_test_env.MakePrimaryAccountAvailable("example@google.com");
  }

  void TearDown() override {
    service_.reset();
    test_url_loader_factory_.ClearResponses();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<DriveService> service_;
  signin::IdentityTestEnvironment identity_test_env;
};

TEST_F(DriveServiceTest, GeneratesTokenOnFetchSuccess) {
  bool token_is_valid;

  base::MockCallback<DriveService::SuggestionsCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&token_is_valid](std::string suggestions) {
        token_is_valid = !suggestions.empty();
      }));

  service_->GetDriveSuggestions(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "token", base::Time());

  EXPECT_TRUE(token_is_valid);
}

TEST_F(DriveServiceTest, PassesEmptyStringOnError) {
  bool token_is_valid = true;

  base::MockCallback<DriveService::SuggestionsCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&token_is_valid](std::string suggestion) {
        token_is_valid = !suggestion.empty();
      }));

  service_->GetDriveSuggestions(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::State::CONNECTION_FAILED));

  EXPECT_FALSE(token_is_valid);
}
