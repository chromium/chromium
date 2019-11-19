// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/captive_portal/captive_portal_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/captive_portal/captive_portal_testing_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

using captive_portal::CaptivePortalDetectorTestBase;
using captive_portal::CaptivePortalResult;

namespace {

// An observer watches the CaptivePortalDetector.  It tracks the last
// received result and the total number of received results.
class CaptivePortalObserver : public content::NotificationObserver {
 public:
  CaptivePortalObserver(Profile* profile,
                        CaptivePortalService* captive_portal_service)
      : captive_portal_result_(
            captive_portal_service->last_detection_result()),
        num_results_received_(0),
        profile_(profile),
        captive_portal_service_(captive_portal_service) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT,
                   content::Source<Profile>(profile_));
  }

  CaptivePortalResult captive_portal_result() const {
    return captive_portal_result_;
  }

  int num_results_received() const { return num_results_received_; }

 private:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    ASSERT_EQ(type, chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT);
    ASSERT_EQ(profile_, content::Source<Profile>(source).ptr());

    CaptivePortalService::Results *results =
        content::Details<CaptivePortalService::Results>(details).ptr();

    EXPECT_EQ(captive_portal_result_, results->previous_result);
    EXPECT_EQ(captive_portal_service_->last_detection_result(),
              results->result);

    captive_portal_result_ = results->result;
    ++num_results_received_;
  }

  CaptivePortalResult captive_portal_result_;
  int num_results_received_;

  Profile* profile_;
  CaptivePortalService* captive_portal_service_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalObserver);
};

}  // namespace

