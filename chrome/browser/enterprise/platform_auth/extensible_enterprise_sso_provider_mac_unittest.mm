// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_provider_mac.h"

#import <AuthenticationServices/AuthenticationServices.h>
#import <Foundation/Foundation.h>

#include <atomic>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_waitable_event.h"
#include "base/time/time.h"
#import "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_entra.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_features.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#include "content/public/test/browser_task_environment.h"
#import "net/base/apple/url_conversions.h"
#include "net/http/http_request_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "url/gurl.h"

// Test subclass of `SSOServiceEntraAuthControllerDelegate` that completes
// synchronously on the main thread without creating a real
// `ASAuthorizationController`.
@interface ExtensibleEnterpriseSSOTestEntraDelegate
    : SSOServiceEntraAuthControllerDelegate
@property(nonatomic, strong) ASAuthorization* authorization;
@property(nonatomic, strong) NSError* error;
@property(nonatomic, assign, readonly) BOOL performedRequest;
@end

@implementation ExtensibleEnterpriseSSOTestEntraDelegate

@synthesize authorization = _authorization;
@synthesize error = _error;
@synthesize performedRequest = _performedRequest;

- (void)performRequest {
  _performedRequest = YES;
  EXPECT_TRUE(NSThread.isMainThread);
  ASAuthorizationController* no_controller = nil;
  if (self.authorization) {
    [self authorizationController:no_controller
        didCompleteWithAuthorization:self.authorization];
  } else {
    NSError* error = self.error;
    ASSERT_TRUE(error);
    [self authorizationController:no_controller didCompleteWithError:error];
  }
}

@end

namespace enterprise_auth {

namespace {

using FailureReason = ExtensibleEnterpriseSSOProvider::FailureReason;

constexpr char kExampleUrl[] = "https://www.example.com/";
constexpr char kNotSupportedDurationHistogram[] =
    "Enterprise.ExtensibleEnterpriseSSO.NotSupported.Duration";
constexpr char kSupportedResultHistogram[] =
    "Enterprise.ExtensibleEnterpriseSSO.Supported.Result";
constexpr char kSupportedSuccessDurationHistogram[] =
    "Enterprise.ExtensibleEnterpriseSSO.Supported.Success.Duration";
constexpr char kSupportedFailureDurationHistogram[] =
    "Enterprise.ExtensibleEnterpriseSSO.Supported.Failure.Duration";
constexpr char kFailureReasonHistogram[] =
    "Enterprise.ExtensibleEnterpriseSSO.Failure.Reason";

class ExtensibleEnterpriseSSOProviderMacTest : public testing::Test {
 protected:
  using FeatureRefs = std::vector<base::test::FeatureRef>;

  explicit ExtensibleEnterpriseSSOProviderMacTest(
      bool async_check = true,
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::SYSTEM_TIME)
      : feature_list_(
            async_check
                ? FeatureRefs{kExtensibleEnterpriseSSOAsyncCapabilityCheck}
                : FeatureRefs{},
            async_check
                ? FeatureRefs{}
                : FeatureRefs{kExtensibleEnterpriseSSOAsyncCapabilityCheck}),
        task_environment_(time_source) {}

  void SetUp() override {
    mock_provider_class_ =
        ClassMock([ASAuthorizationSingleSignOnProvider class]);
    OCMStub([mock_provider_class_
                authorizationProviderWithIdentityProviderURL:[OCMArg any]])
        .andDo(^(NSInvocation* invocation) {
          ++provider_creation_count_;
          id provider = mock_provider_class_;
          [invocation setReturnValue:&provider];
        });
  }

  void TearDown() override {
    allow_check_.Signal();
    base::ThreadPoolInstance::Get()->FlushForTesting();
    EXPECT_GT(class_mocks_.count, 0u);
    for (id mock in class_mocks_) {
      [mock stopMocking];
    }
  }

  id ClassMock(Class cls) {
    id mock = OCMClassMock(cls);
    [class_mocks_ addObject:mock];
    return mock;
  }

