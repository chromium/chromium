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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_login_context.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service_metrics.h"
#include "chrome/browser/autofill/one_time_token_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/actor/core/actor_switches.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_suggestion.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/cert/cert_status_flags.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::one_time_tokens::OneTimeTokenRetrievalError;

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

class TestActorContentAutofillDriver : public TestContentAutofillDriver {
 public:
  TestActorContentAutofillDriver(content::RenderFrameHost* rfh,
                                 ContentAutofillDriverFactory* factory)
      : TestContentAutofillDriver(rfh, factory) {}
  ~TestActorContentAutofillDriver() override = default;

  MOCK_METHOD(
      base::flat_set<FieldGlobalId>,
      ApplyFormAction,
      (mojom::FormActionType action_type,
       mojom::ActionPersistence action_persistence,
       base::span<const FormFieldData> fields,
       const FillId& fill_id,
       bool supports_refill,
       const url::Origin& triggered_origin,
       (const absl::flat_hash_map<FieldGlobalId, FieldType>& field_type_map),
       const Section& section_for_clear_form_on_ios),
      (override));
};

class TestActorChromeAutofillClient : public TestContentAutofillClient {
 public:
  explicit TestActorChromeAutofillClient(content::WebContents* web_contents)
      : TestContentAutofillClient(web_contents) {}
  ~TestActorChromeAutofillClient() override = default;

  std::unique_ptr<AutofillManager> CreateManager(
      base::PassKey<ContentAutofillDriver> pass_key,
      ContentAutofillDriver& driver) override {
    return std::make_unique<TestBrowserAutofillManager>(&driver);
  }
};

class ActorOneTimeTokenFillingServiceImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ActorOneTimeTokenFillingServiceImplTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ActorOneTimeTokenFillingServiceImplTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ON_CALL(mock_tab, GetContents())
        .WillByDefault(testing::Return(web_contents()));
    NavigateAndCommit(GURL("about:blank"));

    ON_CALL(driver(), ApplyFormAction)
        .WillByDefault([&](mojom::FormActionType action_type,
                           mojom::ActionPersistence action_persistence,
                           base::span<const FormFieldData> fields,
                           const FillId& fill_id, bool supports_refill,
                           const url::Origin& triggered_origin,
                           const absl::flat_hash_map<FieldGlobalId, FieldType>&
                               field_type_map,
                           const Section& section_for_clear_form_on_ios) {
          base::flat_set<FieldGlobalId> filled_fields =
              driver().TestContentAutofillDriver::ApplyFormAction(
                  action_type, action_persistence, fields, fill_id,
                  supports_refill, triggered_origin, field_type_map,
                  section_for_clear_form_on_ios);
          for (const FormFieldData& field : fields) {
            if (filled_fields.contains(field.global_id())) {
              last_filled_values_[field.global_id()] = field.value();
            }
          }
          return filled_fields;
        });

    OneTimeTokenServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<NiceMock<MockOneTimeTokenService>>();
        }));

    service_ = std::make_unique<ActorOneTimeTokenFillingServiceImpl>(profile());
  }

  void TearDown() override {
    service_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  MockOneTimeTokenService& otp_service() {
    return *static_cast<MockOneTimeTokenService*>(
        OneTimeTokenServiceFactory::GetForProfile(profile()));
  }

  ActorOneTimeTokenFillingServiceImpl& service() { return *service_; }

  TestActorChromeAutofillClient& client() {
    return *static_cast<TestActorChromeAutofillClient*>(
        autofill_client_injector_[web_contents()]);
  }

  TestActorContentAutofillDriver& driver() {
    return CHECK_DEREF(autofill_driver_injector_[web_contents()]);
  }

  TestBrowserAutofillManager& manager() {
    return static_cast<TestBrowserAutofillManager&>(
        driver().GetAutofillManager());
  }

  FormData SeeForm(test::FormDescription form_description) {
    FormData form = test::GetFormData(form_description);
    manager().AddSeenForm(form, test::GetHeuristicTypes(form_description),
                          test::GetServerTypes(form_description));
    return form;
  }

  const absl::flat_hash_map<FieldGlobalId, std::u16string>& last_filled_values()
      const {
    return last_filled_values_;
  }

  tabs::TabInterface& tab() { return mock_tab; }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  tabs::MockTabInterface mock_tab;
  TestAutofillClientInjector<TestActorChromeAutofillClient>
      autofill_client_injector_;
  TestAutofillDriverInjector<TestActorContentAutofillDriver>
      autofill_driver_injector_;

 protected:
  std::unique_ptr<ActorOneTimeTokenFillingServiceImpl> service_;
  absl::flat_hash_map<FieldGlobalId, std::u16string> last_filled_values_;
  base::HistogramTester histogram_tester_;
};