class CaptivePortalServiceTest : public testing::Test,
                                 public CaptivePortalDetectorTestBase {
 public:
  CaptivePortalServiceTest()
      : old_captive_portal_testing_state_(
            CaptivePortalService::get_state_for_testing()) {
  }

  ~CaptivePortalServiceTest() override {
    CaptivePortalService::set_state_for_testing(
        old_captive_portal_testing_state_);
  }

  // |enable_service| is whether or not the captive portal service itself
  // should be disabled.  This is different from enabling the captive portal
  // detection preference.
  void Initialize(CaptivePortalService::TestingState testing_state) {
    CaptivePortalService::set_state_for_testing(testing_state);

    profile_.reset(new TestingProfile());
    tick_clock_.reset(new base::SimpleTestTickClock());
    tick_clock_->Advance(base::TimeTicks::Now() - tick_clock_->NowTicks());
    service_.reset(new CaptivePortalService(profile_.get(), tick_clock_.get(),
                                            test_loader_factory()));

    // Use no delays for most tests.
    set_initial_backoff_no_portal(base::TimeDelta());
    set_initial_backoff_portal(base::TimeDelta());

    set_detector(service_->captive_portal_detector_.get());
    SetTime(base::Time::Now());

    // Disable jitter, so can check exact values.
    set_jitter_factor(0.0);

    // These values make checking exponential backoff easier.
    set_multiply_factor(2.0);
    set_maximum_backoff(base::TimeDelta::FromSeconds(1600));

    // This means backoff starts after the second "failure", which is the third
    // captive portal test in a row that ends up with the same result.  Since
    // the first request uses no delay, this means the delays will be in
    // the pattern 0, 0, 100, 200, 400, etc.  There are two zeros because the
    // first check never has a delay, and the first check to have a new result
    // is followed by no delay.
    set_num_errors_to_ignore(1);

    EnableCaptivePortalDetectionPreference(true);
  }

  // Sets the captive portal checking preference.
  void EnableCaptivePortalDetectionPreference(bool enabled) {
    profile()->GetPrefs()->SetBoolean(prefs::kAlternateErrorPagesEnabled,
                                      enabled);
  }

  // Triggers a captive portal check, then simulates the URL request
  // returning with the specified |net_error| and |status_code|.  If |net_error|
  // is not OK, |status_code| is ignored.  Expects the CaptivePortalService to
  // return |expected_result|.
  //
  // |expected_delay_secs| is the expected value of GetTimeUntilNextRequest().
  // The function makes sure the value is as expected, and then simulates
  // waiting for that period of time before running the test.
  //
  // If |response_headers| is non-NULL, the response will use it as headers
  // for the simulate URL request.  It must use single linefeeds as line breaks.
  void RunTest(CaptivePortalResult expected_result,
               int net_error,
               int status_code,
               int expected_delay_secs,
               const char* response_headers) {
    base::TimeDelta expected_delay =
        base::TimeDelta::FromSeconds(expected_delay_secs);

    ASSERT_EQ(CaptivePortalService::STATE_IDLE, service()->state());
    ASSERT_EQ(expected_delay, GetTimeUntilNextRequest());

    AdvanceTime(expected_delay);
    ASSERT_EQ(base::TimeDelta(), GetTimeUntilNextRequest());

    CaptivePortalObserver observer(profile(), service());
    service()->DetectCaptivePortal();

    EXPECT_EQ(CaptivePortalService::STATE_TIMER_RUNNING, service()->state());
    EXPECT_FALSE(FetchingURL());
    ASSERT_TRUE(TimerRunning());

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(CaptivePortalService::STATE_CHECKING_FOR_PORTAL,
              service()->state());
    ASSERT_TRUE(FetchingURL());
    EXPECT_FALSE(TimerRunning());

    CompleteURLFetch(net_error, status_code, response_headers);

    EXPECT_FALSE(FetchingURL());
    EXPECT_FALSE(TimerRunning());
    EXPECT_EQ(1, observer.num_results_received());
    EXPECT_EQ(expected_result, observer.captive_portal_result());
  }

  // Runs a test when the captive portal service is disabled.
  void RunDisabledTest(int expected_delay_secs) {
    base::TimeDelta expected_delay =
        base::TimeDelta::FromSeconds(expected_delay_secs);

    ASSERT_EQ(CaptivePortalService::STATE_IDLE, service()->state());
    ASSERT_EQ(expected_delay, GetTimeUntilNextRequest());

    AdvanceTime(expected_delay);
    ASSERT_EQ(base::TimeDelta(), GetTimeUntilNextRequest());

    CaptivePortalObserver observer(profile(), service());
    service()->DetectCaptivePortal();

    EXPECT_EQ(CaptivePortalService::STATE_TIMER_RUNNING, service()->state());
    EXPECT_FALSE(FetchingURL());
    ASSERT_TRUE(TimerRunning());

    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(FetchingURL());
    EXPECT_FALSE(TimerRunning());
    EXPECT_EQ(1, observer.num_results_received());
    EXPECT_EQ(captive_portal::RESULT_INTERNET_CONNECTED,
              observer.captive_portal_result());
  }

  // Tests exponential backoff.  Prior to calling, the relevant recheck settings
  // must be set to have a minimum time of 100 seconds, with 2 checks before
  // starting exponential backoff.
  void RunBackoffTest(CaptivePortalResult expected_result,
                      int net_error,
                      int status_code) {
    RunTest(expected_result, net_error, status_code, 0, NULL);
    RunTest(expected_result, net_error, status_code, 0, NULL);
    RunTest(expected_result, net_error, status_code, 100, NULL);
    RunTest(expected_result, net_error, status_code, 200, NULL);
    RunTest(expected_result, net_error, status_code, 400, NULL);
    RunTest(expected_result, net_error, status_code, 800, NULL);
    RunTest(expected_result, net_error, status_code, 1600, NULL);
    RunTest(expected_result, net_error, status_code, 1600, NULL);
  }

  // Changes test time for the service and service's captive portal
  // detector.
  void AdvanceTime(const base::TimeDelta& delta) {
    tick_clock_->Advance(delta);
    CaptivePortalDetectorTestBase::AdvanceTime(delta);
  }

  bool TimerRunning() {
    return service()->TimerRunning();
  }

  base::TimeDelta GetTimeUntilNextRequest() {
    return service()->backoff_entry_->GetTimeUntilRelease();
  }

  void set_initial_backoff_no_portal(
      base::TimeDelta initial_backoff_no_portal) {
    service()->recheck_policy().initial_backoff_no_portal_ms =
        initial_backoff_no_portal.InMilliseconds();
  }

  void set_initial_backoff_portal(base::TimeDelta initial_backoff_portal) {
    service()->recheck_policy().initial_backoff_portal_ms =
        initial_backoff_portal.InMilliseconds();
  }

  void set_maximum_backoff(base::TimeDelta maximum_backoff) {
    service()->recheck_policy().backoff_policy.maximum_backoff_ms =
        maximum_backoff.InMilliseconds();
  }

  void set_num_errors_to_ignore(int num_errors_to_ignore) {
    service()->recheck_policy().backoff_policy.num_errors_to_ignore =
        num_errors_to_ignore;
  }

  void set_multiply_factor(double multiply_factor) {
    service()->recheck_policy().backoff_policy.multiply_factor =
        multiply_factor;
  }

  void set_jitter_factor(double jitter_factor) {
    service()->recheck_policy().backoff_policy.jitter_factor = jitter_factor;
  }

  TestingProfile* profile() { return profile_.get(); }

  CaptivePortalService* service() { return service_.get(); }

 private:
  // Stores the initial CaptivePortalService::TestingState so it can be restored
  // after the test.
  const CaptivePortalService::TestingState old_captive_portal_testing_state_;

  content::BrowserTaskEnvironment task_environment_;

  // Note that the construction order of these matters.
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<base::SimpleTestTickClock> tick_clock_;
  std::unique_ptr<CaptivePortalService> service_;
};

