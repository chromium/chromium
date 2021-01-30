// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/drive/drive_service.h"
#include "base/test/mock_callback.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

class DriveServiceTest : public testing::Test {
 public:
  DriveServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    testing::Test::SetUp();
    service_ = std::make_unique<DriveService>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        identity_test_env.identity_manager());
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

TEST_F(DriveServiceTest, PassesDataOnSuccess) {
  bool empty_response = true;
  base::MockCallback<DriveService::SuggestionsCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&empty_response](std::string arg) {
        empty_response = arg.empty();
      }));

  service_->GetDriveSuggestions(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "token", base::Time());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://appsitemsuggest-pa.googleapis.com/v1/items", std::string(),
      net::HTTP_OK,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);
  EXPECT_FALSE(empty_response);
}

TEST_F(DriveServiceTest, PassesNoDataOnAuthError) {
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

TEST_F(DriveServiceTest, PassesNoDataOnNetError) {
  bool empty_response = false;
  base::MockCallback<DriveService::SuggestionsCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&empty_response](std::string arg) {
        empty_response = arg.empty();
      }));

  service_->GetDriveSuggestions(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());

  ASSERT_EQ(1u, test_url_loader_factory_.pending_requests()->size());

  EXPECT_EQ(
      test_url_loader_factory_.pending_requests()
          ->at(0)
          .request.headers.ToString(),
      "Content-Type: application/json\r\nAuthorization: Bearer foo\r\n\r\n");
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://appsitemsuggest-pa.googleapis.com/v1/items", std::string(),
      net::HTTP_BAD_REQUEST,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);

  EXPECT_TRUE(empty_response);
}
