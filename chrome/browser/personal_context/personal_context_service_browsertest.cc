// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_service.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/personal_context/personal_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/personal_context/core/context_memory_error.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/core/personal_context_types.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace personal_context {

namespace {

constexpr char kContextMemoryServiceHost[] =
    "contextmemoryservice.pa.googleapis.com";


class PersonalContextServiceImplBrowserTest : public InProcessBrowserTest {
 public:
  PersonalContextServiceImplBrowserTest() = default;
  ~PersonalContextServiceImplBrowserTest() override = default;

  // Configure feature before the main browser process is started
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kPersonalContext);
    InProcessBrowserTest::SetUp();
  }

  // Register the service factory callbacks prior to construction of services
  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&PersonalContextServiceImplBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(GetProfile());

    // Redirection rules on hosts resolver for mock server APIs
    host_resolver()->AddRule(kContextMemoryServiceHost, "127.0.0.1");

    // Installs the loader interceptor
    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            &PersonalContextServiceImplBrowserTest::OnInterceptRequest,
            base::Unretained(this)));

    personal_context_service_ =
        PersonalContextServiceFactory::GetForProfile(GetProfile());
  }

  void TearDownOnMainThread() override {
    personal_context_service_ = nullptr;
    url_loader_interceptor_.reset();
    identity_test_env_adaptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  bool OnInterceptRequest(
      content::URLLoaderInterceptor::RequestParams* params) {
    GURL request_url = params->url_request.url;
    if (request_url.host() != kContextMemoryServiceHost) {
      return false;
    }

    if (simulate_network_failure_) {
      params->client->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_CONNECTION_TIMED_OUT));
      return true;
    }

    std::string headers = base::StringPrintf(
        "HTTP/1.1 %d %s\n"
        "Content-Type: application/x-protobuf\n\n",
        static_cast<int>(http_status_), net::GetHttpReasonPhrase(http_status_));

    content::URLLoaderInterceptor::WriteResponse(headers, response_body_,
                                                 params->client.get());
    return true;
  }

  void SignIn(std::string_view email) {
    identity_test_env()->MakePrimaryAccountAvailable(
        std::string(email), signin::ConsentLevel::kSignin);
    identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  }

  FetchContextResult FetchContextAndWait() {
    base::test::TestFuture<FetchContextResult> future;
    proto::FetchPiiEntitiesRequest dummy_request;
    ContextMemoryRequestOptions options;
    personal_context_service_->FetchContext(
        proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, dummy_request, options,
        future.GetCallback());
    return future.Take();
  }

  void SetMockResponse(std::string_view value,
                       net::HttpStatusCode status = net::HTTP_OK) {
    proto::FetchContextResponse fetch_response;
    fetch_response.set_server_request_id("test_id");
    fetch_response.mutable_response_metadata()->set_value(value);
    SetMockResponse(fetch_response, status);
  }

  void SetMockResponse(const proto::FetchContextResponse& fetch_response,
                       net::HttpStatusCode status = net::HTTP_OK) {
    std::string response_string;
    fetch_response.SerializeToString(&response_string);
    SetRawMockResponse(response_string, status);
  }

  void SetRawMockResponse(std::string_view body,
                          net::HttpStatusCode status = net::HTTP_OK) {
    response_body_ = std::string(body);
    http_status_ = status;
    simulate_network_failure_ = false;
  }

  void SetNetworkFailure() { simulate_network_failure_ = true; }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  raw_ptr<PersonalContextService> personal_context_service_;
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;

  net::HttpStatusCode http_status_ = net::HTTP_OK;
  std::string response_body_;
  bool simulate_network_failure_ = false;
};

// =============================================================================
// AUTHENTICATION TEST CASES
// =============================================================================

// Verify calls block with kPermissionDenied when user is signed out
IN_PROC_BROWSER_TEST_F(PersonalContextServiceImplBrowserTest,
                       AuthSignedOutBlocksFetch) {
  FetchContextResult result = FetchContextAndWait();
  ASSERT_FALSE(result.response.has_value());
  EXPECT_EQ(result.response.error().error(),
            ContextMemoryError::ExecutionError::kPermissionDenied);
}

// Verify requests succeed when user profile is logged in with valid token
IN_PROC_BROWSER_TEST_F(PersonalContextServiceImplBrowserTest,
                       AuthSignedInFulfillFetch) {
  SignIn("test@gmail.com");
  SetMockResponse("test_output");

  FetchContextResult result = FetchContextAndWait();
  ASSERT_TRUE(result.response.has_value());
  EXPECT_EQ(result.response.value().value(), "test_output");
}