// Verify that an observer doesn't get messages from the wrong profile.
TEST_F(CaptivePortalServiceTest, CaptivePortalTwoProfiles) {
  Initialize(CaptivePortalService::SKIP_OS_CHECK_FOR_TESTING);
  TestingProfile profile2;
  std::unique_ptr<CaptivePortalService> service2(
      new CaptivePortalService(&profile2));
  CaptivePortalObserver observer2(&profile2, service2.get());

  RunTest(captive_portal::RESULT_INTERNET_CONNECTED, net::OK, 204, 0, NULL);
  EXPECT_EQ(0, observer2.num_results_received());
}

// Checks exponential backoff when the Internet is connected.
TEST_F(CaptivePortalServiceTest, CaptivePortalRecheckInternetConnected) {
  Initialize(CaptivePortalService::SKIP_OS_CHECK_FOR_TESTING);

  // This value should have no effect on this test, until the end.
  set_initial_backoff_portal(base::TimeDelta::FromSeconds(1));

  set_initial_backoff_no_portal(base::TimeDelta::FromSeconds(100));
  RunBackoffTest(captive_portal::RESULT_INTERNET_CONNECTED, net::OK, 204);

  // Make sure that getting a new result resets the timer.
  RunTest(
      captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL, net::OK, 200, 1600, NULL);
  RunTest(captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL, net::OK, 200, 0, NULL);
  RunTest(captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL, net::OK, 200, 1, NULL);
  RunTest(captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL, net::OK, 200, 2, NULL);
}

// Checks exponential backoff when there's an HTTP error.
TEST_F(CaptivePortalServiceTest, CaptivePortalRecheckError) {
  Initialize(CaptivePortalService::SKIP_OS_CHECK_FOR_TESTING);

  // This value should have no effect on this test.
  set_initial_backoff_portal(base::TimeDelta::FromDays(1));

  set_initial_backoff_no_portal(base::TimeDelta::FromSeconds(100));
  RunBackoffTest(captive_portal::RESULT_NO_RESPONSE, net::OK, 500);

  // Make sure that getting a new result resets the timer.
  RunTest(captive_portal::RESULT_INTERNET_CONNECTED, net::OK, 204, 1600, NULL);
  RunTest(captive_portal::RESULT_INTERNET_CONNECTED, net::OK, 204, 0, NULL);
  RunTest(captive_portal::RESULT_INTERNET_CONNECTED, net::OK, 204, 100, NULL);
}