  void StubCanPerformAuthorization(BOOL result,
                                   bool fail_on_main_thread,
                                   bool wait_for_allow_check) {
    OCMStub([mock_provider_class_ canPerformAuthorization])
        .andDo(^(NSInvocation* invocation) {
          BOOL value = result;
          ++check_count_;
          // Track how many invocations are active and the maximum seen so
          // overlapping checks on separate workers would be detected.
          const int active = ++active_checks_;
          int seen_max = max_active_checks_.load();
          while (active > seen_max &&
                 !max_active_checks_.compare_exchange_weak(seen_max, active)) {
          }
          if (NSThread.isMainThread) {
            if (fail_on_main_thread) {
              ADD_FAILURE()
                  << "canPerformAuthorization read on the main thread";
              value = NO;
            }
          } else if (wait_for_allow_check) {
            check_started_.Signal();
            allow_check_.Wait();
          }
          --active_checks_;
          [invocation setReturnValue:&value];
        });
  }

  base::test::ScopedFeatureList feature_list_;
  base::TestWaitableEvent allow_check_;
  // Signaled when a capability check starts waiting for `allow_check_`.
  base::TestWaitableEvent check_started_;
  std::atomic<int> provider_creation_count_{0};
  std::atomic<int> check_count_{0};
  std::atomic<int> active_checks_{0};
  std::atomic<int> max_active_checks_{0};
  id mock_provider_class_ = nil;
  content::BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  NSMutableArray* class_mocks_ = [NSMutableArray array];
};

class ExtensibleEnterpriseSSOProviderMacSyncCheckTest
    : public ExtensibleEnterpriseSSOProviderMacTest {
 protected:
  ExtensibleEnterpriseSSOProviderMacSyncCheckTest()
      : ExtensibleEnterpriseSSOProviderMacTest(/*async_check=*/false) {}
};

// Uses mock time so capability check timeouts fire deterministically. Mock
// time doesn't advance on its own while a capability check is blocked on a
// worker, so tests advance it explicitly.
class ExtensibleEnterpriseSSOProviderMacTimeoutTest
    : public ExtensibleEnterpriseSSOProviderMacTest {
 protected:
  ExtensibleEnterpriseSSOProviderMacTimeoutTest()
      : ExtensibleEnterpriseSSOProviderMacTest(
            /*async_check=*/true,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  // Advances mock time by `delta` and runs the main thread tasks due by then,
  // without waiting for ThreadPool tasks (which may be blocked). Delayed tasks
  // run in order of their run time, and the barrier posted here runs after
  // tasks due at the same time that were posted earlier.
  void AdvanceClockAndRunDueTasks(base::TimeDelta delta) {
    base::test::TestFuture<void> barrier_future;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, barrier_future.GetCallback(), delta);
    task_environment_.AdvanceClock(delta);
    EXPECT_TRUE(barrier_future.Wait());
  }

  // Waits until replies posted by finished capability checks have run. Must
  // only be called once no capability check is blocked.
  void FlushCapabilityCheckReplies() {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::test::TestFuture<void> reply_barrier_future;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, reply_barrier_future.GetCallback());
    EXPECT_TRUE(reply_barrier_future.Wait());
  }
};

TEST_F(ExtensibleEnterpriseSSOProviderMacTest,
       UnsupportedUrlIsCheckedOffTheMainThread) {
  StubCanPerformAuthorization(/*result=*/NO,
                              /*fail_on_main_thread=*/true,
                              /*wait_for_allow_check=*/true);

  ExtensibleEnterpriseSSOProvider provider;
  base::test::TestFuture<net::HttpRequestHeaders> headers_future;
  provider.GetData(GURL(kExampleUrl), headers_future.GetCallback());

  EXPECT_FALSE(headers_future.IsReady());

  // Verify the main sequence keeps running tasks while the capability check is
  // blocked on the background worker.
  base::test::TestFuture<void> main_thread_task_future;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, main_thread_task_future.GetCallback());
  EXPECT_TRUE(main_thread_task_future.Wait());
  EXPECT_FALSE(headers_future.IsReady());

  allow_check_.Signal();

  EXPECT_TRUE(headers_future.Get().IsEmpty());
  EXPECT_EQ(check_count_.load(), 1);
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonHistogram, FailureReason::kOsCannotPerformAuthorization, 1);
  histogram_tester_.ExpectTotalCount(kNotSupportedDurationHistogram, 1);
  histogram_tester_.ExpectTotalCount(kSupportedResultHistogram, 0);
  histogram_tester_.ExpectTotalCount(kSupportedSuccessDurationHistogram, 0);
  histogram_tester_.ExpectTotalCount(kSupportedFailureDurationHistogram, 0);
}

