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
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/autofill/actor/actor_test_utils.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_login_context.h"
#include "chrome/browser/autofill/one_time_token_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_suggestion.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"
#include "components/one_time_tokens/core/common/one_time_token_features.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
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

// Tests that `RetrieveOtp` returns the mock OTP immediately from the feature
// parameter when the parameter is set.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, RetrieveOtp_MockOtpFeatureSet) {
  const std::string kMockOtp = "987654";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      one_time_tokens::features::kGmailOtpRetrievalService,
      {{"mock-gmail-otp-value", kMockOtp}});

  // The OTP service is not expected to be queried for cached tokens or
  // subscription since RetrieveOtp returns early.
  EXPECT_CALL(otp_service(), GetCachedOneTimeTokens).Times(0);
  EXPECT_CALL(otp_service(), Subscribe).Times(0);

  base::test::TestFuture<
      base::expected<std::string, OneTimeTokenRetrievalError>>
      future;
  service().RetrieveOtp(tab().GetHandle(), {}, future.GetCallback());
  EXPECT_EQ(future.Get().value(), kMockOtp);
}

// Tests that `RetrieveOtp` correctly returns an available OTP from the
// underlying service.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, RetrieveOtp_Success) {
  const std::string kOtp = "123456";
  EXPECT_CALL(otp_service(), GetCachedOneTimeTokens())
      .WillOnce(Return(std::vector<one_time_tokens::OneTimeToken>{
          {one_time_tokens::OneTimeTokenType::kGmail, kOtp,
           base::TimeTicks::Now()}}));

  base::test::TestFuture<
      base::expected<std::string, OneTimeTokenRetrievalError>>
      future;
  service().RetrieveOtp(tab().GetHandle(), {}, future.GetCallback());
  EXPECT_EQ(future.Get().value(), kOtp);
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

  base::test::TestFuture<
      base::expected<std::string, OneTimeTokenRetrievalError>>
      future;
  service().RetrieveOtp(tab().GetHandle(), {}, future.GetCallback());
  EXPECT_EQ(future.Get().value(), kRecentGmailOtp);
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
}

// Tests that `RetrieveOtp` fails gracefully when the tab is null.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, RetrieveOtp_TabNull) {
  base::test::TestFuture<
      base::expected<std::string, OneTimeTokenRetrievalError>>
      future;
  service().RetrieveOtp(tabs::TabHandle(), {}, future.GetCallback());
  EXPECT_EQ(future.Get().error(), OneTimeTokenRetrievalError::kGmailOtpUnknown);
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
}

// Tests that `FillOtp` fails gracefully when the tab is null.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, FillOtp_TabNull) {
  base::test::TestFuture<bool> future;
  service().FillOtp(tabs::TabHandle(), {test::MakeFieldGlobalId()}, "123456",
                    future.GetCallback());
  EXPECT_FALSE(future.Get());
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
}

// Tests that `FillOtp` fails gracefully when the trigger field IDs list is
// empty.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, FillOtp_EmptyTriggerFields) {
  base::test::TestFuture<bool> future;
  service().FillOtp(tab().GetHandle(), {}, "123456", future.GetCallback());

  EXPECT_FALSE(future.Get());
}

// Tests that `FillOtp` fails gracefully when the trigger field is not found in
// the autofill manager's cache.
TEST_F(ActorOneTimeTokenFillingServiceImplTest, FillOtp_FieldNotInCache) {
  base::test::TestFuture<bool> future;
  service().FillOtp(tab().GetHandle(), {test::MakeFieldGlobalId()}, "123456",
                    future.GetCallback());

  EXPECT_FALSE(future.Get());
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
