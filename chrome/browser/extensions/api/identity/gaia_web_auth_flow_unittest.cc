// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/gaia_web_auth_flow.h"

#include <vector>

#include "base/run_loop.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class FakeWebAuthFlow : public WebAuthFlow {
 public:
  explicit FakeWebAuthFlow(WebAuthFlow::Delegate* delegate)
      : WebAuthFlow(delegate,
                    NULL,
                    GURL(),
                    WebAuthFlow::INTERACTIVE) {}

  void Start() override {}
};

class TestGaiaWebAuthFlow : public GaiaWebAuthFlow {
 public:
  TestGaiaWebAuthFlow(GaiaWebAuthFlow::Delegate* delegate,
                      const ExtensionTokenKey* token_key,
                      const std::string oauth2_client_id,
                      GoogleServiceAuthError::State ubertoken_error_state)
      : GaiaWebAuthFlow(delegate, NULL, token_key, oauth2_client_id, "en-us"),
        ubertoken_error_(ubertoken_error_state) {}

  void Start() override {
    OnUbertokenFetchComplete(
        ubertoken_error_,
        ubertoken_error_.state() == GoogleServiceAuthError::NONE
            ? "fake_ubertoken"
            : std::string());
  }

 private:
  std::unique_ptr<WebAuthFlow> CreateWebAuthFlow(GURL url) override {
    return std::unique_ptr<WebAuthFlow>(new FakeWebAuthFlow(this));
  }

  GoogleServiceAuthError ubertoken_error_;
};

class MockGaiaWebAuthFlowDelegate : public GaiaWebAuthFlow::Delegate {
 public:
  MOCK_METHOD3(OnGaiaFlowFailure,
               void(GaiaWebAuthFlow::Failure failure,
                    GoogleServiceAuthError service_error,
                    const std::string& oauth_error));
  MOCK_METHOD2(OnGaiaFlowCompleted,
               void(const std::string& access_token,
                    const std::string& expiration));
};

class IdentityGaiaWebAuthFlowTest : public testing::Test {
 public:
  IdentityGaiaWebAuthFlowTest()
      : ubertoken_error_state_(GoogleServiceAuthError::NONE) {}

  void TearDown() override {
    testing::Test::TearDown();
    base::RunLoop loop;
    loop.RunUntilIdle();  // Run tasks so FakeWebAuthFlows get deleted.
  }

  std::unique_ptr<TestGaiaWebAuthFlow> CreateTestFlow() {
    ExtensionTokenKey token_key("extension_id", CoreAccountId("account_id"),
                                std::set<std::string>());
    return std::unique_ptr<TestGaiaWebAuthFlow>(new TestGaiaWebAuthFlow(
        &delegate_, &token_key, "fake.client.id", ubertoken_error_state_));
  }

  std::string GetFinalTitle(const std::string& fragment) {
    return std::string("Loading id.client.fake:/extension_id#") + fragment;
  }

  GoogleServiceAuthError GetNoneServiceError() {
    return GoogleServiceAuthError(GoogleServiceAuthError::NONE);
  }

  void set_ubertoken_error(
      GoogleServiceAuthError::State ubertoken_error_state) {
    ubertoken_error_state_ = ubertoken_error_state;
  }