TEST_F(ExtensibleEnterpriseSSOProviderMacTest,
       SupportedUrlFailureRecordsMetricsOnMainThread) {
  StubCanPerformAuthorization(/*result=*/YES,
                              /*fail_on_main_thread=*/true,
                              /*wait_for_allow_check=*/false);

  // `TestingBrowserProcess`'s local state registers
  // `prefs::kExtensibleEnterpriseSSOEnabledIdps` with all supported IdPs,
  // including `"microsoft"`, by default.
  ExtensibleEnterpriseSSOTestEntraDelegate* delegate =
      [[ExtensibleEnterpriseSSOTestEntraDelegate alloc]
          initWithAuthorizationSingleSignOnProvider:mock_provider_class_];
  delegate.error = [NSError errorWithDomain:@"domain" code:0 userInfo:@{}];

  // Make the provider's `[[SSOServiceEntraAuthControllerDelegate alloc]
  // initWithAuthorizationSingleSignOnProvider:]` return `delegate`.
  id mock_delegate_class =
      ClassMock([SSOServiceEntraAuthControllerDelegate class]);
  OCMStub([mock_delegate_class alloc]).andReturn(mock_delegate_class);
  OCMStub([mock_delegate_class
              initWithAuthorizationSingleSignOnProvider:[OCMArg any]])
      .andReturn(delegate);

  ExtensibleEnterpriseSSOProvider provider;
  base::test::TestFuture<net::HttpRequestHeaders> headers_future;
  provider.GetData(GURL(kExampleUrl), headers_future.GetCallback());

  EXPECT_TRUE(headers_future.Get().IsEmpty());
  EXPECT_TRUE(delegate.performedRequest);
  EXPECT_EQ(check_count_.load(), 1);
  // The main thread reuses the provider created by the capability check.
  EXPECT_EQ(provider_creation_count_.load(), 1);
  histogram_tester_.ExpectTotalCount(kFailureReasonHistogram, 0);
  histogram_tester_.ExpectTotalCount(kNotSupportedDurationHistogram, 0);
  histogram_tester_.ExpectTotalCount(kSupportedSuccessDurationHistogram, 0);
  histogram_tester_.ExpectTotalCount(kSupportedFailureDurationHistogram, 1);
  histogram_tester_.ExpectUniqueSample(kSupportedResultHistogram, false, 1);
}