// Tests that `RetrieveOtp` returns the mock OTP immediately from the command line
// switch when the switch is set.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, RetrieveOtp_MockOtpSwitchSet) {
  const std::string kMockOtp = "987654";
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      ::actor::switches::kAttemptOtpFillingMockGmailOtpValue, kMockOtp);

  // The OTP service is not expected to be queried for cached tokens or
  // subscription since RetrieveOtp returns early.
  EXPECT_CALL(otp_service(), GetCachedOneTimeTokens).Times(0);
  EXPECT_CALL(otp_service(), Subscribe).Times(0);

  base::test::TestFuture<
      base::expected<std::string, OneTimeTokenRetrievalError>>
      future;
  service().RetrieveOtp(tab().GetHandle(), {}, future.GetCallback());
  EXPECT_EQ(future.Get().value(), kMockOtp);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
      ActorOneTimeTokenFillingServiceRetrieveOtp::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
      ActorOneTimeTokenFillingServiceRetrieveOtp::kMockOtp, 1);
}

// Tests that `RetrieveOtp` correctly returns an available OTP from the
// underlying service.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, RetrieveOtp_Success) {
  const std::string kOtp = "123456";
  EXPECT_CALL(otp_service(), GetCachedOneTimeTokens())
      .WillOnce(Return(std::vector<one_time_tokens::OneTimeToken>{
          {one_time_tokens::OneTimeTokenType::kGmail, kOtp,
           base::TimeTicks::Now(), "sender@example.com"}}));

  base::test::TestFuture<
      base::expected<std::string, OneTimeTokenRetrievalError>>
      future;
  service().RetrieveOtp(tab().GetHandle(), {}, future.GetCallback());
  EXPECT_EQ(future.Get().value(), kOtp);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
      ActorOneTimeTokenFillingServiceRetrieveOtp::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
      ActorOneTimeTokenFillingServiceRetrieveOtp::kCacheHit, 1);
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
       now - base::Minutes(2),
       "sender@example.com"},  // Correct type, but older
      {one_time_tokens::OneTimeTokenType::kGmail, kRecentGmailOtp,
       now - base::Minutes(1), "sender@example.com"}
      // Correct type, most recent valid
  };

  EXPECT_CALL(otp_service(), GetCachedOneTimeTokens())
      .WillOnce(Return(cached_tokens));

  base::test::TestFuture<
      base::expected<std::string, OneTimeTokenRetrievalError>>
      future;
  service().RetrieveOtp(tab().GetHandle(), {}, future.GetCallback());
  EXPECT_EQ(future.Get().value(), kRecentGmailOtp);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
      ActorOneTimeTokenFillingServiceRetrieveOtp::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
      ActorOneTimeTokenFillingServiceRetrieveOtp::kCacheHit, 1);
}

