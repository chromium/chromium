// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/gaia_remote_consent_flow.h"

#include <memory>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

const char kResultHistogramName[] =
    "Signin.Extensions.GaiaRemoteConsentFlowResult";

const char kGaiaId[] = "fake_gaia_id";
const char kConsentResult[] = "CAESCUVOQ1JZUFRFRBoMZmFrZV9nYWlhX2lk";

class FakeWebAuthFlow : public WebAuthFlow {
 public:
  explicit FakeWebAuthFlow(WebAuthFlow::Delegate* delegate)
      : WebAuthFlow(delegate,
                    nullptr,
                    GURL(),
                    WebAuthFlow::INTERACTIVE,
                    /*user_gesture=*/true) {}

  ~FakeWebAuthFlow() override = default;

  void Start() override {}
};

class TestGaiaRemoteConsentFlow : public GaiaRemoteConsentFlow {
 public:
  TestGaiaRemoteConsentFlow(GaiaRemoteConsentFlow::Delegate* delegate,
                            const ExtensionTokenKey& token_key,
                            const RemoteConsentResolutionData& resolution_data)
      : GaiaRemoteConsentFlow(delegate,
                              nullptr,
                              token_key,
                              resolution_data,
                              /*user_gesture=*/true) {
    SetWebAuthFlowForTesting(std::make_unique<FakeWebAuthFlow>(this));
  }
};

class MockGaiaRemoteConsentFlowDelegate
    : public GaiaRemoteConsentFlow::Delegate {
 public:
  MOCK_METHOD1(OnGaiaRemoteConsentFlowFailed,
               void(GaiaRemoteConsentFlow::Failure failure));
  MOCK_METHOD2(OnGaiaRemoteConsentFlowApproved,
               void(const std::string& consent_result,
                    const std::string& gaia_id));
};

class IdentityGaiaRemoteConsentFlowTest : public testing::Test {
 public:
  IdentityGaiaRemoteConsentFlowTest() = default;

  void TearDown() override {
    testing::Test::TearDown();
    base::RunLoop()
        .RunUntilIdle();  // Run tasks so all FakeWebAuthFlowWithWindowKey get
                          // deleted.
  }

  std::unique_ptr<TestGaiaRemoteConsentFlow> CreateTestFlow() {
    return CreateTestFlow(&delegate_);
  }

  std::unique_ptr<TestGaiaRemoteConsentFlow> CreateTestFlow(
      GaiaRemoteConsentFlow::Delegate* delegate) {
    CoreAccountInfo user_info;
    user_info.account_id = CoreAccountId::FromGaiaId("account_id");
    user_info.gaia = "account_id";
    user_info.email = "email";

    ExtensionTokenKey token_key("extension_id", user_info,
                                std::set<std::string>());
    RemoteConsentResolutionData resolution_data;
    resolution_data.url = GURL("https://example.com/auth/");
    return std::make_unique<TestGaiaRemoteConsentFlow>(delegate, token_key,
                                                       resolution_data);
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 protected:
  base::test::TaskEnvironment task_env_;
  base::HistogramTester histogram_tester_;
  testing::StrictMock<MockGaiaRemoteConsentFlowDelegate> delegate_;
};

TEST_F(IdentityGaiaRemoteConsentFlowTest, ConsentResult) {
  std::unique_ptr<TestGaiaRemoteConsentFlow> flow = CreateTestFlow();
  EXPECT_CALL(delegate_,
              OnGaiaRemoteConsentFlowApproved(kConsentResult, kGaiaId));
  flow->ReactToConsentResult(kConsentResult);
  histogram_tester()->ExpectUniqueSample(kResultHistogramName,
                                         GaiaRemoteConsentFlow::NONE, 1);
}

TEST_F(IdentityGaiaRemoteConsentFlowTest, ConsentResult_TwoWindows) {
  std::unique_ptr<TestGaiaRemoteConsentFlow> flow = CreateTestFlow();
  testing::StrictMock<MockGaiaRemoteConsentFlowDelegate> delegate2;
  std::unique_ptr<TestGaiaRemoteConsentFlow> flow2 = CreateTestFlow(&delegate2);

  const char kConsentResult2[] = "CAESCkVOQ1JZUFRFRDI";
  EXPECT_CALL(delegate2, OnGaiaRemoteConsentFlowApproved(kConsentResult2, ""));
  flow2->ReactToConsentResult(kConsentResult2);

  EXPECT_CALL(delegate_,
              OnGaiaRemoteConsentFlowApproved(kConsentResult, kGaiaId));
  flow->ReactToConsentResult(kConsentResult);
  histogram_tester()->ExpectUniqueSample(kResultHistogramName,
                                         GaiaRemoteConsentFlow::NONE, 2);
}

TEST_F(IdentityGaiaRemoteConsentFlowTest, InvalidConsentResult) {
  const char kInvalidConsentResult[] = "abc";
  std::unique_ptr<TestGaiaRemoteConsentFlow> flow = CreateTestFlow();
  EXPECT_CALL(delegate_,
              OnGaiaRemoteConsentFlowFailed(
                  GaiaRemoteConsentFlow::Failure::INVALID_CONSENT_RESULT));
  flow->ReactToConsentResult(kInvalidConsentResult);
  histogram_tester()->ExpectUniqueSample(
      kResultHistogramName, GaiaRemoteConsentFlow::INVALID_CONSENT_RESULT, 1);
}

TEST_F(IdentityGaiaRemoteConsentFlowTest, NoGrant) {
  const char kNoGrantConsentResult[] = "CAA";
  std::unique_ptr<TestGaiaRemoteConsentFlow> flow = CreateTestFlow();
  EXPECT_CALL(delegate_, OnGaiaRemoteConsentFlowFailed(
                             GaiaRemoteConsentFlow::Failure::NO_GRANT));
  flow->ReactToConsentResult(kNoGrantConsentResult);
  histogram_tester()->ExpectUniqueSample(kResultHistogramName,
                                         GaiaRemoteConsentFlow::NO_GRANT, 1);
}

TEST_F(IdentityGaiaRemoteConsentFlowTest, WebAuthFlowFailure_WindowClosed) {
  std::unique_ptr<TestGaiaRemoteConsentFlow> flow = CreateTestFlow();
  EXPECT_CALL(delegate_, OnGaiaRemoteConsentFlowFailed(
                             GaiaRemoteConsentFlow::Failure::WINDOW_CLOSED));
  flow->OnAuthFlowFailure(WebAuthFlow::Failure::WINDOW_CLOSED);
  histogram_tester()->ExpectUniqueSample(
      kResultHistogramName, GaiaRemoteConsentFlow::WINDOW_CLOSED, 1);
}

TEST_F(IdentityGaiaRemoteConsentFlowTest, WebAuthFlowFailure_LoadFailed) {
  std::unique_ptr<TestGaiaRemoteConsentFlow> flow = CreateTestFlow();
  EXPECT_CALL(delegate_, OnGaiaRemoteConsentFlowFailed(
                             GaiaRemoteConsentFlow::Failure::LOAD_FAILED));
  flow->OnAuthFlowFailure(WebAuthFlow::Failure::LOAD_FAILED);
  histogram_tester()->ExpectUniqueSample(kResultHistogramName,
                                         GaiaRemoteConsentFlow::LOAD_FAILED, 1);
}

}  // namespace extensions