// Checks exponential backoff when there's a captive portal.
TEST_F(CaptivePortalServiceTest, CaptivePortalRecheckBehindPortal) {
  Initialize(CaptivePortalService::SKIP_OS_CHECK_FOR_TESTING);

  // This value should have no effect on this test, until the end.
  set_initial_backoff_no_portal(base::TimeDelta::FromSeconds(250));

  set_initial_backoff_portal(base::TimeDelta::FromSeconds(100));
  RunBackoffTest(captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL, net::OK, 200);

  // Make sure that getting a new result resets the timer.
  RunTest(captive_portal::RESULT_INTERNET_CONNECTED, net::OK, 204, 1600, NULL);
  RunTest(captive_portal::RESULT_INTERNET_CONNECTED, net::OK, 204, 0, NULL);
  RunTest(captive_portal::RESULT_INTERNET_CONNECTED, net::OK, 204, 250, NULL);
}

// Check that everything works as expected when captive portal checking is
// disabled, including throttling.  Then enables it again and runs another test.
TEST_F(CaptivePortalServiceTest, CaptivePortalPrefDisabled) {
  Initialize(CaptivePortalService::SKIP_OS_CHECK_FOR_TESTING);

  // This value should have no effect on this test.
  set_initial_backoff_no_portal(base::TimeDelta::FromDays(1));

  set_initial_backoff_portal(base::TimeDelta::FromSeconds(100));

  EnableCaptivePortalDetectionPreference(false);

  RunDisabledTest(0);
  for (int i = 0; i < 6; ++i)
    RunDisabledTest(100);

  EnableCaptivePortalDetectionPreference(true);

  RunTest(captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL, net::OK, 200, 0, NULL);
}

// Check that disabling the captive portal service while a check is running
// works.
TEST_F(CaptivePortalServiceTest, CaptivePortalPrefDisabledWhileRunning) {
  Initialize(CaptivePortalService::SKIP_OS_CHECK_FOR_TESTING);
  CaptivePortalObserver observer(profile(), service());

  // Needed to create the URLFetcher, even if it never returns any results.
  service()->DetectCaptivePortal();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(FetchingURL());
  EXPECT_FALSE(TimerRunning());

  EnableCaptivePortalDetectionPreference(false);
  EXPECT_FALSE(FetchingURL());
  EXPECT_TRUE(TimerRunning());
  EXPECT_EQ(0, observer.num_results_received());

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(FetchingURL());
  EXPECT_FALSE(TimerRunning());
  EXPECT_EQ(1, observer.num_results_received());

  EXPECT_EQ(captive_portal::RESULT_INTERNET_CONNECTED,
            observer.captive_portal_result());
}

// Check that disabling the captive portal service while a check is pending
// works.
TEST_F(CaptivePortalServiceTest, CaptivePortalPrefDisabledWhilePending) {
  Initialize(CaptivePortalService::SKIP_OS_CHECK_FOR_TESTING);
  set_initial_backoff_no_portal(base::TimeDelta::FromDays(1));

  CaptivePortalObserver observer(profile(), service());
  service()->DetectCaptivePortal();
  EXPECT_FALSE(FetchingURL());
  EXPECT_TRUE(TimerRunning());

  EnableCaptivePortalDetectionPreference(false);
  EXPECT_FALSE(FetchingURL());
  EXPECT_TRUE(TimerRunning());
  EXPECT_EQ(0, observer.num_results_received());

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(FetchingURL());
  EXPECT_FALSE(TimerRunning());
  EXPECT_EQ(1, observer.num_results_received());

  EXPECT_EQ(captive_portal::RESULT_INTERNET_CONNECTED,
            observer.captive_portal_result());
}

// Check that disabling the captive portal service while a check is pending
// works.
TEST_F(CaptivePortalServiceTest, CaptivePortalPrefEnabledWhilePending) {
  Initialize(CaptivePortalService::SKIP_OS_CHECK_FOR_TESTING);

  EnableCaptivePortalDetectionPreference(false);
  RunDisabledTest(0);

  CaptivePortalObserver observer(profile(), service());
  service()->DetectCaptivePortal();
  EXPECT_FALSE(FetchingURL());
  EXPECT_TRUE(TimerRunning());

  EnableCaptivePortalDetectionPreference(true);
  EXPECT_FALSE(FetchingURL());
  EXPECT_TRUE(TimerRunning());

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(FetchingURL());
  EXPECT_FALSE(TimerRunning());

  CompleteURLFetch(net::OK, 200, NULL);
  EXPECT_FALSE(FetchingURL());
  EXPECT_FALSE(TimerRunning());

  EXPECT_EQ(1, observer.num_results_received());
  EXPECT_EQ(captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
            observer.captive_portal_result());
}