// Tests that `RetrieveOtp` returns an empty string when no OTPs are available.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, RetrieveOtp_NoTokens) {
  EXPECT_CALL(otp_service(), GetCachedOneTimeTokens())
      .WillOnce(Return(std::vector<one_time_tokens::OneTimeToken>{}));

  EXPECT_CALL(otp_service(), Subscribe)
      .WillOnce([](one_time_tokens::OneTimeTokenSource source,
                   base::Time expiration,
                   one_time_tokens::OneTimeTokenService::Callback callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](one_time_tokens::OneTimeTokenService::Callback callback,
                   one_time_tokens::OneTimeTokenSource source) {
                  callback.Run(
                      source,
                      base::unexpected(OneTimeTokenRetrievalError::kUnknown));
                },
                std::move(callback), source));
        return one_time_tokens::ExpiringSubscription();
      });

  base::test::TestFuture<
      base::expected<std::string, OneTimeTokenRetrievalError>>
      future;
  service().RetrieveOtp(tab().GetHandle(), {}, future.GetCallback());
  EXPECT_EQ(future.Get().error(), OneTimeTokenRetrievalError::kUnknown);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
      ActorOneTimeTokenFillingServiceRetrieveOtp::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
      ActorOneTimeTokenFillingServiceRetrieveOtp::kError, 1);
}

// Tests that `RetrieveOtp` fails gracefully when the tab is null.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, RetrieveOtp_TabNull) {
  base::test::TestFuture<
      base::expected<std::string, OneTimeTokenRetrievalError>>
      future;
  service().RetrieveOtp(tabs::TabHandle(), {}, future.GetCallback());
  EXPECT_EQ(future.Get().error(), OneTimeTokenRetrievalError::kGmailOtpUnknown);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
      ActorOneTimeTokenFillingServiceRetrieveOtp::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
      ActorOneTimeTokenFillingServiceRetrieveOtp::kNullTab, 1);
}

// Tests that `RetrieveOtp` fails gracefully when the OTP service is null.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, RetrieveOtp_ServiceNull) {
  OneTimeTokenServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(
                     [](content::BrowserContext* context)
                         -> std::unique_ptr<KeyedService> { return nullptr; }));

  base::test::TestFuture<
      base::expected<std::string, OneTimeTokenRetrievalError>>
      future;
  service().RetrieveOtp(tab().GetHandle(), {}, future.GetCallback());
  EXPECT_EQ(future.Get().error(),
            OneTimeTokenRetrievalError::kGmailOtpBackendApiNotAvailable);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
      ActorOneTimeTokenFillingServiceRetrieveOtp::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
      ActorOneTimeTokenFillingServiceRetrieveOtp::kNoService, 1);
}

// Tests that multiple sequential `RetrieveOtp` calls supersede previous ones,
// running previous callbacks with an empty string.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, RetrieveOtp_Superseded) {
  EXPECT_CALL(otp_service(), GetCachedOneTimeTokens())
      .WillRepeatedly(Return(std::vector<one_time_tokens::OneTimeToken>{}));

  base::test::TestFuture<
      base::expected<std::string, OneTimeTokenRetrievalError>>
      future1;
  base::test::TestFuture<
      base::expected<std::string, OneTimeTokenRetrievalError>>
      future2;

  service().RetrieveOtp(tab().GetHandle(), {}, future1.GetCallback());
  service().RetrieveOtp(tab().GetHandle(), {}, future2.GetCallback());

  EXPECT_EQ(future1.Get().error(),
            OneTimeTokenRetrievalError::kGmailOtpUnknown);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
      ActorOneTimeTokenFillingServiceRetrieveOtp::kStart, 2);
}