TEST_F(ExtensibleEnterpriseSSOProviderMacTest,
       SupportedUrlSuccessReturnsHeaders) {
  static constexpr NSString* kSSOCookies = @"sso_cookies";
  static constexpr NSString* kExampleResponse =
      @"{\"sso_cookies\":\"JSON formated "
      @"object\",\"operation\":\"get_sso_cookies\",\"broker_version\":\"3.3."
      @"23\",\"preferred_auth_config\":\"preferredAuthNotConfigured\","
      @"\"success\":\"1\",\"wpj_status\":\"notJoined\","
      @"\"operation_response_type\":\"operation_get_sso_cookies_response\","
      @"\"request_received_timestamp\":\"1736954944.2245578766\","
      @"\"extraDeviceInfo\":\"{}\",\"sso_extension_mode\":\"full\","
      @"\"device_mode\":\"personal\",\"response_gen_timestamp\":"
      @"\"1736954944.7778768539\",\"platform_sso_status\":"
      @"\"platformSSONotEnabled\"}";
  static constexpr char kExampleResponseHeaders[] =
      "{\"prt_headers\":[{\"header\":{\"x-ms-RefreshTokenCredential\":"
      "\"refreshcredentialsvalue\"},\"home_account_id\":\"homeaccountid\"},{"
      "\"header\":{\"x-ms-RefreshTokenCredential1\":"
      "\"refreshcredentialsvalue1\"},\"home_account_id\":\"homeaccountid1\"}],"
      "\"device_headers\":[{\"header\":{\"x-ms-DeviceCredential\":"
      "\"devicecredentials\"},\"tenant_id\":\"tenantidvalue\"}]}";
  static constexpr char kRefreshTokenCredentialName[] =
      "x-ms-RefreshTokenCredential";
  static constexpr char kRefreshTokenCredentialName1[] =
      "x-ms-RefreshTokenCredential1";
  static constexpr char kDeviceCredentialName[] = "x-ms-DeviceCredential";
  static constexpr char kRefreshTokenCredentialValue[] =
      "refreshcredentialsvalue";
  static constexpr char kRefreshTokenCredentialValue1[] =
      "refreshcredentialsvalue1";
  static constexpr char kDeviceCredentialValue[] = "devicecredentials";

  StubCanPerformAuthorization(/*result=*/YES,
                              /*fail_on_main_thread=*/true,
                              /*wait_for_allow_check=*/false);

  NSMutableDictionary* header_fields = [NSJSONSerialization
      JSONObjectWithData:[kExampleResponse
                             dataUsingEncoding:NSUTF8StringEncoding]
                 options:NSJSONReadingMutableContainers
                   error:nil];
  header_fields[kSSOCookies] = base::SysUTF8ToNSString(kExampleResponseHeaders);
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://www.example.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:header_fields];

  id mock_credential = ClassMock([ASAuthorizationSingleSignOnCredential class]);
  OCMStub([mock_credential authenticatedResponse]).andReturn(response);

  id mock_authorization = ClassMock([ASAuthorization class]);
  OCMStub([mock_authorization credential]).andReturn(mock_credential);

  ExtensibleEnterpriseSSOTestEntraDelegate* delegate =
      [[ExtensibleEnterpriseSSOTestEntraDelegate alloc]
          initWithAuthorizationSingleSignOnProvider:mock_provider_class_];
  delegate.authorization = mock_authorization;

  // Make the provider's `[[SSOServiceEntraAuthControllerDelegate alloc]
  // initWithAuthorizationSingleSignOnProvider:]` return `delegate`.
  id mock_delegate_class =
      ClassMock([SSOServiceEntraAuthControllerDelegate class]);
  OCMStub([mock_delegate_class alloc]).andReturn(mock_delegate_class);
  OCMStub([mock_delegate_class
              initWithAuthorizationSingleSignOnProvider:[OCMArg any]])
      .andReturn(delegate);

  ExtensibleEnterpriseSSOProvider provider;
  base::test::TestFuture<net::HttpRequestHeaders> headers_future;
  provider.GetData(GURL(kExampleUrl), headers_future.GetCallback());

  const net::HttpRequestHeaders& response_headers = headers_future.Get();
  EXPECT_EQ(response_headers.GetHeader(kRefreshTokenCredentialName),
            std::string(kRefreshTokenCredentialValue));
  EXPECT_EQ(response_headers.GetHeader(kRefreshTokenCredentialName1),
            std::string(kRefreshTokenCredentialValue1));
  EXPECT_EQ(response_headers.GetHeader(kDeviceCredentialName),
            std::string(kDeviceCredentialValue));
  EXPECT_EQ(check_count_.load(), 1);
  EXPECT_EQ(provider_creation_count_.load(), 1);
  histogram_tester_.ExpectTotalCount(kFailureReasonHistogram, 0);
  histogram_tester_.ExpectTotalCount(kNotSupportedDurationHistogram, 0);
  histogram_tester_.ExpectTotalCount(kSupportedSuccessDurationHistogram, 1);
  histogram_tester_.ExpectTotalCount(kSupportedFailureDurationHistogram, 0);
  histogram_tester_.ExpectUniqueSample(kSupportedResultHistogram, true, 1);
}

TEST_F(ExtensibleEnterpriseSSOProviderMacTest,
       NonHttpUrlCompletesSynchronouslyWithoutCheck) {
  StubCanPerformAuthorization(/*result=*/YES,
                              /*fail_on_main_thread=*/true,
                              /*wait_for_allow_check=*/true);

  ExtensibleEnterpriseSSOProvider provider;
  const GURL non_http_urls[] = {
      GURL("chrome://settings"),
      GURL("file:///tmp/example.html"),
      GURL("data:text/plain,hello"),
      GURL("about:blank"),
  };

  int expected_samples = 0;
  for (const GURL& url : non_http_urls) {
    base::test::TestFuture<net::HttpRequestHeaders> headers_future;
    provider.GetData(url, headers_future.GetCallback());
    ASSERT_TRUE(headers_future.IsReady()) << url;
    EXPECT_TRUE(headers_future.Get().IsEmpty()) << url;
    ++expected_samples;
    histogram_tester_.ExpectTotalCount(kNotSupportedDurationHistogram,
                                       expected_samples);
  }

  EXPECT_EQ(check_count_.load(), 0);
  histogram_tester_.ExpectTotalCount(kFailureReasonHistogram, 0);
  histogram_tester_.ExpectTotalCount(kSupportedResultHistogram, 0);
}