// Checks that disabling for browser tests works as expected.
TEST_F(CaptivePortalServiceTest, CaptivePortalDisableForTests) {
  Initialize(CaptivePortalService::DISABLED_FOR_TESTING);
  RunDisabledTest(0);
}

// Checks that jitter gives us values in the correct range.
TEST_F(CaptivePortalServiceTest, CaptivePortalJitter) {
  Initialize(CaptivePortalService::SKIP_OS_CHECK_FOR_TESTING);
  set_jitter_factor(0.3);
  set_initial_backoff_no_portal(base::TimeDelta::FromSeconds(100));
  RunTest(captive_portal::RESULT_INTERNET_CONNECTED, net::OK, 204, 0, NULL);
  RunTest(captive_portal::RESULT_INTERNET_CONNECTED, net::OK, 204, 0, NULL);

  for (int i = 0; i < 50; ++i) {
    int interval_sec = GetTimeUntilNextRequest().InSeconds();
    // Allow for roundoff, though shouldn't be necessary.
    EXPECT_LE(69, interval_sec);
    EXPECT_LE(interval_sec, 101);
  }
}

// Check a Retry-After header that contains a delay in seconds.
TEST_F(CaptivePortalServiceTest, CaptivePortalRetryAfterSeconds) {
  Initialize(CaptivePortalService::SKIP_OS_CHECK_FOR_TESTING);
  set_initial_backoff_no_portal(base::TimeDelta::FromSeconds(100));
  const char* retry_after = "HTTP/1.1 503 OK\nRetry-After: 101\n\n";

  // Check that Retry-After headers work both on the first request to return a
  // result and on subsequent requests.
  RunTest(captive_portal::RESULT_NO_RESPONSE, net::OK, 503, 0, retry_after);
  RunTest(captive_portal::RESULT_NO_RESPONSE, net::OK, 503, 101, retry_after);
  RunTest(captive_portal::RESULT_INTERNET_CONNECTED, net::OK, 204, 101, NULL);

  // Make sure that there's no effect on the next captive portal check after
  // login.
  EXPECT_EQ(base::TimeDelta::FromSeconds(0), GetTimeUntilNextRequest());
}

// Check that the RecheckPolicy is still respected on 503 responses with
// Retry-After headers.
TEST_F(CaptivePortalServiceTest, CaptivePortalRetryAfterSecondsTooShort) {
  Initialize(CaptivePortalService::SKIP_OS_CHECK_FOR_TESTING);
  set_initial_backoff_no_portal(base::TimeDelta::FromSeconds(100));
  const char* retry_after = "HTTP/1.1 503 OK\nRetry-After: 99\n\n";

  RunTest(captive_portal::RESULT_NO_RESPONSE, net::OK, 503, 0, retry_after);
  // Normally would be no delay on the first check with a new result.
  RunTest(captive_portal::RESULT_NO_RESPONSE, net::OK, 503, 99, retry_after);
  EXPECT_EQ(base::TimeDelta::FromSeconds(100), GetTimeUntilNextRequest());
}

// Check a Retry-After header that contains a date.
TEST_F(CaptivePortalServiceTest, CaptivePortalRetryAfterDate) {
  Initialize(CaptivePortalService::SKIP_OS_CHECK_FOR_TESTING);
  set_initial_backoff_no_portal(base::TimeDelta::FromSeconds(50));

  // base has a function to get a time in the right format from a string, but
  // not the other way around.
  base::Time start_time;
  ASSERT_TRUE(
      base::Time::FromString("Tue, 17 Apr 2012 18:02:00 GMT", &start_time));
  SetTime(start_time);

  RunTest(captive_portal::RESULT_NO_RESPONSE,
          net::OK,
          503,
          0,
          "HTTP/1.1 503 OK\nRetry-After: Tue, 17 Apr 2012 18:02:51 GMT\n\n");
  EXPECT_EQ(base::TimeDelta::FromSeconds(51), GetTimeUntilNextRequest());
}