// Tests that a pending subscription request is superseded when a subsequent
// request hits the cache.
TEST_F(ActorOneTimeTokenFillingServiceImplTest,
       RetrieveOtp_SupersededByCacheHit) {
  const std::string kOtp = "123456";
  EXPECT_CALL(otp_service(), GetCachedOneTimeTokens())
      .WillOnce(Return(std::vector<one_time_tokens::OneTimeToken>{}))
      .WillOnce(Return(std::vector<one_time_tokens::OneTimeToken>{
          {one_time_tokens::OneTimeTokenType::kGmail, kOtp,
           base::TimeTicks::Now(), "sender@example.com"}}));

  base::test::TestFuture<
      base::expected<std::string, OneTimeTokenRetrievalError>>
      future1;
  base::test::TestFuture<
      base::expected<std::string, OneTimeTokenRetrievalError>>
      future2;

  service().RetrieveOtp(tab().GetHandle(), {}, future1.GetCallback());
  service().RetrieveOtp(tab().GetHandle(), {}, future2.GetCallback());

  EXPECT_EQ(future1.Get().error(),
            OneTimeTokenRetrievalError::kGmailOtpUnknown);
  EXPECT_EQ(future2.Get().value(), kOtp);

  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
      ActorOneTimeTokenFillingServiceRetrieveOtp::kStart, 2);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
      ActorOneTimeTokenFillingServiceRetrieveOtp::kCacheHit, 1);
}

// Tests that OnOneTimeTokenReceived handles the case where
// retrieve_otp_callback_ has already been cleared (e.g. because the request was
// resolved or superseded).
TEST_F(ActorOneTimeTokenFillingServiceImplTest,
       RetrieveOtp_OnOneTimeTokenReceived_NoCallback) {
  one_time_tokens::OneTimeTokenService::Callback sub_callback1;
  one_time_tokens::OneTimeTokenService::Callback sub_callback2;

  EXPECT_CALL(otp_service(), GetCachedOneTimeTokens())
      .WillRepeatedly(Return(std::vector<one_time_tokens::OneTimeToken>{}));

  EXPECT_CALL(otp_service(), Subscribe)
      .WillOnce([&](one_time_tokens::OneTimeTokenSource source,
                    base::Time expiration,
                    one_time_tokens::OneTimeTokenService::Callback callback) {
        sub_callback1 = std::move(callback);
        return one_time_tokens::ExpiringSubscription();
      })
      .WillOnce([&](one_time_tokens::OneTimeTokenSource source,
                    base::Time expiration,
                    one_time_tokens::OneTimeTokenService::Callback callback) {
        sub_callback2 = std::move(callback);
        return one_time_tokens::ExpiringSubscription();
      });

  base::test::TestFuture<
      base::expected<std::string, OneTimeTokenRetrievalError>>
      future1;
  base::test::TestFuture<
      base::expected<std::string, OneTimeTokenRetrievalError>>
      future2;

  // Call 1: subscribes, saves sub_callback1.
  service().RetrieveOtp(tab().GetHandle(), {}, future1.GetCallback());

  // Call 2: supersedes Call 1, subscribes, saves sub_callback2.
  service().RetrieveOtp(tab().GetHandle(), {}, future2.GetCallback());

  EXPECT_EQ(future1.Get().error(),
            OneTimeTokenRetrievalError::kGmailOtpUnknown);

  // Resolve Call 2 successfully.
  const std::string kOtp = "123456";
  sub_callback2.Run(one_time_tokens::OneTimeTokenSource::kGmail,
                    one_time_tokens::OneTimeToken(
                        one_time_tokens::OneTimeTokenType::kGmail, kOtp,
                        base::TimeTicks::Now(), "sender@example.com"));

  EXPECT_EQ(future2.Get().value(), kOtp);

  // At this point, retrieve_otp_callback_ is null.
  // Run sub_callback1 (the old subscription callback).
  sub_callback1.Run(one_time_tokens::OneTimeTokenSource::kGmail,
                    base::unexpected(OneTimeTokenRetrievalError::kUnknown));

  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
      ActorOneTimeTokenFillingServiceRetrieveOtp::kStart, 2);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
      ActorOneTimeTokenFillingServiceRetrieveOtp::kSuccess, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
      ActorOneTimeTokenFillingServiceRetrieveOtp::kNoCallback, 1);
}