TEST_F(ExtensibleEnterpriseSSOProviderMacSyncCheckTest,
       KillSwitchDisabledChecksSynchronously) {
  StubCanPerformAuthorization(/*result=*/NO,
                              /*fail_on_main_thread=*/false,
                              /*wait_for_allow_check=*/false);

  ExtensibleEnterpriseSSOProvider provider;
  base::test::TestFuture<net::HttpRequestHeaders> headers_future;
  provider.GetData(GURL(kExampleUrl), headers_future.GetCallback());

  EXPECT_TRUE(headers_future.IsReady());
  EXPECT_TRUE(headers_future.Get().IsEmpty());
  EXPECT_EQ(check_count_.load(), 1);
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonHistogram, FailureReason::kOsCannotPerformAuthorization, 1);
  histogram_tester_.ExpectTotalCount(kNotSupportedDurationHistogram, 1);
}

TEST_F(ExtensibleEnterpriseSSOProviderMacTest, ChecksRunOneAtATime) {
  StubCanPerformAuthorization(/*result=*/NO,
                              /*fail_on_main_thread=*/true,
                              /*wait_for_allow_check=*/true);

  ExtensibleEnterpriseSSOProvider provider;
  base::test::TestFuture<net::HttpRequestHeaders> first_future;
  base::test::TestFuture<net::HttpRequestHeaders> second_future;
  provider.GetData(GURL("https://first.example.com/"),
                   first_future.GetCallback());
  provider.GetData(GURL("https://second.example.com/"),
                   second_future.GetCallback());

  EXPECT_FALSE(first_future.IsReady());
  EXPECT_FALSE(second_future.IsReady());

  allow_check_.Signal();

  EXPECT_TRUE(first_future.Get().IsEmpty());
  EXPECT_TRUE(second_future.Get().IsEmpty());
  EXPECT_EQ(check_count_.load(), 2);
  EXPECT_EQ(max_active_checks_.load(), 1);
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonHistogram, FailureReason::kOsCannotPerformAuthorization, 2);
}

TEST_F(ExtensibleEnterpriseSSOProviderMacTest,
       NilNativeUrlCompletesWithoutCreatingProvider) {
  StubCanPerformAuthorization(/*result=*/YES,
                              /*fail_on_main_thread=*/true,
                              /*wait_for_allow_check=*/false);

  // Valid HTTPS `GURL` for which `net::NSURLWithGURL()` returns `nil`.
  const GURL invalid_nsurl("https://{}");
  ASSERT_TRUE(invalid_nsurl.is_valid());
  ASSERT_TRUE(invalid_nsurl.SchemeIsHTTPOrHTTPS());
  ASSERT_FALSE(net::NSURLWithGURL(invalid_nsurl));

  ExtensibleEnterpriseSSOProvider provider;
  base::test::TestFuture<net::HttpRequestHeaders> headers_future;
  provider.GetData(invalid_nsurl, headers_future.GetCallback());

  EXPECT_TRUE(headers_future.Get().IsEmpty());
  EXPECT_EQ(provider_creation_count_.load(), 0);
  EXPECT_EQ(check_count_.load(), 0);
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonHistogram, FailureReason::kOsCannotPerformAuthorization, 1);
  histogram_tester_.ExpectTotalCount(kNotSupportedDurationHistogram, 1);
  histogram_tester_.ExpectTotalCount(kSupportedResultHistogram, 0);
}