 protected:
  testing::StrictMock<MockGaiaWebAuthFlowDelegate> delegate_;
  GoogleServiceAuthError::State ubertoken_error_state_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(IdentityGaiaWebAuthFlowTest, OAuthError) {
  std::unique_ptr<TestGaiaWebAuthFlow> flow = CreateTestFlow();
  flow->Start();
  EXPECT_CALL(delegate_, OnGaiaFlowFailure(
          GaiaWebAuthFlow::OAUTH_ERROR,
          GoogleServiceAuthError(GoogleServiceAuthError::NONE),
          "access_denied"));
  flow->OnAuthFlowTitleChange(GetFinalTitle("error=access_denied"));
}

TEST_F(IdentityGaiaWebAuthFlowTest, Token) {
  std::unique_ptr<TestGaiaWebAuthFlow> flow = CreateTestFlow();
  flow->Start();
  EXPECT_CALL(delegate_, OnGaiaFlowCompleted("fake_access_token", ""));
  flow->OnAuthFlowTitleChange(GetFinalTitle("access_token=fake_access_token"));
}

TEST_F(IdentityGaiaWebAuthFlowTest, TokenAndExpiration) {
  std::unique_ptr<TestGaiaWebAuthFlow> flow = CreateTestFlow();
  flow->Start();
  EXPECT_CALL(delegate_, OnGaiaFlowCompleted("fake_access_token", "3600"));
  flow->OnAuthFlowTitleChange(
      GetFinalTitle("access_token=fake_access_token&expires_in=3600"));
}

TEST_F(IdentityGaiaWebAuthFlowTest, ExtraFragmentParametersSuccess) {
  std::unique_ptr<TestGaiaWebAuthFlow> flow = CreateTestFlow();
  flow->Start();
  EXPECT_CALL(delegate_,
              OnGaiaFlowCompleted("fake_access_token", "3600"));
  flow->OnAuthFlowTitleChange(GetFinalTitle("chaff1=stuff&"
                                            "expires_in=3600&"
                                            "chaff2=and&"
                                            "nonerror=fake_error&"
                                            "chaff3=nonsense&"
                                            "access_token=fake_access_token&"
                                            "chaff4="));
}

TEST_F(IdentityGaiaWebAuthFlowTest, ExtraFragmentParametersError) {
  std::unique_ptr<TestGaiaWebAuthFlow> flow = CreateTestFlow();
  flow->Start();
  EXPECT_CALL(delegate_, OnGaiaFlowFailure(
          GaiaWebAuthFlow::OAUTH_ERROR,
          GoogleServiceAuthError(GoogleServiceAuthError::NONE),
          "fake_error"));
  flow->OnAuthFlowTitleChange(GetFinalTitle("chaff1=stuff&"
                                            "expires_in=3600&"
                                            "chaff2=and&"
                                            "error=fake_error&"
                                            "chaff3=nonsense&"
                                            "access_token=fake_access_token&"
                                            "chaff4="));
}

TEST_F(IdentityGaiaWebAuthFlowTest, TitleSpam) {
  std::unique_ptr<TestGaiaWebAuthFlow> flow = CreateTestFlow();
  flow->Start();
  flow->OnAuthFlowTitleChange(
      "Loading https://extension_id.chromiumapp.org/#error=non_final_title");
  flow->OnAuthFlowTitleChange("I'm feeling entitled.");
  flow->OnAuthFlowTitleChange("");
  flow->OnAuthFlowTitleChange(
      "Loading id.client.fake:/bad_extension_id#error=non_final_title");
  flow->OnAuthFlowTitleChange(
      "Loading bad.id.client.fake:/extension_id#error=non_final_title");
  EXPECT_CALL(delegate_, OnGaiaFlowCompleted("fake_access_token", ""));
  flow->OnAuthFlowTitleChange(GetFinalTitle("access_token=fake_access_token"));
}

TEST_F(IdentityGaiaWebAuthFlowTest, EmptyFragment) {
  std::unique_ptr<TestGaiaWebAuthFlow> flow = CreateTestFlow();
  flow->Start();
  EXPECT_CALL(
      delegate_,
      OnGaiaFlowFailure(
          GaiaWebAuthFlow::INVALID_REDIRECT,
          GoogleServiceAuthError(GoogleServiceAuthError::NONE),
          ""));
  flow->OnAuthFlowTitleChange(GetFinalTitle(""));
}

TEST_F(IdentityGaiaWebAuthFlowTest, JunkFragment) {
  std::unique_ptr<TestGaiaWebAuthFlow> flow = CreateTestFlow();
  flow->Start();
  EXPECT_CALL(
      delegate_,
      OnGaiaFlowFailure(
          GaiaWebAuthFlow::INVALID_REDIRECT,
          GoogleServiceAuthError(GoogleServiceAuthError::NONE),
          ""));
  flow->OnAuthFlowTitleChange(GetFinalTitle("thisisjustabunchofjunk"));
}

TEST_F(IdentityGaiaWebAuthFlowTest, NoFragment) {
  std::unique_ptr<TestGaiaWebAuthFlow> flow = CreateTestFlow();
  flow->Start();
  // This won't be recognized as an interesting title.
  flow->OnAuthFlowTitleChange("Loading id.client.fake:/extension_id");
}

TEST_F(IdentityGaiaWebAuthFlowTest, Host) {
  std::unique_ptr<TestGaiaWebAuthFlow> flow = CreateTestFlow();
  flow->Start();
  // These won't be recognized as interesting titles.
  flow->OnAuthFlowTitleChange(
      "Loading id.client.fake://extension_id#access_token=fake_access_token");
  flow->OnAuthFlowTitleChange(
      "Loading id.client.fake://extension_id/#access_token=fake_access_token");
  flow->OnAuthFlowTitleChange(
      "Loading "
      "id.client.fake://host/extension_id/#access_token=fake_access_token");
}

TEST_F(IdentityGaiaWebAuthFlowTest, UbertokenFailure) {
  set_ubertoken_error(GoogleServiceAuthError::CONNECTION_FAILED);
  std::unique_ptr<TestGaiaWebAuthFlow> flow = CreateTestFlow();
  EXPECT_CALL(
      delegate_,
      OnGaiaFlowFailure(
          GaiaWebAuthFlow::SERVICE_AUTH_ERROR,
          GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED),
          ""));
  flow->Start();
}

TEST_F(IdentityGaiaWebAuthFlowTest, AuthFlowFailure) {
  std::unique_ptr<TestGaiaWebAuthFlow> flow = CreateTestFlow();
  flow->Start();
  EXPECT_CALL(
      delegate_,
      OnGaiaFlowFailure(
          GaiaWebAuthFlow::WINDOW_CLOSED,
          GoogleServiceAuthError(GoogleServiceAuthError::NONE),
          ""));
  flow->OnAuthFlowFailure(WebAuthFlow::WINDOW_CLOSED);
}

}  // namespace extensions