// Tests that multiple sequential `RetrieveOtp` calls supersede previous ones,
// running previous callbacks with an error when the second call has a cached
// token.
TEST_F(ActorOneTimeTokenFillingServiceImplTest,
       RetrieveOtp_SupersededByCachedToken) {
  const std::string kOtp = "123456";
  EXPECT_CALL(otp_service(), GetCachedOneTimeTokens())
      .WillOnce(Return(std::vector<one_time_tokens::OneTimeToken>{}))
      .WillOnce(Return(std::vector<one_time_tokens::OneTimeToken>{
          {one_time_tokens::OneTimeTokenType::kGmail, kOtp,
           base::TimeTicks::Now(), "sender@example.com"}}));

  base::test::TestFuture<
      base::expected<std::string, OneTimeTokenRetrievalError>>
      future1;
  base::test::TestFuture<
      base::expected<std::string, OneTimeTokenRetrievalError>>
      future2;

  service().RetrieveOtp(tab().GetHandle(), {}, future1.GetCallback());
  service().RetrieveOtp(tab().GetHandle(), {}, future2.GetCallback());

  EXPECT_EQ(future1.Get().error(),
            OneTimeTokenRetrievalError::kGmailOtpUnknown);
  EXPECT_EQ(future2.Get().value(), kOtp);

  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
      ActorOneTimeTokenFillingServiceRetrieveOtp::kStart, 2);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
      ActorOneTimeTokenFillingServiceRetrieveOtp::kCacheHit, 1);
}

// Tests that `FillOtp` fails gracefully when the tab is null.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, FillOtp_TabNull) {
  base::test::TestFuture<bool> future;
  service().FillOtp(tabs::TabHandle(), {test::MakeFieldGlobalId()}, "123456",
                    future.GetCallback());
  EXPECT_FALSE(future.Get());
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceFillOtpHistogram,
      ActorOneTimeTokenFillingServiceFillOtp::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceFillOtpHistogram,
      ActorOneTimeTokenFillingServiceFillOtp::kNullTab, 1);
}

// Tests that `FillOtp` fails gracefully when the AutofillManager is not
// available for the given tab.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, FillOtp_AutofillManagerNull) {
  // Create a separate profile and WebContents that doesn't have the
  // AutofillClient injected by the test base.
  TestingProfile other_profile;
  std::unique_ptr<content::WebContents> other_web_contents =
      content::WebContentsTester::CreateTestWebContents(&other_profile, nullptr);

  tabs::MockTabInterface other_tab;
  EXPECT_CALL(other_tab, GetContents())
      .WillRepeatedly(testing::Return(other_web_contents.get()));

  base::test::TestFuture<bool> future;
  service().FillOtp(other_tab.GetHandle(), {test::MakeFieldGlobalId()}, "123456",
                    future.GetCallback());

  EXPECT_FALSE(future.Get());
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceFillOtpHistogram,
      ActorOneTimeTokenFillingServiceFillOtp::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceFillOtpHistogram,
      ActorOneTimeTokenFillingServiceFillOtp::kNoAutofillManager, 1);
}

// Tests that `FillOtp` correctly triggers the filling operation in the
// `AutofillManager` for a given OTP value and trigger field.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, FillOtp_Success) {
  FormData form = SeeForm({.fields = {{.server_type = ONE_TIME_CODE}}});
  FieldGlobalId field_id = form.fields()[0].global_id();
  const std::string kOtp = "123456";

  base::test::TestFuture<bool> future;
  service().FillOtp(tab().GetHandle(), {field_id}, kOtp, future.GetCallback());

  // Wait for the asynchronous filling operation to complete and verify success.
  EXPECT_TRUE(future.Get());

  // Verify that the manager was instructed to fill the correct value into the
  // field.
  EXPECT_THAT(last_filled_values(),
              testing::Contains(testing::Pair(field_id, u"123456")));
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceFillOtpHistogram,
      ActorOneTimeTokenFillingServiceFillOtp::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceFillOtpHistogram,
      ActorOneTimeTokenFillingServiceFillOtp::kSuccess, 1);
}