TEST_F(ExtensibleEnterpriseSSOProviderMacTimeoutTest,
       StalledCheckTimesOutWithEmptyHeaders) {
  StubCanPerformAuthorization(/*result=*/YES,
                              /*fail_on_main_thread=*/true,
                              /*wait_for_allow_check=*/true);
  const base::TimeDelta timeout =
      kExtensibleEnterpriseSSOAsyncCapabilityCheckTimeout.Get();

  ExtensibleEnterpriseSSOProvider provider;
  base::test::TestFuture<net::HttpRequestHeaders> headers_future;
  provider.GetData(GURL(kExampleUrl), headers_future.GetCallback());
  check_started_.Wait();

  AdvanceClockAndRunDueTasks(timeout - base::Milliseconds(1));
  EXPECT_FALSE(headers_future.IsReady());

  AdvanceClockAndRunDueTasks(base::Milliseconds(1));
  ASSERT_TRUE(headers_future.IsReady());
  EXPECT_TRUE(headers_future.Get().IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonHistogram, FailureReason::kCapabilityCheckTimedOut, 1);

  // A timed-out check records only `kFailureReasonHistogram`, and the late
  // reply must not run the callback again or record more metrics.
  allow_check_.Signal();
  FlushCapabilityCheckReplies();

  EXPECT_EQ(check_count_.load(), 1);
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonHistogram, FailureReason::kCapabilityCheckTimedOut, 1);
  histogram_tester_.ExpectTotalCount(kNotSupportedDurationHistogram, 0);
  histogram_tester_.ExpectTotalCount(kSupportedResultHistogram, 0);
  histogram_tester_.ExpectTotalCount(kSupportedSuccessDurationHistogram, 0);
  histogram_tester_.ExpectTotalCount(kSupportedFailureDurationHistogram, 0);
}

TEST_F(ExtensibleEnterpriseSSOProviderMacTimeoutTest,
       CheckFinishingBeforeTimeoutCompletesOnce) {
  StubCanPerformAuthorization(/*result=*/NO,
                              /*fail_on_main_thread=*/true,
                              /*wait_for_allow_check=*/true);

  ExtensibleEnterpriseSSOProvider provider;
  base::MockCallback<PlatformAuthProviderManager::GetDataCallback> callback;
  EXPECT_CALL(callback,
              Run(testing::Property(&net::HttpRequestHeaders::IsEmpty, true)));
  provider.GetData(GURL(kExampleUrl), callback.Get());

  allow_check_.Signal();
  FlushCapabilityCheckReplies();
  testing::Mock::VerifyAndClearExpectations(&callback);
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonHistogram, FailureReason::kOsCannotPerformAuthorization, 1);
  histogram_tester_.ExpectTotalCount(kNotSupportedDurationHistogram, 1);

  // The timeout must not run the callback again or record metrics.
  EXPECT_CALL(callback, Run(testing::_)).Times(0);
  const base::TimeDelta timeout =
      kExtensibleEnterpriseSSOAsyncCapabilityCheckTimeout.Get();
  task_environment_.FastForwardBy(timeout * 2);
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonHistogram, FailureReason::kOsCannotPerformAuthorization, 1);
  histogram_tester_.ExpectTotalCount(kNotSupportedDurationHistogram, 1);
}

TEST_F(ExtensibleEnterpriseSSOProviderMacTimeoutTest,
       ProviderDestroyedBeforeTimeoutDropsCallback) {
  StubCanPerformAuthorization(/*result=*/NO,
                              /*fail_on_main_thread=*/true,
                              /*wait_for_allow_check=*/true);

  base::MockCallback<PlatformAuthProviderManager::GetDataCallback> callback;
  EXPECT_CALL(callback, Run(testing::_)).Times(0);

  std::optional<ExtensibleEnterpriseSSOProvider> provider(std::in_place);
  provider->GetData(GURL(kExampleUrl), callback.Get());
  check_started_.Wait();
  provider.reset();

  // Neither the timeout nor the late reply may run the callback.
  const base::TimeDelta timeout =
      kExtensibleEnterpriseSSOAsyncCapabilityCheckTimeout.Get();
  AdvanceClockAndRunDueTasks(timeout * 2);
  allow_check_.Signal();
  FlushCapabilityCheckReplies();

  EXPECT_EQ(check_count_.load(), 1);
  histogram_tester_.ExpectTotalCount(kFailureReasonHistogram, 0);
  histogram_tester_.ExpectTotalCount(kNotSupportedDurationHistogram, 0);
}

}  // namespace

}  // namespace enterprise_auth
