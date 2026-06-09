// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service_impl.h"

#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/autofill/actor/actor_test_utils.h"
#include "chrome/browser/autofill/one_time_token_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_suggestion.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class MockOneTimeTokenService : public one_time_tokens::OneTimeTokenService {
 public:
  MOCK_METHOD(void,
              GetRecentOneTimeTokens,
              (one_time_tokens::OneTimeTokenService::Callback),
              (override));
  MOCK_METHOD(std::vector<one_time_tokens::OneTimeToken>,
              GetCachedOneTimeTokens,
              (),
              (const, override));
  MOCK_METHOD(one_time_tokens::ExpiringSubscription,
              Subscribe,
              (one_time_tokens::OneTimeTokenSource,
               base::Time,
               one_time_tokens::OneTimeTokenService::Callback),
              (override));
  MOCK_METHOD(
      void,
      RequestOneTimeToken,
      (base::TimeDelta,
       base::OnceCallback<void(std::optional<one_time_tokens::OneTimeToken>)>),
      (override));
};

class ActorOneTimeTokenFillingServiceImplTest : public ActorTestBase {
 public:
  void SetUp() override {
    ActorTestBase::SetUp();
    OneTimeTokenServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<NiceMock<MockOneTimeTokenService>>();
        }));

    service_ = std::make_unique<ActorOneTimeTokenFillingServiceImpl>(profile());
  }

  void TearDown() override {
    service_.reset();
    ActorTestBase::TearDown();
  }

  MockOneTimeTokenService& otp_service() {
    return *static_cast<MockOneTimeTokenService*>(
        OneTimeTokenServiceFactory::GetForProfile(profile()));
  }

  ActorOneTimeTokenFillingServiceImpl& service() { return *service_; }

 private:
  std::unique_ptr<ActorOneTimeTokenFillingServiceImpl> service_;
};

// Tests that `RetrieveOtp` correctly returns an available OTP from the
// underlying service.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, RetrieveOtp_Success) {
  const std::string kOtp = "123456";
  EXPECT_CALL(otp_service(), GetCachedOneTimeTokens())
      .WillOnce(Return(std::vector<one_time_tokens::OneTimeToken>{
          {one_time_tokens::OneTimeTokenType::kGmail, kOtp,
           base::TimeTicks::Now()}}));

  base::test::TestFuture<std::string> future;
  service().RetrieveOtp(tab().GetHandle(), {}, future.GetCallback());
  EXPECT_EQ(future.Get(), kOtp);
}

// Tests that `RetrieveOtp` correctly selects the most recent Gmail OTP when
// multiple tokens of different types and arrival times are cached.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, RetrieveOtp_MultipleTokens) {
  const std::string kSmsOtp = "111111";
  const std::string kOldGmailOtp = "222222";
  const std::string kRecentGmailOtp = "333333";

  base::TimeTicks now = base::TimeTicks::Now();

  std::vector<one_time_tokens::OneTimeToken> cached_tokens = {
      {one_time_tokens::OneTimeTokenType::kSmsOtp, kSmsOtp,
       now + base::Minutes(5)},  // Most recent, but wrong type
      {one_time_tokens::OneTimeTokenType::kGmail, kOldGmailOtp,
       now - base::Minutes(2)},  // Correct type, but older
      {one_time_tokens::OneTimeTokenType::kGmail, kRecentGmailOtp,
       now - base::Minutes(1)}  // Correct type, most recent valid
  };

  EXPECT_CALL(otp_service(), GetCachedOneTimeTokens())
      .WillOnce(Return(cached_tokens));

  base::test::TestFuture<std::string> future;
  service().RetrieveOtp(tab().GetHandle(), {}, future.GetCallback());
  EXPECT_EQ(future.Get(), kRecentGmailOtp);
}

// Tests that `RetrieveOtp` returns an empty string when no OTPs are available.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, RetrieveOtp_NoTokens) {
  EXPECT_CALL(otp_service(), GetCachedOneTimeTokens())
      .WillOnce(Return(std::vector<one_time_tokens::OneTimeToken>{}));

  EXPECT_CALL(otp_service(), Subscribe(_, _, _))
      .WillOnce([](one_time_tokens::OneTimeTokenSource source,
                   base::Time expiration,
                   one_time_tokens::OneTimeTokenService::Callback callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](one_time_tokens::OneTimeTokenService::Callback callback,
                   one_time_tokens::OneTimeTokenSource source) {
                  callback.Run(source,
                               base::unexpected(
                                   one_time_tokens::OneTimeTokenRetrievalError::
                                       kUnknown));
                },
                std::move(callback), source));
        return one_time_tokens::ExpiringSubscription();
      });

  base::test::TestFuture<std::string> future;
  service().RetrieveOtp(tab().GetHandle(), {}, future.GetCallback());
  EXPECT_EQ(future.Get(), "");
}

// Tests that `RetrieveOtp` fails gracefully when the tab is null.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, RetrieveOtp_TabNull) {
  base::test::TestFuture<std::string> future;
  service().RetrieveOtp(tabs::TabHandle(), {}, future.GetCallback());
  EXPECT_EQ(future.Get(), "");
}

// Tests that `RetrieveOtp` fails gracefully when the OTP service is null.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, RetrieveOtp_ServiceNull) {
  OneTimeTokenServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
        return nullptr;
      }));

  base::test::TestFuture<std::string> future;
  service().RetrieveOtp(tab().GetHandle(), {}, future.GetCallback());
  EXPECT_EQ(future.Get(), "");
}

// Tests that multiple sequential `RetrieveOtp` calls supersede previous ones,
// running previous callbacks with an empty string.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, RetrieveOtp_Superseded) {
  EXPECT_CALL(otp_service(), GetCachedOneTimeTokens())
      .WillRepeatedly(Return(std::vector<one_time_tokens::OneTimeToken>{}));

  base::test::TestFuture<std::string> future1;
  base::test::TestFuture<std::string> future2;

  service().RetrieveOtp(tab().GetHandle(), {}, future1.GetCallback());
  service().RetrieveOtp(tab().GetHandle(), {}, future2.GetCallback());

  EXPECT_EQ(future1.Get(), "");
}

}  // namespace

}  // namespace autofill