// Tests that concurrent calls to `FillOtp` are handled gracefully and the
// second call is ignored.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, FillOtp_ConcurrentCalls) {
  FormData form = SeeForm({.fields = {{.server_type = ONE_TIME_CODE}}});
  FieldGlobalId field_id = form.fields()[0].global_id();

  base::test::TestFuture<bool> future1;
  service().FillOtp(tab().GetHandle(), {field_id}, "123456",
                    future1.GetCallback());

  base::test::TestFuture<bool> future2;
  service().FillOtp(tab().GetHandle(), {field_id}, "654321",
                    future2.GetCallback());

  EXPECT_FALSE(future2.Get());
  EXPECT_TRUE(future1.Get());
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceFillOtpHistogram,
      ActorOneTimeTokenFillingServiceFillOtp::kStart, 2);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceFillOtpHistogram,
      ActorOneTimeTokenFillingServiceFillOtp::kConcurrentCall, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceFillOtpHistogram,
      ActorOneTimeTokenFillingServiceFillOtp::kSuccess, 1);
}

// Tests that `FillOtp` fails gracefully when the trigger field IDs list is
// empty.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, FillOtp_EmptyTriggerFields) {
  base::test::TestFuture<bool> future;
  service().FillOtp(tab().GetHandle(), {}, "123456", future.GetCallback());

  EXPECT_FALSE(future.Get());
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceFillOtpHistogram,
      ActorOneTimeTokenFillingServiceFillOtp::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceFillOtpHistogram,
      ActorOneTimeTokenFillingServiceFillOtp::kEmptyTriggerFieldIds, 1);
}

// Tests that `FillOtp` fails gracefully when the trigger field is not found in
// the autofill manager's cache.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, FillOtp_FieldNotInCache) {
  base::test::TestFuture<bool> future;
  service().FillOtp(tab().GetHandle(), {test::MakeFieldGlobalId()}, "123456",
                    future.GetCallback());

  EXPECT_FALSE(future.Get());
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceFillOtpHistogram,
      ActorOneTimeTokenFillingServiceFillOtp::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceFillOtpHistogram,
      ActorOneTimeTokenFillingServiceFillOtp::kFormStructureNotFound, 1);
}

// Tests that `FillOtp` succeeds by falling back to filling the trigger field
// even when the form does not contain any classified OTP fields.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, FillOtp_NoOtpFieldsInForm) {
  FormData form = SeeForm({.fields = {{.server_type = NAME_FIRST}}});
  FieldGlobalId field_id = form.fields()[0].global_id();

  base::test::TestFuture<bool> future;
  service().FillOtp(tab().GetHandle(), {field_id}, "123456",
                    future.GetCallback());

  EXPECT_TRUE(future.Get());
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceFillOtpHistogram,
      ActorOneTimeTokenFillingServiceFillOtp::kStart, 1);
  histogram_tester_.ExpectBucketCount(
      kActorOneTimeTokenFillingServiceFillOtpHistogram,
      ActorOneTimeTokenFillingServiceFillOtp::kSuccess, 1);
}

TEST_F(ActorOneTimeTokenFillingServiceImplTest, OnPasswordFillingStarted) {
  url::Origin test_origin = url::Origin::Create(GURL("https://example.com"));
  std::vector<int> target_global_frame_ids = {123, 456};

  service().OnPasswordFillingStarted(tab().GetHandle(), test_origin,
                                     /*should_use_strong_matching=*/true,
                                     target_global_frame_ids);

  std::optional<ActorLoginContext> context = service().ConsumeLoginContext();
  ASSERT_TRUE(context.has_value());
  EXPECT_EQ(context->origin, test_origin);
  EXPECT_TRUE(context->should_use_strong_matching);
  EXPECT_EQ(context->navigations_per_frame.size(), 3u);
}

TEST_F(ActorOneTimeTokenFillingServiceImplTest, ConsumeLoginContext) {
  url::Origin test_origin = url::Origin::Create(GURL("https://example.com"));
  service().OnPasswordFillingStarted(tab().GetHandle(), test_origin,
                                     /*should_use_strong_matching=*/true,
                                     /*global_frame_ids=*/{123});

  EXPECT_TRUE(service().ConsumeLoginContext().has_value());
  EXPECT_FALSE(service().ConsumeLoginContext().has_value());
}