// Confirm authorization halts on credential error states
IN_PROC_BROWSER_TEST_F(PersonalContextServiceImplBrowserTest,
                       AuthPersistentAuthErrorBlocksFetch) {
  CoreAccountId account_id =
      identity_test_env()
          ->MakePrimaryAccountAvailable("error_user@gmail.com",
                                        signin::ConsentLevel::kSignin)
          .account_id;
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      account_id, GoogleServiceAuthError(
                      GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  base::test::TestFuture<FetchContextResult> future;
  proto::FetchPiiEntitiesRequest dummy_request;
  ContextMemoryRequestOptions options;

  personal_context_service_->FetchContext(
      proto::CONTEXT_MEMORY_FEATURE_AMBIENT_AUTOFILL, dummy_request, options,
      future.GetCallback());

  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  FetchContextResult result = future.Take();
  ASSERT_FALSE(result.response.has_value());
  EXPECT_EQ(result.response.error().error(),
            ContextMemoryError::ExecutionError::kPermissionDenied);
}

// =============================================================================
// NETWORK INTERCEPTOR & ERROR TEST CASES
// =============================================================================

// Verify context fetch successfully returns data on 200 OK
IN_PROC_BROWSER_TEST_F(PersonalContextServiceImplBrowserTest,
                       NetworkFetchContextSuccess) {
  SignIn("test@gmail.com");
  SetMockResponse("success_payload");

  FetchContextResult result = FetchContextAndWait();
  ASSERT_TRUE(result.response.has_value());
  EXPECT_EQ(result.response.value().value(), "success_payload");

  histogram_tester_.ExpectBucketCount(
      "PersonalContext.FetchContext.Result.AmbientAutofill", true, 1);
}

// Verify handling when network connection drops or times out
IN_PROC_BROWSER_TEST_F(PersonalContextServiceImplBrowserTest,
                       NetworkFetchContextConnectionTimeout) {
  SignIn("test@gmail.com");
  SetNetworkFailure();

  FetchContextResult result = FetchContextAndWait();
  ASSERT_FALSE(result.response.has_value());
  EXPECT_EQ(result.response.error().error(),
            ContextMemoryError::ExecutionError::kGenericFailure);
  EXPECT_TRUE(result.response.error().transient());
}

// Confirm HTTP 429 (Throttle) results in kRequestThrottled (transient = true)
IN_PROC_BROWSER_TEST_F(PersonalContextServiceImplBrowserTest,
                       NetworkFetchContextThrottled) {
  SignIn("test@gmail.com");
  SetRawMockResponse("", net::HTTP_TOO_MANY_REQUESTS);

  FetchContextResult result = FetchContextAndWait();
  ASSERT_FALSE(result.response.has_value());
  EXPECT_EQ(result.response.error().error(),
            ContextMemoryError::ExecutionError::kRequestThrottled);
  EXPECT_TRUE(result.response.error().transient());

  histogram_tester_.ExpectBucketCount(
      "PersonalContext.FetchContext.Result.AmbientAutofill", false, 1);
  histogram_tester_.ExpectBucketCount(
      "PersonalContext.FetchContext.ErrorStatus.AmbientAutofill",
      static_cast<int>(ContextMemoryError::ExecutionError::kRequestThrottled),
      1);
}

// Confirm HTTP 401 (Unauthorized) results in kPermissionDenied (transient =
// false)
IN_PROC_BROWSER_TEST_F(PersonalContextServiceImplBrowserTest,
                       NetworkFetchContextUnauthorized) {
  SignIn("test@gmail.com");
  SetRawMockResponse("", net::HTTP_UNAUTHORIZED);

  FetchContextResult result = FetchContextAndWait();
  ASSERT_FALSE(result.response.has_value());
  EXPECT_EQ(result.response.error().error(),
            ContextMemoryError::ExecutionError::kPermissionDenied);
  EXPECT_FALSE(result.response.error().transient());
}

// Confirm malformed response payload results in kGenericFailure (with transient
// = true)
IN_PROC_BROWSER_TEST_F(PersonalContextServiceImplBrowserTest,
                       NetworkFetchContextParseError) {
  SignIn("test@gmail.com");
  SetRawMockResponse("corrupted_non_protobuf_payload", net::HTTP_OK);

  FetchContextResult result = FetchContextAndWait();
  ASSERT_FALSE(result.response.has_value());
  EXPECT_EQ(result.response.error().error(),
            ContextMemoryError::ExecutionError::kGenericFailure);
  EXPECT_TRUE(result.response.error().transient());
}

// =============================================================================
// CONCURRENCY & LIFECYCLE TEST CASES
// =============================================================================

// Verify parallel limit cancellation terminates the oldest query without
// crashing containers
IN_PROC_BROWSER_TEST_F(PersonalContextServiceImplBrowserTest,
                       ConcurrencyParallelRequestCancellation) {
  SignIn("test@gmail.com");

  proto::FetchContextResponse fetch_response;
  fetch_response.set_server_request_id("req_2");
  fetch_response.mutable_response_metadata()->set_value("req_2_value");
  SetMockResponse(fetch_response, net::HTTP_OK);

  base::test::TestFuture<FetchContextResult> future1;
  base::test::TestFuture<FetchContextResult> future2;
  proto::FetchPiiEntitiesRequest dummy_request;
  ContextMemoryRequestOptions options;

  // Start Request 1
  personal_context_service_->FetchContext(
      proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY, dummy_request, options,
      future1.GetCallback());

  // Start Request 2 (should cancel Request 1 because parallel limit is 1)
  personal_context_service_->FetchContext(
      proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY, dummy_request, options,
      future2.GetCallback());

  FetchContextResult result1 = future1.Take();
  ASSERT_FALSE(result1.response.has_value());
  EXPECT_EQ(result1.response.error().error(),
            ContextMemoryError::ExecutionError::kCancelled);
  FetchContextResult result2 = future2.Take();
  EXPECT_TRUE(result2.response.has_value());
}
}  // namespace
}  // namespace personal_context