TEST_F(ActorOneTimeTokenFillingServiceImplTest, AbortLoginTracking) {
  url::Origin test_origin = url::Origin::Create(GURL("https://example.com"));
  service().OnPasswordFillingStarted(tab().GetHandle(), test_origin,
                                     /*should_use_strong_matching=*/false,
                                     /*global_frame_ids=*/{});
  service().AbortLoginTracking();
  EXPECT_FALSE(service().ConsumeLoginContext().has_value());
}

TEST_F(ActorOneTimeTokenFillingServiceImplTest,
       ValidateFormFillingContext_NoSecurityStateHelper) {
  FormData form = SeeForm({.fields = {{.server_type = ONE_TIME_CODE}}});
  FieldGlobalId field_id = form.fields()[0].global_id();

  EXPECT_EQ(service().ValidateFormFillingContext(tab().GetHandle(), {field_id}),
            FormFillingContextStatus::kInsecureContext);
}

TEST_F(ActorOneTimeTokenFillingServiceImplTest,
       ValidateFormFillingContext_Success) {
  NavigateAndCommit(GURL("https://example.com"));
  client().set_last_committed_primary_main_frame_url(
      GURL("https://example.com"));
  SecurityStateTabHelper::CreateForWebContents(tab().GetContents());

  content::NavigationEntry* entry =
      tab().GetContents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  content::SSLStatus& ssl = entry->GetSSL();
  ssl.initialized = true;
  ssl.certificate =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl.cert_status = net::OK;
  // Set a valid TLS connection version (`TLS 1.2`) in `connection_status` using
  // `SSLConnectionStatusSetVersion` so `VisibleSecurityState` sets
  // `connection_info_initialized` to true.
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &ssl.connection_status);

  FormData form = SeeForm({.fields = {{.server_type = ONE_TIME_CODE}}});
  FieldGlobalId field_id = form.fields()[0].global_id();
  EXPECT_EQ(service().ValidateFormFillingContext(tab().GetHandle(), {field_id}),
            FormFillingContextStatus::kSecure);
}

TEST_F(ActorOneTimeTokenFillingServiceImplTest,
       ValidateFormFillingContext_InsecureFormAction) {
  NavigateAndCommit(GURL("https://example.com"));
  client().set_last_committed_primary_main_frame_url(
      GURL("https://example.com"));
  SecurityStateTabHelper::CreateForWebContents(tab().GetContents());

  content::NavigationEntry* entry =
      tab().GetContents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  content::SSLStatus& ssl = entry->GetSSL();
  ssl.initialized = true;
  ssl.certificate =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl.cert_status = net::OK;
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &ssl.connection_status);

  FormData insecure_form =
      SeeForm({.fields = {{.server_type = ONE_TIME_CODE}},
               .action = "http://example.com/submit.html"});
  FieldGlobalId insecure_field_id = insecure_form.fields()[0].global_id();
  EXPECT_EQ(service().ValidateFormFillingContext(tab().GetHandle(),
                                                 {insecure_field_id}),
            FormFillingContextStatus::kInsecureContext);
}

TEST_F(ActorOneTimeTokenFillingServiceImplTest,
       ValidateFormFillingContext_MixedContentPage) {
  NavigateAndCommit(GURL("https://example.com"));
  client().set_last_committed_primary_main_frame_url(
      GURL("https://example.com"));
  SecurityStateTabHelper::CreateForWebContents(tab().GetContents());

  content::NavigationEntry* entry =
      tab().GetContents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  content::SSLStatus& ssl = entry->GetSSL();
  ssl.initialized = true;
  ssl.certificate =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl.cert_status = net::OK;
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &ssl.connection_status);

  FormData form = SeeForm({.fields = {{.server_type = ONE_TIME_CODE}}});
  FieldGlobalId field_id = form.fields()[0].global_id();

  // Simulating mixed content (`DISPLAYED_INSECURE_CONTENT`) in `content_status`
  // downgrades `SecurityLevel` and returns false.
  ssl.content_status |= content::SSLStatus::DISPLAYED_INSECURE_CONTENT;
  EXPECT_EQ(service().ValidateFormFillingContext(tab().GetHandle(), {field_id}),
            FormFillingContextStatus::kInsecureContext);
}

TEST_F(ActorOneTimeTokenFillingServiceImplTest,
       ValidateFormFillingContext_HttpPage) {
  NavigateAndCommit(GURL("http://example.com"));
  client().set_last_committed_primary_main_frame_url(
      GURL("http://example.com"));
  SecurityStateTabHelper::CreateForWebContents(tab().GetContents());

  FormData form = SeeForm({.fields = {{.server_type = ONE_TIME_CODE}}});
  FieldGlobalId field_id = form.fields()[0].global_id();

  EXPECT_EQ(service().ValidateFormFillingContext(tab().GetHandle(), {field_id}),
            FormFillingContextStatus::kInsecureContext);
}

TEST_F(ActorOneTimeTokenFillingServiceImplTest,
       ValidateFormFillingContext_InvalidCertificate) {
  NavigateAndCommit(GURL("https://example.com"));
  client().set_last_committed_primary_main_frame_url(
      GURL("https://example.com"));
  SecurityStateTabHelper::CreateForWebContents(tab().GetContents());

  content::NavigationEntry* entry =
      tab().GetContents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  content::SSLStatus& ssl = entry->GetSSL();
  ssl.initialized = true;
  ssl.certificate =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "expired_cert.pem");
  // Setting `CERT_STATUS_DATE_INVALID` in `cert_status` downgrades
  // `SecurityLevel` to `DANGEROUS`.
  ssl.cert_status = net::CERT_STATUS_DATE_INVALID;
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &ssl.connection_status);

  FormData form = SeeForm({.fields = {{.server_type = ONE_TIME_CODE}}});
  FieldGlobalId field_id = form.fields()[0].global_id();

  EXPECT_EQ(service().ValidateFormFillingContext(tab().GetHandle(), {field_id}),
            FormFillingContextStatus::kInsecureContext);
}

TEST_F(ActorOneTimeTokenFillingServiceImplTest,
       ValidateFormFillingContext_FormNotFound) {
  NavigateAndCommit(GURL("https://example.com"));
  client().set_last_committed_primary_main_frame_url(
      GURL("https://example.com"));
  SecurityStateTabHelper::CreateForWebContents(tab().GetContents());

  content::NavigationEntry* entry =
      tab().GetContents()->GetController().GetVisibleEntry();
  ASSERT_TRUE(entry);
  content::SSLStatus& ssl = entry->GetSSL();
  ssl.initialized = true;
  ssl.certificate =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ssl.cert_status = net::OK;
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &ssl.connection_status);

  FormData form = SeeForm({.fields = {{.server_type = ONE_TIME_CODE}}});
  FieldGlobalId field_id = form.fields()[0].global_id();

  // Simulate dynamic removal of the form from the DOM by notifying
  // `BrowserAutofillManager` via `OnFormsSeen` that the form was removed.
  manager().OnFormsSeen(/*updated_forms=*/{},
                        /*removed_forms=*/{form.global_id()});

  EXPECT_EQ(service().ValidateFormFillingContext(tab().GetHandle(), {field_id}),
            FormFillingContextStatus::kFormNotFound);
}

TEST_F(ActorOneTimeTokenFillingServiceImplTest,
       ValidateFormFillingContext_InvalidTabHandle) {
  EXPECT_EQ(service().ValidateFormFillingContext(tabs::TabHandle::Null(),
                                                 {test::MakeFieldGlobalId()}),
            FormFillingContextStatus::kTabNotAvailable);
}

}  // namespace

}  // namespace autofill
