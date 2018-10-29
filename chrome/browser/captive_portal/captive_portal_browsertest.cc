// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/captive_portal/captive_portal_service.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"
#include "chrome/browser/captive_portal/captive_portal_tab_reloader.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/captive_portal_blocking_page.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ssl/ssl_error_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/http/transport_security_state.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"

using captive_portal::CaptivePortalResult;
using content::BrowserThread;
using content::WebContents;

namespace {

// Path of the fake login page, when using the TestServer.
const char* const kTestServerLoginPath = "/captive_portal/login.html";

// Path of a page with an iframe that has a mock SSL timeout, when using the
// TestServer.
const char* const kTestServerIframeTimeoutPath =
    "/captive_portal/iframe_timeout.html";

// Path of a page that redirects to kMockHttpsUrl.
const char* const kRedirectToMockHttpsPath =
    "/captive_portal/redirect_to_mock_https.html";

// Path of a page that serves a bad SSL certificate.
// The path doesn't matter because all we need is that it's served from a
// server that's configured to serve a bad cert.
const char* const kMockHttpsBadCertPath = "/bad_cert.html";

// The following URLs each have two different behaviors, depending on whether
// URLRequestMockCaptivePortalJobFactory is currently simulating the presence
// of a captive portal or not.  They use different domains so that HSTS can be
// applied to them independently.

// A mock URL for the CaptivePortalService's |test_url|.  When behind a captive
// portal, this URL returns a mock login page.  When connected to the Internet,
// it returns a 204 response.  Uses the name of the login file so that reloading
// it will not request a different URL.
const char* const kMockCaptivePortalTestUrl =
    "http://mock.captive.portal.test/login.html";

// Another mock URL for the CaptivePortalService's |test_url|.  When behind a
// captive portal, this URL returns a 511 status code and an HTML page that
// redirect to the above URL.  When connected to the Internet, it returns a 204
// response.
const char* const kMockCaptivePortal511Url =
    "http://mock.captive.portal.511/page511.html";

// When behind a captive portal, this URL hangs without committing until a call
// to FailJobs.  When that function is called, the request will time out.
//
// When connected to the Internet, this URL returns a non-error page.
const char* const kMockHttpsUrl =
    "https://mock.captive.portal.long.timeout/title2.html";

// Same as above, but different domain, so can be used to trigger cross-site
// navigations.
const char* const kMockHttpsUrl2 =
    "https://mock.captive.portal.long.timeout2/title2.html";

// Same as kMockHttpsUrl, except the timeout happens instantly.
const char* const kMockHttpsQuickTimeoutUrl =
    "https://mock.captive.portal.quick.timeout/title2.html";

// The intercepted URLs used to mock errors.
const char* const kMockHttpConnectionTimeoutErr =
    "http://mock.captive.portal.quick.error/timeout";
const char* const kMockHttpsConnectionTimeoutErr =
    "https://mock.captive.portal.quick.error/timeout";
const char* const kMockHttpsConnectionUnexpectedErr =
    "https://mock.captive.portal.quick.error/unexpected";
const char* const kMockHttpConnectionConnectionClosedErr =
    "http://mock.captive.portal.quick.error/connection_closed";

// Expected title of a tab once an HTTPS load completes, when not behind a
// captive portal.
const char* const kInternetConnectedTitle = "Title Of Awesomeness";

// Creates a server-side redirect for use with the TestServer.
std::string CreateServerRedirect(const std::string& dest_url) {
  const char* const kServerRedirectBase = "/server-redirect?";
  return kServerRedirectBase + dest_url;
}

// Returns the total number of loading tabs across all Browsers, for all
// Profiles.
int NumLoadingTabs() {
  return std::count_if(AllTabContentses().begin(), AllTabContentses().end(),
                       [](content::WebContents* web_contents) {
                         return web_contents->IsLoading();
                       });
}

bool IsLoginTab(WebContents* web_contents) {
  return CaptivePortalTabHelper::FromWebContents(web_contents)->IsLoginTab();
}

// Tracks how many times each tab has been navigated since the Observer was
// created.  The standard TestNavigationObserver can only watch specific
// pre-existing tabs or loads in serial for all tabs.
class MultiNavigationObserver : public content::NotificationObserver {
 public:
  MultiNavigationObserver();
  ~MultiNavigationObserver() override;

  // Waits for exactly |num_navigations_to_wait_for| LOAD_STOP
  // notifications to have occurred since the construction of |this|.  More
  // navigations than expected occuring will trigger a expect failure.
  void WaitForNavigations(int num_navigations_to_wait_for);

  // Returns the number of LOAD_STOP events that have occurred for
  // |web_contents| since this was constructed.
  int NumNavigationsForTab(WebContents* web_contents) const;

  // The number of LOAD_STOP events since |this| was created.
  int num_navigations() const { return num_navigations_; }

 private:
  typedef std::map<const WebContents*, int> TabNavigationMap;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  int num_navigations_;

  // Map of how many times each tab has navigated since |this| was created.
  TabNavigationMap tab_navigation_map_;

  // Total number of navigations to wait for.  Value only matters when
  // |waiting_for_navigation_| is true.
  int num_navigations_to_wait_for_;

  // True if WaitForNavigations has been called, until
  // |num_navigations_to_wait_for_| have been observed.
  bool waiting_for_navigation_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(MultiNavigationObserver);
};

MultiNavigationObserver::MultiNavigationObserver()
    : num_navigations_(0),
      num_navigations_to_wait_for_(0),
      waiting_for_navigation_(false) {
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::NotificationService::AllSources());
}

MultiNavigationObserver::~MultiNavigationObserver() {
}

void MultiNavigationObserver::WaitForNavigations(
    int num_navigations_to_wait_for) {
  // Shouldn't already be waiting for navigations.
  EXPECT_FALSE(waiting_for_navigation_);
  EXPECT_LT(0, num_navigations_to_wait_for);
  if (num_navigations_ < num_navigations_to_wait_for) {
    num_navigations_to_wait_for_ = num_navigations_to_wait_for;
    waiting_for_navigation_ = true;
    content::RunMessageLoop();
    EXPECT_FALSE(waiting_for_navigation_);
  }
  EXPECT_EQ(num_navigations_, num_navigations_to_wait_for);
}

int MultiNavigationObserver::NumNavigationsForTab(
    WebContents* web_contents) const {
  auto tab_navigations = tab_navigation_map_.find(web_contents);
  if (tab_navigations == tab_navigation_map_.end())
    return 0;
  return tab_navigations->second;
}

void MultiNavigationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  ASSERT_EQ(type, content::NOTIFICATION_LOAD_STOP);
  content::NavigationController* controller =
      content::Source<content::NavigationController>(source).ptr();
  ++num_navigations_;
  ++tab_navigation_map_[controller->GetWebContents()];
  if (waiting_for_navigation_ &&
      num_navigations_to_wait_for_ == num_navigations_) {
    waiting_for_navigation_ = false;
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }
}

// This observer creates a list of loading tabs, and then waits for them all
// to stop loading and have the kInternetConnectedTitle.
//
// This is for the specific purpose of observing tabs time out after logging in
// to a captive portal, which will then cause them to reload.
// MultiNavigationObserver is insufficient for this because there may or may not
// be a LOAD_STOP event between the timeout and the reload.
// See bug http://crbug.com/133227
class FailLoadsAfterLoginObserver : public content::NotificationObserver {
 public:
  FailLoadsAfterLoginObserver();
  ~FailLoadsAfterLoginObserver() override;

  void WaitForNavigations();

 private:
  typedef std::set<const WebContents*> TabSet;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // The set of tabs that need to be navigated.  This is the set of loading
  // tabs when the observer is created.
  TabSet tabs_needing_navigation_;

  // Number of tabs that have stopped navigating with the expected title.  These
  // are expected not to be navigated again.
  TabSet tabs_navigated_to_final_destination_;

  // True if WaitForNavigations has been called, until
  // |tabs_navigated_to_final_destination_| equals |tabs_needing_navigation_|.
  bool waiting_for_navigation_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(FailLoadsAfterLoginObserver);
};

FailLoadsAfterLoginObserver::FailLoadsAfterLoginObserver()
    : waiting_for_navigation_(false) {
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::NotificationService::AllSources());
  std::copy_if(
      AllTabContentses().begin(), AllTabContentses().end(),
      std::inserter(tabs_needing_navigation_, tabs_needing_navigation_.end()),
      [](content::WebContents* web_contents) {
        return web_contents->IsLoading();
      });
}

FailLoadsAfterLoginObserver::~FailLoadsAfterLoginObserver() {
}

void FailLoadsAfterLoginObserver::WaitForNavigations() {
  // Shouldn't already be waiting for navigations.
  EXPECT_FALSE(waiting_for_navigation_);
  if (tabs_needing_navigation_.size() !=
          tabs_navigated_to_final_destination_.size()) {
    waiting_for_navigation_ = true;
    content::RunMessageLoop();
    EXPECT_FALSE(waiting_for_navigation_);
  }
  EXPECT_EQ(tabs_needing_navigation_.size(),
            tabs_navigated_to_final_destination_.size());
}

void FailLoadsAfterLoginObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  ASSERT_EQ(type, content::NOTIFICATION_LOAD_STOP);
  content::NavigationController* controller =
      content::Source<content::NavigationController>(source).ptr();
  WebContents* contents = controller->GetWebContents();

  ASSERT_EQ(1u, tabs_needing_navigation_.count(contents));
  ASSERT_EQ(0u, tabs_navigated_to_final_destination_.count(contents));

  if (contents->GetTitle() != base::ASCIIToUTF16(kInternetConnectedTitle))
    return;
  tabs_navigated_to_final_destination_.insert(contents);

  if (waiting_for_navigation_ &&
      tabs_needing_navigation_.size() ==
          tabs_navigated_to_final_destination_.size()) {
    waiting_for_navigation_ = false;
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }
}

// An observer for watching the CaptivePortalService.  It tracks the last
// received result and the total number of received results.
class CaptivePortalObserver : public content::NotificationObserver {
 public:
  explicit CaptivePortalObserver(Profile* profile);

  // Runs the message loop until exactly |update_count| captive portal
  // results have been received, since the creation of |this|.  Expects no
  // additional captive portal results.
  void WaitForResults(int num_results_to_wait_for);

  int num_results_received() const { return num_results_received_; }

  CaptivePortalResult captive_portal_result() const {
    return captive_portal_result_;
  }

 private:
  // Records results and exits the message loop, if needed.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Number of times OnPortalResult has been called since construction.
  int num_results_received_;

  // If WaitForResults was called, the total number of updates for which to
  // wait.  Value doesn't matter when |waiting_for_result_| is false.
  int num_results_to_wait_for_;

  bool waiting_for_result_;

  Profile* profile_;

  CaptivePortalService* captive_portal_service_;

  // Last result received.
  CaptivePortalResult captive_portal_result_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalObserver);
};

CaptivePortalObserver::CaptivePortalObserver(Profile* profile)
    : num_results_received_(0),
      num_results_to_wait_for_(0),
      waiting_for_result_(false),
      profile_(profile),
      captive_portal_service_(
          CaptivePortalServiceFactory::GetForProfile(profile)),
      captive_portal_result_(
          captive_portal_service_->last_detection_result()) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT,
                 content::Source<Profile>(profile_));
}

void CaptivePortalObserver::WaitForResults(int num_results_to_wait_for) {
  EXPECT_LT(0, num_results_to_wait_for);
  EXPECT_FALSE(waiting_for_result_);
  if (num_results_received_ < num_results_to_wait_for) {
    num_results_to_wait_for_ = num_results_to_wait_for;
    waiting_for_result_ = true;
    content::RunMessageLoop();
    EXPECT_FALSE(waiting_for_result_);
  }
  EXPECT_EQ(num_results_to_wait_for, num_results_received_);
}

void CaptivePortalObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  ASSERT_EQ(type, chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT);
  ASSERT_EQ(profile_, content::Source<Profile>(source).ptr());

  CaptivePortalService::Results* results =
      content::Details<CaptivePortalService::Results>(details).ptr();

  EXPECT_EQ(captive_portal_result_, results->previous_result);
  EXPECT_EQ(captive_portal_service_->last_detection_result(),
            results->result);

  captive_portal_result_ = results->result;
  ++num_results_received_;

  if (waiting_for_result_ &&
      num_results_to_wait_for_ == num_results_received_) {
    waiting_for_result_ = false;
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }
}

// This observer waits for the SSLErrorHandler to start an interstitial timer
// for the given web contents.
class SSLInterstitialTimerObserver {
 public:
  explicit SSLInterstitialTimerObserver(content::WebContents* web_contents);
  ~SSLInterstitialTimerObserver();

  // Waits until the interstitial delay timer in SSLErrorHandler is started.
  void WaitForTimerStarted();

 private:
  void OnTimerStarted(content::WebContents* web_contents);

  const content::WebContents* web_contents_;
  SSLErrorHandler::TimerStartedCallback callback_;

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(SSLInterstitialTimerObserver);
};

SSLInterstitialTimerObserver::SSLInterstitialTimerObserver(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      message_loop_runner_(new content::MessageLoopRunner) {
  callback_ = base::Bind(&SSLInterstitialTimerObserver::OnTimerStarted,
                         base::Unretained(this));
  SSLErrorHandler::SetInterstitialTimerStartedCallbackForTesting(&callback_);
}

SSLInterstitialTimerObserver::~SSLInterstitialTimerObserver() {
  SSLErrorHandler::SetInterstitialTimerStartedCallbackForTesting(nullptr);
}

void SSLInterstitialTimerObserver::WaitForTimerStarted() {
  message_loop_runner_->Run();
}

void SSLInterstitialTimerObserver::OnTimerStarted(
    content::WebContents* web_contents) {
  if (web_contents_ == web_contents && message_loop_runner_.get())
    message_loop_runner_->Quit();
}

// Helper for waiting for a change of the active tab.
// Users can wait for the change via WaitForActiveTabChange method.
// DCHECKs ensure that only one change happens during the lifetime of a
// TabActivationWaiter instance.
class TabActivationWaiter : public TabStripModelObserver {
 public:
  explicit TabActivationWaiter(TabStripModel* tab_strip_model)
      : number_of_unconsumed_active_tab_changes_(0), scoped_observer_(this) {
    scoped_observer_.Add(tab_strip_model);
  }

  void WaitForActiveTabChange() {
    if (number_of_unconsumed_active_tab_changes_ == 0) {
      // Wait until TabStripModelObserver::ActiveTabChanged will get called.
      message_loop_runner_ = new content::MessageLoopRunner;
      message_loop_runner_->Run();
    }

    // "consume" one tab activation event.
    DCHECK_EQ(1, number_of_unconsumed_active_tab_changes_);
    number_of_unconsumed_active_tab_changes_--;
  }

  // TabStripModelObserver overrides.
  void ActiveTabChanged(content::WebContents* old_contents,
                        content::WebContents* new_contents,
                        int index,
                        int reason) override {
    number_of_unconsumed_active_tab_changes_++;
    DCHECK_EQ(1, number_of_unconsumed_active_tab_changes_);
    if (message_loop_runner_)
      message_loop_runner_->Quit();
  }

 private:
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  int number_of_unconsumed_active_tab_changes_;
  ScopedObserver<TabStripModel, TabActivationWaiter> scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(TabActivationWaiter);
};

}  // namespace

class CaptivePortalBrowserTest : public InProcessBrowserTest {
 public:
  CaptivePortalBrowserTest();

  // InProcessBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // Called by |url_loader_interceptor_|.
  // It emulates captive portal behavior.
  // Initially, it emulates being behind a captive portal. When
  // SetBehindCaptivePortal(false) is called, it emulates behavior when not
  // behind a captive portal.
  bool OnIntercept(content::URLLoaderInterceptor::RequestParams* params);

  // Sets the captive portal checking preference.  Does not affect the command
  // line flag, which is set in SetUpCommandLine.
  void EnableCaptivePortalDetection(Profile* profile, bool enabled);

  // Enables or disables actual captive portal probes. Should only be called
  // after captive portal service setup is done. When disabled, probe requests
  // are silently ignored, never receiving a response.
  void RespondToProbeRequests(bool enabled);

  // Sets up the captive portal service for the given profile so that
  // all checks go to |test_url|.  Also disables all timers.
  void SetUpCaptivePortalService(Profile* profile, const GURL& test_url);

  // Returns true if |browser|'s profile is currently running a captive portal
  // check.
  bool CheckPending(Browser* browser);

  // Returns the type of the interstitial being shown.
  content::InterstitialPageDelegate::TypeID GetInterstitialType(
      WebContents* contents) const;

  // Returns the CaptivePortalTabReloader::State of |web_contents|.
  CaptivePortalTabReloader::State GetStateOfTabReloader(
      WebContents* web_contents) const;

  // Returns the CaptivePortalTabReloader::State of the indicated tab.
  CaptivePortalTabReloader::State GetStateOfTabReloaderAt(Browser* browser,
                                                          int index) const;

  // Returns the number of tabs with the given state, across all profiles.
  int NumTabsWithState(CaptivePortalTabReloader::State state) const;

  // Returns the number of tabs broken by captive portals, across all profiles.
  int NumBrokenTabs() const;

  // Returns the number of tabs that need to be reloaded due to having logged
  // in to a captive portal, across all profiles.
  int NumNeedReloadTabs() const;

  // Navigates |browser|'s active tab to |url| and expects no captive portal
  // test to be triggered.  |expected_navigations| is the number of times the
  // active tab will end up being navigated.  It should be 1, except for the
  // Link Doctor page, which acts like two navigations.
  void NavigateToPageExpectNoTest(Browser* browser,
                                  const GURL& url,
                                  int expected_navigations);

  // Navigates |browser|'s active tab to an SSL tab that takes a while to load,
  // triggering a captive portal check, which is expected to give the result
  // |expected_result|.  The page finishes loading, with a timeout, after the
  // captive portal check.
  void SlowLoadNoCaptivePortal(Browser* browser,
                               CaptivePortalResult expected_result);

  // Navigates |browser|'s active tab to an SSL timeout, expecting a captive
  // portal check to be triggered and return a result which will indicates
  // there's no detected captive portal.
  void FastTimeoutNoCaptivePortal(Browser* browser,
                                  CaptivePortalResult expected_result);

  // Navigates the active tab to a slow loading SSL page, which will then
  // trigger a captive portal test.  The test is expected to find a captive
  // portal.  The slow loading page will continue to load after the function
  // returns, until FailJobs() is called, at which point it will timeout.
  //
  // When |expect_open_login_tab| is false, no login tab is expected to be
  // opened, because one already exists, and the function returns once the
  // captive portal test is complete.
  //
  // If |expect_open_login_tab| is true, a login tab is then expected to be
  // opened. It waits until both the login tab has finished loading, and two
  // captive portal tests complete.  The second test is triggered by the load of
  // the captive portal tab completing.
  //
  // This function must not be called when the active tab is currently loading.
  // Waits for the hanging request to be issued, so other functions can rely
  // on WaitForJobs having been called.
  void SlowLoadBehindCaptivePortal(Browser* browser,
                                   bool expect_open_login_tab);

  // Same as above, but takes extra parameters.
  //
  // |hanging_url| should either be kMockHttpsUrl or redirect to kMockHttpsUrl.
  //
  // |expected_portal_checks| and |expected_login_tab_navigations| allow
  // client-side redirects to be tested.  |expected_login_tab_navigations| is
  // ignored when |expect_open_login_tab| is false.
  void SlowLoadBehindCaptivePortal(Browser* browser,
                                   bool expect_open_login_tab,
                                   const GURL& hanging_url,
                                   int expected_portal_checks,
                                   int expected_login_tab_navigations);

  // Just like SlowLoadBehindCaptivePortal, except the navigated tab has
  // a connection timeout rather having its time trigger, and the function
  // waits until that timeout occurs.
  void FastTimeoutBehindCaptivePortal(Browser* browser,
                                      bool expect_open_login_tab);

  // Much as above, but accepts a URL parameter and can be used for errors that
  // trigger captive portal checks other than timeouts.  |error_url| should
  // result in an error rather than hanging.
  void FastErrorBehindCaptivePortal(Browser* browser,
                                    bool expect_open_login_tab,
                                    const GURL& error_url);

  // Navigates the active tab to an SSL error page which triggers an
  // interstitial timer. Also disables captive portal checks indefinitely, so
  // the page appears to be hanging.
  void FastErrorWithInterstitialTimer(Browser* browser,
                                      const GURL& cert_error_url);

  // Navigates the login tab without logging in.  The login tab must be the
  // specified browser's active tab.  Expects no other tab to change state.
  // |num_loading_tabs| and |num_timed_out_tabs| are used as extra checks
  // that nothing has gone wrong prior to the function call.
  void NavigateLoginTab(Browser* browser,
                        int num_loading_tabs,
                        int num_timed_out_tabs);

  // Simulates a login by updating the URLRequestMockCaptivePortalJob's
  // behind captive portal state, and navigating the login tab.  Waits for
  // all broken but not loading tabs to be reloaded.
  // |num_loading_tabs| and |num_timed_out_tabs| are used as extra checks
  // that nothing has gone wrong prior to the function call.
  void Login(Browser* browser, int num_loading_tabs, int num_timed_out_tabs);

  // Simulates a login when the broken tab shows an SSL or captive portal
  // interstitial. Can't use Login() in those cases because the interstitial
  // tab looks like a cross between a hung tab (Load was never committed) and a
  // tab at an error page (The load was stopped).
  void LoginCertError(Browser* browser);

  // Makes the slow SSL loads of all active tabs time out at once, and waits for
  // them to finish both that load and the automatic reload it should trigger.
  // There should be no timed out tabs when this is called.
  void FailLoadsAfterLogin(Browser* browser, int num_loading_tabs);

  // Makes the slow SSL loads of all active tabs time out at once, and waits for
  // them to finish displaying their error pages.  The login tab should be the
  // active tab.  There should be no timed out tabs when this is called.
  void FailLoadsWithoutLogin(Browser* browser, int num_loading_tabs);

  // Navigates |browser|'s active tab to |starting_url| while not behind a
  // captive portal.  Then navigates to |interrupted_url|, which should create
  // a URLRequestTimeoutOnDemandJob, which is then abandoned.  The load should
  // trigger a captive portal check, which finds a captive portal and opens a
  // tab.
  //
  // Then the navigation is interrupted by a navigation to |timeout_url|, which
  // should trigger a captive portal check, and finally the test simulates
  // logging in.
  //
  // The purpose of this test is to make sure the TabHelper triggers a captive
  // portal check when a load is interrupted by another load, particularly in
  // the case of cross-process navigations.
  void RunNavigateLoadingTabToTimeoutTest(Browser* browser,
                                          const GURL& starting_url,
                                          const GURL& interrupted_url,
                                          const GURL& timeout_url);

  // Sets the timeout used by a CaptivePortalTabReloader on slow SSL loads
  // before a captive portal check.
  void SetSlowSSLLoadTime(CaptivePortalTabReloader* tab_reloader,
                          base::TimeDelta slow_ssl_load_time);

  CaptivePortalTabReloader* GetTabReloader(WebContents* web_contents) const;

  // Sets whether or not there is a captive portal. Outstanding requests are
  // not affected.
  void SetBehindCaptivePortal(bool behind_captive_portal) {
    if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      base::PostTaskWithTraits(
          FROM_HERE, {content::BrowserThread::IO},
          base::BindOnce(&CaptivePortalBrowserTest::SetBehindCaptivePortal,
                         base::Unretained(this), behind_captive_portal));
      return;
    }

    behind_captive_portal_ = behind_captive_portal;
  }

  // Waits for exactly |num_jobs| kMockHttps* requests.
  void WaitForJobs(int num_jobs) {
    if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      base::PostTaskWithTraits(
          FROM_HERE, {content::BrowserThread::IO},
          base::BindOnce(&CaptivePortalBrowserTest::WaitForJobs,
                         base::Unretained(this), num_jobs));
      content::RunMessageLoop();
      return;
    }

    DCHECK(!num_jobs_to_wait_for_);
    EXPECT_LE(static_cast<int>(ongoing_mock_requests_.size()), num_jobs);
    if (num_jobs == static_cast<int>(ongoing_mock_requests_.size())) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::RunLoop::QuitCurrentWhenIdleClosureDeprecated());
    } else {
      num_jobs_to_wait_for_ = num_jobs;
    }
  }

  // Fails all active kMockHttps* requests with connection timeouts.
  // There are expected to be exactly |expected_num_jobs| waiting for
  // failure.  The only way to guarantee this is with an earlier call to
  // WaitForJobs, so makes sure there has been a matching WaitForJobs call.
  void FailJobs(int expected_num_jobs) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(&CaptivePortalBrowserTest::FailJobs,
                         base::Unretained(this), expected_num_jobs));
      return;
    }

    EXPECT_EQ(expected_num_jobs,
              static_cast<int>(ongoing_mock_requests_.size()));
    network::URLLoaderCompletionStatus status;
    status.error_code = net::ERR_CONNECTION_TIMED_OUT;
    for (auto& job : ongoing_mock_requests_)
      job.client->OnComplete(status);
    ongoing_mock_requests_.clear();
  }

  // Fails all active kMockHttps* requests with SSL cert errors.
  // |expected_num_jobs| behaves just as in FailJobs.
  void FailJobsWithCertError(int expected_num_jobs,
                             const net::SSLInfo& ssl_info) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(&CaptivePortalBrowserTest::FailJobsWithCertError,
                         base::Unretained(this), expected_num_jobs, ssl_info));
      return;
    }

    DCHECK(intercept_bad_cert_);
    // With the network service enabled, these will be requests to
    // kMockHttpsBadCertPath that is served by a misconfigured
    // EmbeddedTestServer. Once the request reaches the network service, it'll
    // notice the bad SSL cert.
    // Set |intercept_bad_cert_| so that when we use the network service'
    // URLLoaderFactory again it doesn't get intercepted and goes to the
    // nework process. This has to be done on the UI thread as that's where we
    // currently have a public URLLoaderFactory for the profile.
    intercept_bad_cert_ = false;
    EXPECT_EQ(expected_num_jobs,
              static_cast<int>(ongoing_mock_requests_.size()));
    for (auto& job : ongoing_mock_requests_) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&CaptivePortalBrowserTest::CreateLoader,
                         base::Unretained(this), std::move(job)));
    }
    ongoing_mock_requests_.clear();
  }

  void CreateLoader(content::URLLoaderInterceptor::RequestParams job) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
        ->GetURLLoaderFactoryForBrowserProcess()
        ->CreateLoaderAndStart(std::move(job.request), job.routing_id,
                               job.request_id, job.options,
                               std::move(job.url_request),
                               std::move(job.client), job.traffic_annotation);
  }

  // Abandon all active kMockHttps* requests.  |expected_num_jobs|
  // behaves just as in FailJobs.
  void AbandonJobs(int expected_num_jobs) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(&CaptivePortalBrowserTest::AbandonJobs,
                         base::Unretained(this), expected_num_jobs));
      return;
    }

    EXPECT_EQ(expected_num_jobs,
              static_cast<int>(ongoing_mock_requests_.size()));
    for (auto& job : ongoing_mock_requests_)
      ignore_result(job.client.PassInterface().PassHandle().release());
    ongoing_mock_requests_.clear();
  }

  // Returns the contents of the given filename under chrome/test/data.
  static std::string GetContents(const std::string& path) {
    base::FilePath root_http;
    base::PathService::Get(chrome::DIR_TEST_DATA, &root_http);
    base::ScopedAllowBlockingForTesting allow_io;
    base::FilePath file_path = root_http.AppendASCII(path);
    std::string contents;
    CHECK(base::ReadFileToString(file_path, &contents));
    return contents;
  }

 protected:
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
  // Only accessed on the IO thread.
  int num_jobs_to_wait_for_ = 0;
  std::vector<content::URLLoaderInterceptor::RequestParams>
      ongoing_mock_requests_;
  bool behind_captive_portal_ = true;
  bool intercept_bad_cert_ = true;

 private:
  DISALLOW_COPY_AND_ASSIGN(CaptivePortalBrowserTest);
};

CaptivePortalBrowserTest::CaptivePortalBrowserTest() {
}

void CaptivePortalBrowserTest::SetUpOnMainThread() {
  url_loader_interceptor_ =
      std::make_unique<content::URLLoaderInterceptor>(base::Bind(
          &CaptivePortalBrowserTest::OnIntercept, base::Unretained(this)));

  // Double-check that the captive portal service isn't enabled by default for
  // browser tests.
  EXPECT_EQ(CaptivePortalService::DISABLED_FOR_TESTING,
            CaptivePortalService::get_state_for_testing());

  CaptivePortalService::set_state_for_testing(
      CaptivePortalService::SKIP_OS_CHECK_FOR_TESTING);
  EnableCaptivePortalDetection(browser()->profile(), true);

  // Set the captive portal service to use URLRequestMockCaptivePortalJob's
  // mock URL, by default.
  SetUpCaptivePortalService(browser()->profile(),
                            GURL(kMockCaptivePortalTestUrl));

  // Set SSL interstitial delay long enough so that a captive portal result
  // is guaranteed to arrive during this window, and a captive portal
  // error page is displayed instead of an SSL interstitial.
  SSLErrorHandler::SetInterstitialDelayForTesting(
      base::TimeDelta::FromHours(1));
}

bool CaptivePortalBrowserTest::OnIntercept(
    content::URLLoaderInterceptor::RequestParams* params) {
  if (params->url_request.url.path() == kMockHttpsBadCertPath &&
      intercept_bad_cert_) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ongoing_mock_requests_.emplace_back(std::move(*params));
    return true;
  }

  auto url_string = params->url_request.url.spec();
  net::Error error = net::OK;
  if (url_string == kMockHttpConnectionTimeoutErr ||
      url_string == kMockHttpsConnectionTimeoutErr) {
    error = net::ERR_CONNECTION_TIMED_OUT;
  } else if (url_string == kMockHttpsConnectionUnexpectedErr) {
    error = net::ERR_UNEXPECTED;
  } else if (url_string == kMockHttpConnectionConnectionClosedErr) {
    error = net::ERR_CONNECTION_CLOSED;
  }
  if (error != net::OK) {
    params->client->OnComplete(network::URLLoaderCompletionStatus(error));
    return true;
  }

  if (url_string == kMockHttpsUrl || url_string == kMockHttpsUrl2 ||
      url_string == kMockHttpsQuickTimeoutUrl ||
      params->url_request.url.path() == kRedirectToMockHttpsPath) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (params->url_request.url.path() == kRedirectToMockHttpsPath) {
      net::RedirectInfo redirect_info;
      redirect_info.new_url = GURL(kMockHttpsUrl);
      redirect_info.new_method = "GET";

      std::string headers;
      headers = base::StringPrintf(
          "HTTP/1.0 301 Moved permanently\n"
          "Location: %s\n"
          "Content-Type: text/html\n\n",
          kMockHttpsUrl);
      net::HttpResponseInfo info;
      info.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
          net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.length()));
      network::ResourceResponseHead response;
      response.headers = info.headers;
      response.headers->GetMimeType(&response.mime_type);
      response.encoded_data_length = 0;
      params->client->OnReceiveRedirect(redirect_info, response);
    }

    if (behind_captive_portal_) {
      if (url_string == kMockHttpsQuickTimeoutUrl) {
        network::URLLoaderCompletionStatus status;
        status.error_code = net::ERR_CONNECTION_TIMED_OUT;
        params->client->OnComplete(status);
      } else {
        ongoing_mock_requests_.emplace_back(std::move(*params));
        if (num_jobs_to_wait_for_ ==
            static_cast<int>(ongoing_mock_requests_.size())) {
          num_jobs_to_wait_for_ = 0;
          base::PostTaskWithTraits(
              FROM_HERE, {BrowserThread::UI},
              base::RunLoop::QuitCurrentWhenIdleClosureDeprecated());
        }
      }
    } else {
      // Once logged in to the portal, HTTPS requests return the page that was
      // actually requested.
      content::URLLoaderInterceptor::WriteResponse(
          "HTTP/1.1 200 OK\nContent-type: text/html\n\n",
          GetContents("title2.html"), params->client.get());
    }
    return true;
  }

  std::string headers;
  if (url_string == kMockCaptivePortalTestUrl ||
      url_string == kMockCaptivePortal511Url) {
    std::string contents;
    if (behind_captive_portal_) {
      // Prior to logging in to the portal, the HTTP test URLs are
      // intercepted by the captive portal.
      if (url_string == kMockCaptivePortal511Url) {
        contents = GetContents("captive_portal/page511.html");
        headers = "HTTP/1.1 511 Network Authentication Required\n";
      } else {
        contents = GetContents("captive_portal/login.html");
        headers = "HTTP/1.0 200 Just Peachy\n";
      }
    } else {
      // After logging in to the portal, the test URLs return a 204
      // response.
      headers = "HTTP/1.0 204 No Content\nContent-Length: 0\n";
    }
    headers += "Content-Type: text/html\n\n";
    content::URLLoaderInterceptor::WriteResponse(headers, contents,
                                                 params->client.get());
    return true;
  }

  return false;
}

void CaptivePortalBrowserTest::TearDownOnMainThread() {
  // No test should have a captive portal check pending on quit.
  EXPECT_FALSE(CheckPending(browser()));
  url_loader_interceptor_.reset();
}

void CaptivePortalBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Enable finch experiment for captive portal interstitials.
  command_line->AppendSwitchASCII(
      switches::kForceFieldTrials, "CaptivePortalInterstitial/Enabled/");
}

void CaptivePortalBrowserTest::EnableCaptivePortalDetection(
    Profile* profile, bool enabled) {
  profile->GetPrefs()->SetBoolean(prefs::kAlternateErrorPagesEnabled, enabled);
}

void CaptivePortalBrowserTest::RespondToProbeRequests(bool enabled) {
  if (enabled) {
    EXPECT_EQ(CaptivePortalService::IGNORE_REQUESTS_FOR_TESTING,
              CaptivePortalService::get_state_for_testing());
    CaptivePortalService::set_state_for_testing(
        CaptivePortalService::SKIP_OS_CHECK_FOR_TESTING);
  } else {
    EXPECT_EQ(CaptivePortalService::SKIP_OS_CHECK_FOR_TESTING,
              CaptivePortalService::get_state_for_testing());
    CaptivePortalService::set_state_for_testing(
        CaptivePortalService::IGNORE_REQUESTS_FOR_TESTING);
  }
}

void CaptivePortalBrowserTest::SetUpCaptivePortalService(Profile* profile,
                                                         const GURL& test_url) {
  CaptivePortalService* captive_portal_service =
      CaptivePortalServiceFactory::GetForProfile(profile);
  captive_portal_service->set_test_url(test_url);

  // Don't use any non-zero timers.  Timers are checked in unit tests.
  CaptivePortalService::RecheckPolicy* recheck_policy =
      &captive_portal_service->recheck_policy();
  recheck_policy->initial_backoff_no_portal_ms = 0;
  recheck_policy->initial_backoff_portal_ms = 0;
  recheck_policy->backoff_policy.maximum_backoff_ms = 0;
}

bool CaptivePortalBrowserTest::CheckPending(Browser* browser) {
  CaptivePortalService* captive_portal_service =
      CaptivePortalServiceFactory::GetForProfile(browser->profile());

  return captive_portal_service->DetectionInProgress() ||
      captive_portal_service->TimerRunning();
}

content::InterstitialPageDelegate::TypeID
CaptivePortalBrowserTest::GetInterstitialType(WebContents* contents) const {
  if (!contents->ShowingInterstitialPage())
    return nullptr;
  return contents->GetInterstitialPage()
      ->GetDelegateForTesting()
      ->GetTypeForTesting();
}

CaptivePortalTabReloader::State CaptivePortalBrowserTest::GetStateOfTabReloader(
    WebContents* web_contents) const {
  return GetTabReloader(web_contents)->state();
}

CaptivePortalTabReloader::State
CaptivePortalBrowserTest::GetStateOfTabReloaderAt(Browser* browser,
                                                  int index) const {
  return GetStateOfTabReloader(
      browser->tab_strip_model()->GetWebContentsAt(index));
}

int CaptivePortalBrowserTest::NumTabsWithState(
    CaptivePortalTabReloader::State state) const {
  return std::count_if(AllTabContentses().begin(), AllTabContentses().end(),
                       [this, state](content::WebContents* web_contents) {
                         return GetStateOfTabReloader(web_contents) == state;
                       });
}

int CaptivePortalBrowserTest::NumBrokenTabs() const {
  return NumTabsWithState(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL);
}

int CaptivePortalBrowserTest::NumNeedReloadTabs() const {
  return NumTabsWithState(CaptivePortalTabReloader::STATE_NEEDS_RELOAD);
}

void CaptivePortalBrowserTest::NavigateToPageExpectNoTest(
    Browser* browser,
    const GURL& url,
    int expected_navigations) {
  MultiNavigationObserver navigation_observer;
  CaptivePortalObserver portal_observer(browser->profile());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser, url, expected_navigations);

  // No captive portal checks should have ocurred or be pending, and there
  // should be no new tabs.
  EXPECT_EQ(0, portal_observer.num_results_received());
  EXPECT_FALSE(CheckPending(browser));
  EXPECT_EQ(1, browser->tab_strip_model()->count());
  EXPECT_EQ(expected_navigations, navigation_observer.num_navigations());
  EXPECT_EQ(0, NumLoadingTabs());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser, 0));
}

void CaptivePortalBrowserTest::SlowLoadNoCaptivePortal(
    Browser* browser,
    CaptivePortalResult expected_result) {
  CaptivePortalTabReloader* tab_reloader =
      GetTabReloader(browser->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(tab_reloader);
  SetSlowSSLLoadTime(tab_reloader, base::TimeDelta());

  MultiNavigationObserver navigation_observer;
  CaptivePortalObserver portal_observer(browser->profile());
  ui_test_utils::NavigateToURLWithDisposition(
      browser, GURL(kMockHttpsUrl), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  portal_observer.WaitForResults(1);

  ASSERT_EQ(1, browser->tab_strip_model()->count());
  EXPECT_EQ(expected_result, portal_observer.captive_portal_result());
  EXPECT_EQ(1, portal_observer.num_results_received());
  EXPECT_EQ(0, navigation_observer.num_navigations());
  EXPECT_FALSE(CheckPending(browser));

  // First tab should still be loading.
  EXPECT_EQ(1, NumLoadingTabs());

  // Wait for the request to be issued, then time it out.
  WaitForJobs(1);
  FailJobs(1);
  navigation_observer.WaitForNavigations(1);

  ASSERT_EQ(1, browser->tab_strip_model()->count());
  EXPECT_EQ(1, portal_observer.num_results_received());
  EXPECT_FALSE(CheckPending(browser));
  EXPECT_EQ(0, NumLoadingTabs());

  // Set a slow SSL load time to prevent the timer from triggering.
  SetSlowSSLLoadTime(tab_reloader, base::TimeDelta::FromDays(1));
}

void CaptivePortalBrowserTest::FastTimeoutNoCaptivePortal(
    Browser* browser,
    CaptivePortalResult expected_result) {
  ASSERT_NE(expected_result, captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL);

  // Set the load time to be large, so the timer won't trigger.  The value is
  // not restored at the end of the function.
  CaptivePortalTabReloader* tab_reloader =
      GetTabReloader(browser->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(tab_reloader);
  SetSlowSSLLoadTime(tab_reloader, base::TimeDelta::FromHours(1));

  MultiNavigationObserver navigation_observer;
  CaptivePortalObserver portal_observer(browser->profile());

  // Neither of these should be changed by the navigation.
  int active_index = browser->tab_strip_model()->active_index();
  int expected_tab_count = browser->tab_strip_model()->count();

  ui_test_utils::NavigateToURL(browser, GURL(kMockHttpsConnectionTimeoutErr));

  // An attempt to detect a captive portal should have started by now.  If not,
  // abort early to prevent hanging.
  ASSERT_TRUE(portal_observer.num_results_received() > 0 ||
              CheckPending(browser));

  portal_observer.WaitForResults(1);
  navigation_observer.WaitForNavigations(1);

  // Check the result.
  EXPECT_EQ(1, portal_observer.num_results_received());
  EXPECT_EQ(expected_result, portal_observer.captive_portal_result());

  // Check that the right tab was navigated, and there were no extra
  // navigations.
  EXPECT_EQ(1, navigation_observer.NumNavigationsForTab(
                   browser->tab_strip_model()->GetWebContentsAt(active_index)));
  EXPECT_EQ(0, NumLoadingTabs());

  // Check the tab's state, and verify no captive portal check is pending.
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser, 0));
  EXPECT_FALSE(CheckPending(browser));

  // Make sure no login tab was opened.
  EXPECT_EQ(expected_tab_count, browser->tab_strip_model()->count());
}

void CaptivePortalBrowserTest::SlowLoadBehindCaptivePortal(
    Browser* browser,
    bool expect_open_login_tab) {
  SlowLoadBehindCaptivePortal(browser,
                              expect_open_login_tab,
                              GURL(kMockHttpsUrl),
                              1,
                              1);
}

void CaptivePortalBrowserTest::SlowLoadBehindCaptivePortal(
    Browser* browser,
    bool expect_open_login_tab,
    const GURL& hanging_url,
    int expected_portal_checks,
    int expected_login_tab_navigations) {
  ASSERT_GE(expected_portal_checks, 1);
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  // Calling this on a tab that's waiting for a load to manually be timed out
  // will result in a hang.
  ASSERT_FALSE(tab_strip_model->GetActiveWebContents()->IsLoading());

  // Trigger a captive portal check quickly.
  CaptivePortalTabReloader* tab_reloader =
      GetTabReloader(tab_strip_model->GetActiveWebContents());
  ASSERT_TRUE(tab_reloader);
  SetSlowSSLLoadTime(tab_reloader, base::TimeDelta());

  // Number of tabs expected to be open after the captive portal checks
  // have completed.
  int initial_tab_count = tab_strip_model->count();
  int initial_active_index = tab_strip_model->active_index();
  int initial_loading_tabs = NumLoadingTabs();
  int expected_broken_tabs = NumBrokenTabs();
  if (CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL !=
          GetStateOfTabReloader(tab_strip_model->GetActiveWebContents())) {
    ++expected_broken_tabs;
  }

  MultiNavigationObserver navigation_observer;
  CaptivePortalObserver portal_observer(browser->profile());
  ui_test_utils::NavigateToURLWithDisposition(
      browser, hanging_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  portal_observer.WaitForResults(expected_portal_checks);

  if (expect_open_login_tab) {
    ASSERT_GE(expected_login_tab_navigations, 1);

    navigation_observer.WaitForNavigations(expected_login_tab_navigations);

    ASSERT_EQ(initial_tab_count + 1, tab_strip_model->count());
    EXPECT_EQ(initial_tab_count, tab_strip_model->active_index());

    EXPECT_EQ(expected_login_tab_navigations,
              navigation_observer.NumNavigationsForTab(
                  tab_strip_model->GetWebContentsAt(initial_tab_count)));
    EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
              GetStateOfTabReloaderAt(browser, 1));
    EXPECT_TRUE(IsLoginTab(tab_strip_model->GetWebContentsAt(1)));
  } else {
    EXPECT_EQ(0, navigation_observer.num_navigations());
    EXPECT_EQ(initial_active_index, tab_strip_model->active_index());
    ASSERT_EQ(initial_tab_count, tab_strip_model->count());
    EXPECT_EQ(initial_active_index, tab_strip_model->active_index());
  }

  // Wait for all the expect resource loads to actually start, so subsequent
  // functions can rely on them having started.
  WaitForJobs(initial_loading_tabs + 1);

  EXPECT_EQ(initial_loading_tabs + 1, NumLoadingTabs());
  EXPECT_EQ(expected_broken_tabs, NumBrokenTabs());
  EXPECT_EQ(captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
            portal_observer.captive_portal_result());
  EXPECT_EQ(expected_portal_checks, portal_observer.num_results_received());
  EXPECT_FALSE(CheckPending(browser));

  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            GetStateOfTabReloaderAt(browser, initial_active_index));

  // Reset the load time to be large, so the timer won't trigger on a reload.
  SetSlowSSLLoadTime(tab_reloader, base::TimeDelta::FromHours(1));
}

void CaptivePortalBrowserTest::FastTimeoutBehindCaptivePortal(
    Browser* browser,
    bool expect_open_login_tab) {
  FastErrorBehindCaptivePortal(browser, expect_open_login_tab,
                               GURL(kMockHttpsQuickTimeoutUrl));
}

void CaptivePortalBrowserTest::FastErrorBehindCaptivePortal(
    Browser* browser,
    bool expect_open_login_tab,
    const GURL& error_url) {
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  // Calling this on a tab that's waiting for a load to manually be timed out
  // will result in a hang.
  ASSERT_FALSE(tab_strip_model->GetActiveWebContents()->IsLoading());

  // Set the load time to be large, so the timer won't trigger.  The value is
  // not restored at the end of the function.
  CaptivePortalTabReloader* tab_reloader =
      GetTabReloader(tab_strip_model->GetActiveWebContents());
  ASSERT_TRUE(tab_reloader);
  SetSlowSSLLoadTime(tab_reloader, base::TimeDelta::FromHours(1));

  // Number of tabs expected to be open after the captive portal checks
  // have completed.
  int initial_tab_count = tab_strip_model->count();
  int initial_active_index = tab_strip_model->active_index();
  int initial_loading_tabs = NumLoadingTabs();
  int expected_broken_tabs = NumBrokenTabs();
  if (CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL !=
          GetStateOfTabReloader(tab_strip_model->GetActiveWebContents())) {
    ++expected_broken_tabs;
  }

  MultiNavigationObserver navigation_observer;
  CaptivePortalObserver portal_observer(browser->profile());
  ui_test_utils::NavigateToURLWithDisposition(
      browser, error_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  portal_observer.WaitForResults(1);

  if (expect_open_login_tab) {
    navigation_observer.WaitForNavigations(2);
    ASSERT_EQ(initial_tab_count + 1, tab_strip_model->count());
    EXPECT_EQ(initial_tab_count, tab_strip_model->active_index());
    // Make sure that the originally active tab and the captive portal tab have
    // each loaded once.
    EXPECT_EQ(1, navigation_observer.NumNavigationsForTab(
                     tab_strip_model->GetWebContentsAt(initial_active_index)));
    EXPECT_EQ(1, navigation_observer.NumNavigationsForTab(
                     tab_strip_model->GetWebContentsAt(initial_tab_count)));
    EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
              GetStateOfTabReloaderAt(browser, 1));
    EXPECT_TRUE(IsLoginTab(tab_strip_model->GetWebContentsAt(1)));
  } else {
    navigation_observer.WaitForNavigations(1);
    EXPECT_EQ(initial_active_index, tab_strip_model->active_index());
    EXPECT_EQ(1, navigation_observer.NumNavigationsForTab(
                     tab_strip_model->GetWebContentsAt(initial_active_index)));
    ASSERT_EQ(initial_tab_count, tab_strip_model->count());
    EXPECT_EQ(initial_active_index, tab_strip_model->active_index());
  }

  EXPECT_EQ(initial_loading_tabs, NumLoadingTabs());
  EXPECT_EQ(expected_broken_tabs, NumBrokenTabs());
  EXPECT_EQ(captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
            portal_observer.captive_portal_result());
  EXPECT_EQ(1, portal_observer.num_results_received());
  EXPECT_FALSE(CheckPending(browser));

  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            GetStateOfTabReloaderAt(browser, initial_active_index));
}

void CaptivePortalBrowserTest::FastErrorWithInterstitialTimer(
    Browser* browser,
    const GURL& cert_error_url) {
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  WebContents* broken_tab_contents = tab_strip_model->GetActiveWebContents();

  // Disable captive portal checks indefinitely.
  RespondToProbeRequests(false);

  SSLInterstitialTimerObserver interstitial_timer_observer(broken_tab_contents);
  ui_test_utils::NavigateToURLWithDisposition(
      browser, cert_error_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  interstitial_timer_observer.WaitForTimerStarted();

  // The tab should be in loading state, waiting for the interstitial timer to
  // expire or a captive portal result to arrive. Since captive portal checks
  // are disabled and timer set to expire after a very long time, the tab should
  // hang indefinitely.
  EXPECT_TRUE(broken_tab_contents->IsLoading());
  EXPECT_EQ(1, NumLoadingTabs());
}

void CaptivePortalBrowserTest::NavigateLoginTab(Browser* browser,
                                                int num_loading_tabs,
                                                int num_timed_out_tabs) {
  MultiNavigationObserver navigation_observer;
  CaptivePortalObserver portal_observer(browser->profile());

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  int initial_tab_count = tab_strip_model->count();
  EXPECT_EQ(num_loading_tabs, NumLoadingTabs());
  EXPECT_EQ(num_timed_out_tabs, NumBrokenTabs() - NumLoadingTabs());

  int login_tab_index = tab_strip_model->active_index();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloader(tab_strip_model->GetActiveWebContents()));
  ASSERT_TRUE(IsLoginTab(browser->tab_strip_model()->GetActiveWebContents()));

  // Do the navigation.
  content::ExecuteScriptAsync(tab_strip_model->GetActiveWebContents(),
                              "submitForm()");

  portal_observer.WaitForResults(1);
  navigation_observer.WaitForNavigations(1);

  // Check the captive portal result.
  EXPECT_EQ(captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
            portal_observer.captive_portal_result());
  EXPECT_EQ(1, portal_observer.num_results_received());
  EXPECT_FALSE(CheckPending(browser));

  // Make sure not much has changed.
  EXPECT_EQ(initial_tab_count, tab_strip_model->count());
  EXPECT_EQ(num_loading_tabs, NumLoadingTabs());
  EXPECT_EQ(num_loading_tabs + num_timed_out_tabs, NumBrokenTabs());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser, login_tab_index));
  EXPECT_TRUE(IsLoginTab(tab_strip_model->GetWebContentsAt(login_tab_index)));

  // Make sure there were no unexpected navigations.
  EXPECT_EQ(1, navigation_observer.NumNavigationsForTab(
                   tab_strip_model->GetWebContentsAt(login_tab_index)));
}

void CaptivePortalBrowserTest::Login(Browser* browser,
                                     int num_loading_tabs,
                                     int num_timed_out_tabs) {
  // Simulate logging in.
  SetBehindCaptivePortal(false);

  MultiNavigationObserver navigation_observer;
  CaptivePortalObserver portal_observer(browser->profile());

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  int initial_tab_count = tab_strip_model->count();
  ASSERT_EQ(num_loading_tabs, NumLoadingTabs());
  EXPECT_EQ(num_timed_out_tabs, NumBrokenTabs() - NumLoadingTabs());

  // Verify that the login page is on top.
  int login_tab_index = tab_strip_model->active_index();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser, login_tab_index));
  ASSERT_TRUE(IsLoginTab(tab_strip_model->GetWebContentsAt(login_tab_index)));

  // Trigger a navigation.
  content::ExecuteScriptAsync(tab_strip_model->GetActiveWebContents(),
                              "submitForm()");
  portal_observer.WaitForResults(1);

  // Wait for all the timed out tabs to reload.
  navigation_observer.WaitForNavigations(1 + num_timed_out_tabs);
  EXPECT_EQ(1, portal_observer.num_results_received());

  // The tabs that were loading before should still be loading, and now be in
  // STATE_NEEDS_RELOAD.
  EXPECT_EQ(0, NumBrokenTabs());
  EXPECT_EQ(num_loading_tabs, NumLoadingTabs());
  EXPECT_EQ(num_loading_tabs, NumNeedReloadTabs());

  // Make sure that the broken tabs have reloaded, and there's no more
  // captive portal tab.
  EXPECT_EQ(initial_tab_count, tab_strip_model->count());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser, login_tab_index));
  EXPECT_FALSE(IsLoginTab(tab_strip_model->GetWebContentsAt(login_tab_index)));

  // Make sure there were no unexpected navigations of the login tab.
  EXPECT_EQ(1, navigation_observer.NumNavigationsForTab(
                   tab_strip_model->GetWebContentsAt(login_tab_index)));
}

void CaptivePortalBrowserTest::LoginCertError(Browser* browser) {
  SetBehindCaptivePortal(false);

  MultiNavigationObserver navigation_observer;
  CaptivePortalObserver portal_observer(browser->profile());

  TabStripModel* tab_strip_model = browser->tab_strip_model();

  // Verify that the login page is on top.
  int login_tab_index = tab_strip_model->active_index();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser, login_tab_index));
  ASSERT_TRUE(IsLoginTab(tab_strip_model->GetWebContentsAt(login_tab_index)));

  // Trigger a navigation.
  content::ExecuteScriptAsync(tab_strip_model->GetActiveWebContents(),
                              "submitForm()");

  // The captive portal tab navigation will trigger a captive portal check,
  // and reloading the original tab will bring up the interstitial page again,
  // triggering a second captive portal check.
  portal_observer.WaitForResults(2);

  // Wait for both tabs to finish loading.
  navigation_observer.WaitForNavigations(2);
  EXPECT_EQ(2, portal_observer.num_results_received());
  EXPECT_FALSE(CheckPending(browser));
  EXPECT_EQ(captive_portal::RESULT_INTERNET_CONNECTED,
            portal_observer.captive_portal_result());

  // Check state of tabs.  While the first tab is still displaying an
  // interstitial page, since no portal was found, it should be in STATE_NONE,
  // as should the login tab.
  ASSERT_EQ(2, tab_strip_model->count());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser, 0));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser, login_tab_index));
  EXPECT_FALSE(IsLoginTab(tab_strip_model->GetWebContentsAt(login_tab_index)));

  // Make sure only one navigation was for the login tab.
  EXPECT_EQ(1, navigation_observer.NumNavigationsForTab(
                   tab_strip_model->GetWebContentsAt(login_tab_index)));
}

void CaptivePortalBrowserTest::FailLoadsAfterLogin(Browser* browser,
                                                   int num_loading_tabs) {
  ASSERT_EQ(num_loading_tabs, NumLoadingTabs());
  ASSERT_EQ(num_loading_tabs, NumNeedReloadTabs());
  EXPECT_EQ(0, NumBrokenTabs());

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  int initial_num_tabs = tab_strip_model->count();
  int initial_active_tab = tab_strip_model->active_index();

  CaptivePortalObserver portal_observer(browser->profile());
  FailLoadsAfterLoginObserver fail_loads_observer;
  // Connection(s) finally time out.  There should have already been a call
  // to wait for the requests to be issued before logging on.
  WaitForJobs(num_loading_tabs);
  FailJobs(num_loading_tabs);

  fail_loads_observer.WaitForNavigations();

  // No captive portal checks should have ocurred or be pending, and there
  // should be no new tabs.
  EXPECT_EQ(0, portal_observer.num_results_received());
  EXPECT_FALSE(CheckPending(browser));
  EXPECT_EQ(initial_num_tabs, tab_strip_model->count());

  EXPECT_EQ(initial_active_tab, tab_strip_model->active_index());

  EXPECT_EQ(0, NumNeedReloadTabs());
  EXPECT_EQ(0, NumLoadingTabs());
}

void CaptivePortalBrowserTest::FailLoadsWithoutLogin(Browser* browser,
                                                     int num_loading_tabs) {
  ASSERT_EQ(num_loading_tabs, NumLoadingTabs());
  ASSERT_EQ(0, NumNeedReloadTabs());
  EXPECT_EQ(num_loading_tabs, NumBrokenTabs());

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  int initial_num_tabs = tab_strip_model->count();
  int login_tab = tab_strip_model->active_index();
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloader(tab_strip_model->GetActiveWebContents()));
  ASSERT_TRUE(IsLoginTab(tab_strip_model->GetActiveWebContents()));

  CaptivePortalObserver portal_observer(browser->profile());
  MultiNavigationObserver navigation_observer;
  // Connection(s) finally time out.  There should have already been a call
  // to wait for the requests to be issued.
  FailJobs(num_loading_tabs);

  navigation_observer.WaitForNavigations(num_loading_tabs);

  // No captive portal checks should have ocurred or be pending, and there
  // should be no new tabs.
  EXPECT_EQ(0, portal_observer.num_results_received());
  EXPECT_FALSE(CheckPending(browser));
  EXPECT_EQ(initial_num_tabs, tab_strip_model->count());

  EXPECT_EQ(0, NumNeedReloadTabs());
  EXPECT_EQ(0, NumLoadingTabs());
  EXPECT_EQ(num_loading_tabs, NumBrokenTabs());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloader(tab_strip_model->GetActiveWebContents()));
  EXPECT_TRUE(IsLoginTab(tab_strip_model->GetActiveWebContents()));
  EXPECT_EQ(login_tab, tab_strip_model->active_index());

  EXPECT_EQ(0, navigation_observer.NumNavigationsForTab(
                   tab_strip_model->GetWebContentsAt(login_tab)));
}

void CaptivePortalBrowserTest::RunNavigateLoadingTabToTimeoutTest(
    Browser* browser,
    const GURL& starting_url,
    const GURL& hanging_url,
    const GURL& timeout_url) {
  // Temporarily disable the captive portal and navigate to the starting
  // URL, which may be a URL that will hang when behind a captive portal.
  SetBehindCaptivePortal(false);
  NavigateToPageExpectNoTest(browser, starting_url, 1);
  SetBehindCaptivePortal(true);

  // Go to the first hanging url.
  SlowLoadBehindCaptivePortal(browser, true, hanging_url, 1, 1);

  // Abandon the request.
  WaitForJobs(1);
  AbandonJobs(1);

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  CaptivePortalTabReloader* tab_reloader =
      GetTabReloader(tab_strip_model->GetWebContentsAt(0));
  ASSERT_TRUE(tab_reloader);

  // A non-zero delay makes it more likely that CaptivePortalTabHelper will
  // be confused by events relating to canceling the old navigation.
  SetSlowSSLLoadTime(tab_reloader, base::TimeDelta::FromSeconds(2));
  CaptivePortalObserver portal_observer(browser->profile());

  // Navigate the error tab to another slow loading page.  Can't have
  // ui_test_utils do the navigation because it will wait for loading tabs to
  // stop loading before navigating.
  //
  // This may result in either 0 or 1 DidStopLoading events.  If there is one,
  // it must happen before the CaptivePortalService sends out its test request,
  // so waiting for PortalObserver to see that request prevents it from
  // confusing the MultiNavigationObservers used later.
  tab_strip_model->ActivateTabAt(0, true);
  browser->OpenURL(content::OpenURLParams(timeout_url, content::Referrer(),
                                          WindowOpenDisposition::CURRENT_TAB,
                                          ui::PAGE_TRANSITION_TYPED, false));
  portal_observer.WaitForResults(1);
  EXPECT_FALSE(CheckPending(browser));
  EXPECT_EQ(1, NumLoadingTabs());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            GetStateOfTabReloaderAt(browser, 0));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser, 1));
  ASSERT_TRUE(IsLoginTab(tab_strip_model->GetWebContentsAt(1)));

  // Need to make sure the request has been issued before logging in.
  WaitForJobs(1);

  // Simulate logging in.
  tab_strip_model->ActivateTabAt(1, true);
  SetSlowSSLLoadTime(tab_reloader, base::TimeDelta::FromDays(1));
  Login(browser, 1, 0);

  // Timeout occurs, and page is automatically reloaded.
  FailLoadsAfterLogin(browser, 1);
}

void CaptivePortalBrowserTest::SetSlowSSLLoadTime(
    CaptivePortalTabReloader* tab_reloader,
    base::TimeDelta slow_ssl_load_time) {
  tab_reloader->set_slow_ssl_load_time(slow_ssl_load_time);
}

CaptivePortalTabReloader* CaptivePortalBrowserTest::GetTabReloader(
    WebContents* web_contents) const {
  return CaptivePortalTabHelper::FromWebContents(web_contents)->
      GetTabReloaderForTest();
}

// Make sure there's no test for a captive portal on HTTP timeouts.  This will
// also trigger the link doctor page, which results in the load of a second
// error page.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, HttpTimeout) {
  NavigateToPageExpectNoTest(browser(), GURL(kMockHttpConnectionTimeoutErr), 2);
}

// Make sure there's no check for a captive portal on HTTPS errors other than
// timeouts, when they preempt the slow load timer.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, HttpsNonTimeoutError) {
  NavigateToPageExpectNoTest(browser(), GURL(kMockHttpsConnectionUnexpectedErr),
                             1);
}

// Make sure no captive portal test triggers on HTTPS timeouts of iframes.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, HttpsIframeTimeout) {
  // Use an HTTPS server for the top level page.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());

  GURL url = https_server.GetURL(kTestServerIframeTimeoutPath);
  NavigateToPageExpectNoTest(browser(), url, 1);
}

// Check the captive portal result when the test request reports a network
// error.  The check is triggered by a slow loading page, and the page
// errors out only after getting a captive portal result.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, RequestFails) {
  SetUpCaptivePortalService(browser()->profile(),
                            GURL(kMockHttpConnectionConnectionClosedErr));
  SlowLoadNoCaptivePortal(browser(), captive_portal::RESULT_NO_RESPONSE);
}

// Same as above, but for the rather unlikely case that the connection times out
// before the timer triggers.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, RequestFailsFastTimout) {
  SetUpCaptivePortalService(browser()->profile(),
                            GURL(kMockHttpConnectionConnectionClosedErr));
  FastTimeoutNoCaptivePortal(browser(), captive_portal::RESULT_NO_RESPONSE);
}

// Checks the case that captive portal detection is disabled.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, Disabled) {
  EnableCaptivePortalDetection(browser()->profile(), false);
  SlowLoadNoCaptivePortal(browser(), captive_portal::RESULT_INTERNET_CONNECTED);
}

// Checks that we look for a captive portal on HTTPS timeouts and don't reload
// the error tab when the captive portal probe gets a 204 response, indicating
// there is no captive portal.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, InternetConnected) {
  // Can't just use SetBehindCaptivePortal(false), since then there wouldn't
  // be a timeout.
  ASSERT_TRUE(embedded_test_server()->Start());
  SetUpCaptivePortalService(browser()->profile(),
                            embedded_test_server()->GetURL("/nocontent"));
  SlowLoadNoCaptivePortal(browser(), captive_portal::RESULT_INTERNET_CONNECTED);
}

// Checks that no login page is opened when the HTTP test URL redirects to an
// SSL certificate error.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, RedirectSSLCertError) {
  // Need an HTTP TestServer to handle a dynamically created server redirect.
  ASSERT_TRUE(embedded_test_server()->Start());

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());

  GURL ssl_login_url = https_server.GetURL(kTestServerLoginPath);

  CaptivePortalService* captive_portal_service =
      CaptivePortalServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(captive_portal_service);
  SetUpCaptivePortalService(browser()->profile(),
                            embedded_test_server()->GetURL(
                                CreateServerRedirect(ssl_login_url.spec())));

  SlowLoadNoCaptivePortal(browser(), captive_portal::RESULT_NO_RESPONSE);
}

// A slow SSL load triggers a captive portal check.  The user logs on before
// the SSL page times out.  We wait for the timeout and subsequent reload.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, Login) {
  // Load starts, detect captive portal and open up a login tab.
  SlowLoadBehindCaptivePortal(browser(), true);

  // Log in.  One loading tab, no timed out ones.
  Login(browser(), 1, 0);

  // Timeout occurs, and page is automatically reloaded.
  FailLoadsAfterLogin(browser(), 1);
}

// Same as above, except we make sure everything works with an incognito
// profile.  Main issues it tests for are that the incognito has its own
// non-NULL captive portal service, and we open the tab in the correct
// window.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, LoginIncognito) {
  // This will watch tabs for both profiles, but only used to make sure no
  // navigations occur for the non-incognito profile.
  MultiNavigationObserver navigation_observer;
  CaptivePortalObserver non_incognito_portal_observer(browser()->profile());

  Browser* incognito_browser = CreateIncognitoBrowser();
  EnableCaptivePortalDetection(incognito_browser->profile(), true);
  SetUpCaptivePortalService(incognito_browser->profile(),
                            GURL(kMockCaptivePortalTestUrl));

  SlowLoadBehindCaptivePortal(incognito_browser, true);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip_model->count());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser(), 0));

  Login(incognito_browser, 1, 0);
  FailLoadsAfterLogin(incognito_browser, 1);

  EXPECT_EQ(1, tab_strip_model->count());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser(), 0));

  EXPECT_EQ(0, navigation_observer.NumNavigationsForTab(
                   tab_strip_model->GetWebContentsAt(0)));
  EXPECT_EQ(0, non_incognito_portal_observer.num_results_received());
}

// The captive portal page is opened before the SSL page times out,
// but the user logs in only after the page times out.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, LoginSlow) {
  SlowLoadBehindCaptivePortal(browser(), true);
  FailLoadsWithoutLogin(browser(), 1);
  Login(browser(), 0, 1);
}

// Checks the unlikely case that the tab times out before the timer triggers.
// This most likely won't happen, but should still work:
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, LoginFastTimeout) {
  FastTimeoutBehindCaptivePortal(browser(), true);
  Login(browser(), 0, 1);
}

// A cert error triggers a captive portal check and results in opening a login
// tab.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest,
                       ShowCaptivePortalInterstitialOnCertError) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  WebContents* broken_tab_contents = tab_strip_model->GetActiveWebContents();

  // The path does not matter.
  GURL cert_error_url = https_server.GetURL(kTestServerLoginPath);
  int cert_error_tab_index = tab_strip_model->active_index();
  // The interstitial should trigger a captive portal check when it opens, just
  // like navigating to kMockHttpsQuickTimeoutUrl.
  FastErrorBehindCaptivePortal(browser(), true, cert_error_url);
  EXPECT_EQ(CaptivePortalBlockingPage::kTypeForTesting,
            GetInterstitialType(broken_tab_contents));

  // Switch to the interstitial and click the |Connect| button. Should switch
  // active tab to the captive portal landing page.
  int login_tab_index = tab_strip_model->active_index();
  tab_strip_model->ActivateTabAt(cert_error_tab_index, false);
  // Wait for the interstitial to load all the JavaScript code. Otherwise,
  // trying to click on a button will fail.
  content::RenderFrameHost* rfh =
      broken_tab_contents->GetInterstitialPage()->GetMainFrame();
  EXPECT_TRUE(WaitForRenderFrameReady(rfh));
  const char kClickConnectButtonJS[] =
      "document.getElementById('primary-button').click();";
  {
    TabActivationWaiter tab_activation_waiter(tab_strip_model);
    content::ExecuteScriptAsync(rfh, kClickConnectButtonJS);
    tab_activation_waiter.WaitForActiveTabChange();
  }
  EXPECT_EQ(login_tab_index, tab_strip_model->active_index());

  // For completeness, close the login tab and try clicking |Connect| again.
  // A new login tab should open.
  EXPECT_EQ(1, login_tab_index);
  content::WebContentsDestroyedWatcher destroyed_watcher(
      tab_strip_model->GetActiveWebContents());
  EXPECT_TRUE(
      tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(), 0));
  destroyed_watcher.Wait();
  MultiNavigationObserver navigation_observer;
  content::ExecuteScriptAsync(rfh, kClickConnectButtonJS);
  navigation_observer.WaitForNavigations(1);
  EXPECT_EQ(login_tab_index, tab_strip_model->active_index());

  LoginCertError(browser());

  // Once logged in, broken tab should reload and display the SSL interstitial.
  WaitForInterstitialAttach(broken_tab_contents);
  tab_strip_model->ActivateTabAt(cert_error_tab_index, false);

  EXPECT_EQ(SSLBlockingPage::kTypeForTesting,
            GetInterstitialType(tab_strip_model->GetActiveWebContents()));

  // Trigger another captive portal check while the SSL interstitial is showing.
  // At this point the user is logged in to the captive portal, so the captive
  // portal interstitial shouldn't get recreated.
  CaptivePortalObserver portal_observer(browser()->profile());
  CaptivePortalService* captive_portal_service =
      CaptivePortalServiceFactory::GetForProfile(browser()->profile());
  captive_portal_service->DetectCaptivePortal();
  portal_observer.WaitForResults(1);
  EXPECT_EQ(SSLBlockingPage::kTypeForTesting,
            GetInterstitialType(broken_tab_contents));

  // A captive portal appears. Trigger a final captive portal check. The
  // captive portal interstitial should still not get recreated.
  SetBehindCaptivePortal(true);
  CaptivePortalObserver final_portal_observer(browser()->profile());
  captive_portal_service->DetectCaptivePortal();
  final_portal_observer.WaitForResults(1);
  EXPECT_EQ(SSLBlockingPage::kTypeForTesting,
            GetInterstitialType(broken_tab_contents));
}

// Tests this scenario:
// - Portal probe requests are ignored, so that no captive portal result can
//   arrive.
// - A cert error triggers an interstitial timer with a very long timeout.
// - No captive portal results arrive, causing the tab to appear as loading
//   indefinitely (because probe requests are ignored).
// - Stopping the page load shouldn't result in any interstitials.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest,
                       InterstitialTimerStopNavigationWhileLoading) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  // The path does not matter.
  GURL cert_error_url = https_server.GetURL(kTestServerLoginPath);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  WebContents* broken_tab_contents = tab_strip_model->GetActiveWebContents();

  CaptivePortalObserver portal_observer1(browser()->profile());
  FastErrorWithInterstitialTimer(browser(), cert_error_url);

  // Page appears loading. Stop the navigation. There should be no interstitial.
  MultiNavigationObserver test_navigation_observer;
  broken_tab_contents->Stop();
  test_navigation_observer.WaitForNavigations(1);

  // Make sure that the |ssl_error_handler| is deleted if page load is stopped.
  EXPECT_TRUE(nullptr == SSLErrorHandler::FromWebContents(broken_tab_contents));

  EXPECT_FALSE(broken_tab_contents->ShowingInterstitialPage());
  EXPECT_FALSE(broken_tab_contents->IsLoading());
  EXPECT_EQ(0, portal_observer1.num_results_received());
  EXPECT_EQ(0, NumLoadingTabs());
  EXPECT_FALSE(CheckPending(browser()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser(), 0));

  // Re-enable captive portal checks and fire one. The result should be ignored.
  RespondToProbeRequests(true);
  CaptivePortalObserver portal_observer2(browser()->profile());
  CaptivePortalService* captive_portal_service =
      CaptivePortalServiceFactory::GetForProfile(browser()->profile());
  captive_portal_service->DetectCaptivePortal();
  portal_observer2.WaitForResults(1);

  EXPECT_FALSE(broken_tab_contents->ShowingInterstitialPage());
  EXPECT_FALSE(broken_tab_contents->IsLoading());
  EXPECT_EQ(1, portal_observer2.num_results_received());
  EXPECT_EQ(captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
            portal_observer2.captive_portal_result());
  EXPECT_EQ(0, NumLoadingTabs());
  EXPECT_FALSE(CheckPending(browser()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser(), 0));
}

// Same as above, but instead of stopping, the loading page is reloaded. The end
// result is the same. (i.e. page load stops, no interstitials shown)
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest,
                       InterstitialTimerReloadWhileLoading) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  // The path does not matter.
  GURL cert_error_url = https_server.GetURL(kTestServerLoginPath);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  WebContents* broken_tab_contents = tab_strip_model->GetActiveWebContents();

  CaptivePortalObserver portal_observer(browser()->profile());
  FastErrorWithInterstitialTimer(browser(), cert_error_url);

  // Page appears loading. Reloading it cancels the page load. Since the load is
  // stopped, no cert error occurs and SSLErrorHandler isn't instantiated.
  MultiNavigationObserver test_navigation_observer;
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  test_navigation_observer.WaitForNavigations(1);

  // Make sure that the |ssl_error_handler| is deleted.
  EXPECT_TRUE(nullptr == SSLErrorHandler::FromWebContents(broken_tab_contents));

  EXPECT_FALSE(broken_tab_contents->ShowingInterstitialPage());
  EXPECT_FALSE(broken_tab_contents->IsLoading());
  EXPECT_EQ(0, portal_observer.num_results_received());
  EXPECT_EQ(0, NumLoadingTabs());
  EXPECT_FALSE(CheckPending(browser()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser(), 0));

  // Re-enable captive portal checks and fire one. The result should be ignored.
  RespondToProbeRequests(true);
  CaptivePortalObserver portal_observer2(browser()->profile());
  CaptivePortalService* captive_portal_service =
      CaptivePortalServiceFactory::GetForProfile(browser()->profile());
  captive_portal_service->DetectCaptivePortal();
  portal_observer2.WaitForResults(1);

  EXPECT_FALSE(broken_tab_contents->ShowingInterstitialPage());
  EXPECT_FALSE(broken_tab_contents->IsLoading());
  EXPECT_EQ(1, portal_observer2.num_results_received());
  EXPECT_EQ(captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
            portal_observer2.captive_portal_result());
  EXPECT_EQ(0, NumLoadingTabs());
  EXPECT_FALSE(CheckPending(browser()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser(), 0));
}

// Same as |InterstitialTimerReloadWhileLoading_NoSSLError|, but instead of
// reloading, the page is navigated away. The new page should load, and no
// interstitials should be shown.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest,
                       InterstitialTimerNavigateAwayWhileLoading) {
  ASSERT_TRUE(embedded_test_server()->Start());
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  // The path does not matter.
  GURL cert_error_url = https_server.GetURL(kTestServerLoginPath);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  WebContents* broken_tab_contents = tab_strip_model->GetActiveWebContents();

  CaptivePortalObserver portal_observer(browser()->profile());
  FastErrorWithInterstitialTimer(browser(), cert_error_url);

  // Page appears loading. Navigating away shouldn't result in any interstitial.
  // Can't use ui_test_utils::NavigateToURLWithDisposition because it waits for
  // a load stop notification before starting a new navigation.
  MultiNavigationObserver test_navigation_observer;
  browser()->OpenURL(content::OpenURLParams(
      embedded_test_server()->GetURL("/title2.html"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
  test_navigation_observer.WaitForNavigations(1);

  // Make sure that the |ssl_error_handler| is deleted.
  EXPECT_TRUE(nullptr == SSLErrorHandler::FromWebContents(broken_tab_contents));

  EXPECT_FALSE(broken_tab_contents->ShowingInterstitialPage());
  EXPECT_FALSE(broken_tab_contents->IsLoading());
  EXPECT_EQ(0, portal_observer.num_results_received());
  EXPECT_EQ(0, NumLoadingTabs());
  EXPECT_FALSE(CheckPending(browser()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser(), 0));

  // Re-enable captive portal checks and fire one. The result should be ignored.
  RespondToProbeRequests(true);
  CaptivePortalObserver portal_observer2(browser()->profile());
  CaptivePortalService* captive_portal_service =
      CaptivePortalServiceFactory::GetForProfile(browser()->profile());
  captive_portal_service->DetectCaptivePortal();
  portal_observer2.WaitForResults(1);

  EXPECT_FALSE(broken_tab_contents->ShowingInterstitialPage());
  EXPECT_FALSE(broken_tab_contents->IsLoading());
  EXPECT_EQ(1, portal_observer2.num_results_received());
  EXPECT_EQ(captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
            portal_observer2.captive_portal_result());
  EXPECT_EQ(0, NumLoadingTabs());
  EXPECT_FALSE(CheckPending(browser()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser(), 0));
}

// Same as above, but the hanging load is interrupted by a navigation to the
// same page, this time committing the navigation. This should end up with an
// SSL interstitial when not behind a captive portal. This ensures that a new
// |SSLErrorHandler| is created on a new navigation, even though the tab's
// WebContents doesn't change.
IN_PROC_BROWSER_TEST_F(
    CaptivePortalBrowserTest,
    InterstitialTimerNavigateWhileLoading_EndWithSSLInterstitial) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  // The path does not matter.
  GURL cert_error_url = https_server.GetURL(kTestServerLoginPath);
  SetBehindCaptivePortal(false);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  WebContents* broken_tab_contents = tab_strip_model->GetActiveWebContents();

  FastErrorWithInterstitialTimer(browser(), cert_error_url);
  // Page appears loading. Turn on response to probe request again, and navigate
  // to the same page. This should result in a cert error which should
  // instantiate an |SSLErrorHandler| and end up showing an SSL interstitial.
  RespondToProbeRequests(true);
  // Can't have ui_test_utils do the navigation because it will wait for loading
  // tabs to stop loading before navigating.
  CaptivePortalObserver portal_observer(browser()->profile());
  MultiNavigationObserver test_navigation_observer;
  browser()->OpenURL(content::OpenURLParams(cert_error_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));
  test_navigation_observer.WaitForNavigations(1);
  // Should end up with an SSL interstitial.
  WaitForInterstitialAttach(broken_tab_contents);
  ASSERT_TRUE(broken_tab_contents->ShowingInterstitialPage());
  EXPECT_EQ(SSLBlockingPage::kTypeForTesting,
            broken_tab_contents->GetInterstitialPage()
                ->GetDelegateForTesting()
                ->GetTypeForTesting());
  EXPECT_FALSE(broken_tab_contents->IsLoading());
  EXPECT_EQ(1, portal_observer.num_results_received());
  EXPECT_EQ(captive_portal::RESULT_INTERNET_CONNECTED,
            portal_observer.captive_portal_result());
  EXPECT_EQ(0, NumLoadingTabs());
  EXPECT_FALSE(CheckPending(browser()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser(), 0));
}

// Same as above, but this time behind a captive portal.
IN_PROC_BROWSER_TEST_F(
    CaptivePortalBrowserTest,
    InterstitialTimerNavigateWhileLoading_EndWithCaptivePortalInterstitial) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  // The path does not matter.
  GURL cert_error_url = https_server.GetURL(kTestServerLoginPath);
  SetBehindCaptivePortal(true);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  WebContents* broken_tab_contents = tab_strip_model->GetActiveWebContents();
  int initial_tab_count = tab_strip_model->count();

  FastErrorWithInterstitialTimer(browser(), cert_error_url);
  // Page appears loading. Turn on response to probe request again, and navigate
  // to the same page. This should result in a cert error which should
  // instantiate an |SSLErrorHandler| and end up showing an SSL.
  RespondToProbeRequests(true);
  // Can't have ui_test_utils do the navigation because it will wait for loading
  // tabs to stop loading before navigating.
  CaptivePortalObserver portal_observer(browser()->profile());
  MultiNavigationObserver test_navigation_observer;
  browser()->OpenURL(content::OpenURLParams(cert_error_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));
  // Expect two navigations:
  // 1- For completing the load of the above navigation.
  // 2- For completing the load of the login tab.
  test_navigation_observer.WaitForNavigations(2);
  // Should end up with a captive portal interstitial and a new login tab.
  WaitForInterstitialAttach(broken_tab_contents);
  ASSERT_TRUE(broken_tab_contents->ShowingInterstitialPage());
  EXPECT_EQ(CaptivePortalBlockingPage::kTypeForTesting,
            broken_tab_contents->GetInterstitialPage()
                ->GetDelegateForTesting()
                ->GetTypeForTesting());
  ASSERT_EQ(initial_tab_count + 1, tab_strip_model->count());
  EXPECT_EQ(initial_tab_count, tab_strip_model->active_index());
  EXPECT_FALSE(broken_tab_contents->IsLoading());
  EXPECT_EQ(1, portal_observer.num_results_received());
  EXPECT_EQ(captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
            portal_observer.captive_portal_result());
  EXPECT_EQ(0, NumLoadingTabs());
  EXPECT_FALSE(CheckPending(browser()));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            GetStateOfTabReloaderAt(browser(), 0));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser(), 1));
  EXPECT_TRUE(IsLoginTab(tab_strip_model->GetWebContentsAt(1)));
}

// A cert error triggers a captive portal check and results in opening a login
// tab.  The user then logs in and the page with the error is reloaded.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, SSLCertErrorLogin) {
  // Need an HTTP TestServer to handle a dynamically created server redirect.
  ASSERT_TRUE(embedded_test_server()->Start());

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());

  // Set SSL interstitial delay to zero so that a captive portal result can not
  // arrive during this window, so an SSL interstitial is displayed instead
  // of a captive portal error page.
  SSLErrorHandler::SetInterstitialDelayForTesting(base::TimeDelta());
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  WebContents* broken_tab_contents = tab_strip_model->GetActiveWebContents();

  // The path does not matter.
  GURL cert_error_url = https_server.GetURL(kTestServerLoginPath);
  // A captive portal check is triggered in FastErrorBehindCaptivePortal.
  FastErrorBehindCaptivePortal(browser(), true, cert_error_url);

  EXPECT_EQ(SSLBlockingPage::kTypeForTesting,
            GetInterstitialType(broken_tab_contents));

  LoginCertError(browser());
}

// Tries navigating both the tab that encounters an SSL timeout and the
// login tab twice, only logging in the second time.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, LoginExtraNavigations) {
  FastTimeoutBehindCaptivePortal(browser(), true);

  // Activate the timed out tab and navigate it to a timeout again.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->ActivateTabAt(0, true);
  FastTimeoutBehindCaptivePortal(browser(), false);

  // Activate and navigate the captive portal tab.  This should not trigger a
  // reload of the tab with the error.
  tab_strip_model->ActivateTabAt(1, true);
  NavigateLoginTab(browser(), 0, 1);

  // Simulate logging in.
  Login(browser(), 0, 1);
}

// After the first SSL timeout, closes the login tab and makes sure it's opened
// it again on a second timeout.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, CloseLoginTab) {
  // First load starts, opens a login tab, and then times out.
  SlowLoadBehindCaptivePortal(browser(), true);
  FailLoadsWithoutLogin(browser(), 1);

  // Close login tab.
  chrome::CloseTab(browser());

  // Go through the standard slow load login, and make sure it still works.
  SlowLoadBehindCaptivePortal(browser(), true);
  Login(browser(), 1, 0);
  FailLoadsAfterLogin(browser(), 1);
}

// Checks that two tabs with SSL timeouts in the same window work.  Both
// tabs only timeout after logging in.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, TwoBrokenTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SlowLoadBehindCaptivePortal(browser(), true);

  // Can't set the TabReloader HTTPS timeout on a new tab without doing some
  // acrobatics, so open a new tab at a normal page, and then navigate it to a
  // timeout.
  MultiNavigationObserver navigation_observer;
  CaptivePortalObserver portal_observer(browser()->profile());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/title2.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(3, tab_strip_model->count());
  EXPECT_FALSE(CheckPending(browser()));
  EXPECT_EQ(0, portal_observer.num_results_received());
  EXPECT_EQ(1, NumLoadingTabs());
  EXPECT_EQ(1, navigation_observer.num_navigations());
  EXPECT_EQ(1, navigation_observer.NumNavigationsForTab(
                   tab_strip_model->GetWebContentsAt(2)));
  ASSERT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            GetStateOfTabReloaderAt(browser(), 0));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser(), 1));
  ASSERT_TRUE(IsLoginTab(tab_strip_model->GetWebContentsAt(1)));
  ASSERT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser(), 2));
  ASSERT_EQ(2, tab_strip_model->active_index());

  SlowLoadBehindCaptivePortal(browser(), false);

  tab_strip_model->ActivateTabAt(1, true);
  Login(browser(), 2, 0);
  FailLoadsAfterLogin(browser(), 2);
}

IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, AbortLoad) {
  SlowLoadBehindCaptivePortal(browser(), true);

  // Abandon the request.
  WaitForJobs(1);
  AbandonJobs(1);

  CaptivePortalObserver portal_observer(browser()->profile());
  MultiNavigationObserver navigation_observer;

  // Switch back to the hung tab from the login tab, and abort the navigation.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->ActivateTabAt(0, true);
  chrome::Stop(browser());
  navigation_observer.WaitForNavigations(1);

  EXPECT_EQ(0, NumBrokenTabs());
  EXPECT_EQ(0, portal_observer.num_results_received());
  EXPECT_FALSE(CheckPending(browser()));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser(), 0));

  tab_strip_model->ActivateTabAt(1, true);
  Login(browser(), 0, 0);
}

// Checks the case where the timed out tab is successfully navigated before
// logging in.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, NavigateBrokenTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Go to the error page.
  SlowLoadBehindCaptivePortal(browser(), true);
  FailLoadsWithoutLogin(browser(), 1);

  // Navigate the error tab to a non-error page.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->ActivateTabAt(0, true);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title2.html"));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser(), 0));

  // Simulate logging in.
  tab_strip_model->ActivateTabAt(1, true);
  Login(browser(), 0, 0);
}

// Checks that captive portal detection triggers correctly when a same-site
// navigation is cancelled by a navigation to the same site.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest,
                       NavigateLoadingTabToTimeoutSingleSite) {
  RunNavigateLoadingTabToTimeoutTest(
      browser(),
      GURL(kMockHttpsUrl),
      GURL(kMockHttpsUrl),
      GURL(kMockHttpsUrl));
}

// Fails on Windows only, mostly on Win7. http://crbug.com/170033
#if defined(OS_WIN)
#define MAYBE_NavigateLoadingTabToTimeoutTwoSites \
        DISABLED_NavigateLoadingTabToTimeoutTwoSites
#else
#define MAYBE_NavigateLoadingTabToTimeoutTwoSites \
        NavigateLoadingTabToTimeoutTwoSites
#endif

// Checks that captive portal detection triggers correctly when a same-site
// navigation is cancelled by a navigation to another site.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest,
                       MAYBE_NavigateLoadingTabToTimeoutTwoSites) {
  RunNavigateLoadingTabToTimeoutTest(
      browser(),
      GURL(kMockHttpsUrl),
      GURL(kMockHttpsUrl),
      GURL(kMockHttpsUrl2));
}

// Checks that captive portal detection triggers correctly when a cross-site
// navigation is cancelled by a navigation to yet another site.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest,
                       NavigateLoadingTabToTimeoutThreeSites) {
  ASSERT_TRUE(embedded_test_server()->Start());
  RunNavigateLoadingTabToTimeoutTest(
      browser(), embedded_test_server()->GetURL("/title1.html"),
      GURL(kMockHttpsUrl), GURL(kMockHttpsUrl2));
}

// Checks that navigating a timed out tab back clears its state.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, GoBack) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Navigate to a working page.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title2.html"));

  // Go to the error page.
  SlowLoadBehindCaptivePortal(browser(), true);
  FailLoadsWithoutLogin(browser(), 1);

  CaptivePortalObserver portal_observer(browser()->profile());
  MultiNavigationObserver navigation_observer;

  // Activate the error page tab again and go back.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->ActivateTabAt(0, true);
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  navigation_observer.WaitForNavigations(1);

  EXPECT_EQ(1, navigation_observer.NumNavigationsForTab(
                   tab_strip_model->GetWebContentsAt(0)));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser(), 0));
  EXPECT_EQ(0, portal_observer.num_results_received());
}

// Checks that navigating back to a timeout triggers captive portal detection.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, GoBackToTimeout) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Disable captive portal detection so the first navigation doesn't open a
  // login tab.
  EnableCaptivePortalDetection(browser()->profile(), false);

  SlowLoadNoCaptivePortal(browser(), captive_portal::RESULT_INTERNET_CONNECTED);

  // Navigate to a working page.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title2.html"));
  ASSERT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser(), 0));

  EnableCaptivePortalDetection(browser()->profile(), true);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  CaptivePortalTabReloader* tab_reloader =
      GetTabReloader(tab_strip_model->GetActiveWebContents());
  ASSERT_TRUE(tab_reloader);
  SetSlowSSLLoadTime(tab_reloader, base::TimeDelta());

  // Go to the error page.
  MultiNavigationObserver navigation_observer;
  CaptivePortalObserver portal_observer(browser()->profile());
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);

  // Wait for the check triggered by the broken tab and for the login tab to
  // stop loading.
  portal_observer.WaitForResults(1);
  navigation_observer.WaitForNavigations(1);
  // Make sure the request has been issued.
  WaitForJobs(1);

  EXPECT_EQ(1, portal_observer.num_results_received());
  ASSERT_FALSE(CheckPending(browser()));
  ASSERT_EQ(captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
            portal_observer.captive_portal_result());

  ASSERT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            GetStateOfTabReloaderAt(browser(), 0));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser(), 1));
  ASSERT_TRUE(IsLoginTab(browser()->tab_strip_model()->GetWebContentsAt(1)));

  ASSERT_EQ(2, tab_strip_model->count());
  EXPECT_EQ(1, tab_strip_model->active_index());
  EXPECT_EQ(1, navigation_observer.NumNavigationsForTab(
                   tab_strip_model->GetWebContentsAt(1)));
  EXPECT_EQ(1, NumLoadingTabs());

  SetSlowSSLLoadTime(tab_reloader, base::TimeDelta::FromDays(1));
  Login(browser(), 1, 0);
  FailLoadsAfterLogin(browser(), 1);
}

// Checks that reloading a timeout triggers captive portal detection.
// Much like the last test, though the captive portal is disabled before
// the inital navigation, rather than captive portal detection.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, ReloadTimeout) {
  SetBehindCaptivePortal(false);

  // Do the first navigation while not behind a captive portal.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  CaptivePortalObserver portal_observer(browser()->profile());
  ui_test_utils::NavigateToURL(browser(), GURL(kMockHttpsUrl));
  ASSERT_EQ(0, portal_observer.num_results_received());
  ASSERT_EQ(1, tab_strip_model->count());

  // A captive portal spontaneously appears.
  SetBehindCaptivePortal(true);

  CaptivePortalTabReloader* tab_reloader =
      GetTabReloader(tab_strip_model->GetActiveWebContents());
  ASSERT_TRUE(tab_reloader);
  SetSlowSSLLoadTime(tab_reloader, base::TimeDelta());

  MultiNavigationObserver navigation_observer;
  tab_strip_model->GetActiveWebContents()->GetController().Reload(
      content::ReloadType::NORMAL, true);

  // Wait for the check triggered by the broken tab and for the login tab to
  // stop loading.
  portal_observer.WaitForResults(1);
  navigation_observer.WaitForNavigations(1);
  // Make sure the request has been issued.
  WaitForJobs(1);

  ASSERT_EQ(1, portal_observer.num_results_received());
  ASSERT_FALSE(CheckPending(browser()));
  ASSERT_EQ(captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
            portal_observer.captive_portal_result());

  ASSERT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            GetStateOfTabReloaderAt(browser(), 0));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(browser(), 1));
  ASSERT_TRUE(IsLoginTab(tab_strip_model->GetWebContentsAt(1)));

  ASSERT_EQ(2, tab_strip_model->count());
  EXPECT_EQ(1, tab_strip_model->active_index());
  EXPECT_EQ(1, navigation_observer.NumNavigationsForTab(
                   tab_strip_model->GetWebContentsAt(1)));
  EXPECT_EQ(1, NumLoadingTabs());

  SetSlowSSLLoadTime(tab_reloader, base::TimeDelta::FromDays(1));
  Login(browser(), 1, 0);
  FailLoadsAfterLogin(browser(), 1);
}

// Checks the case where there are two windows, and there's an SSL timeout in
// the background one.
// Disabled:  http://crbug.com/134357
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, DISABLED_TwoWindows) {
  Browser* browser2 =
      new Browser(Browser::CreateParams(browser()->profile(), true));
  // Navigate the new browser window so it'll be shown and we can pick the
  // active window.
  ui_test_utils::NavigateToURL(browser2, GURL(url::kAboutBlankURL));

  // Generally, |browser2| will be the active window.  However, if the
  // original browser window lost focus before creating the new one, such as
  // when running multiple tests at once, the original browser window may
  // remain the profile's active window.
  Browser* active_browser =
      chrome::FindTabbedBrowser(browser()->profile(), true);
  Browser* inactive_browser;
  if (active_browser == browser2) {
    // When only one test is running at a time, the new browser will probably be
    // on top, but when multiple tests are running at once, this is not
    // guaranteed.
    inactive_browser = browser();
  } else {
    ASSERT_EQ(active_browser, browser());
    inactive_browser = browser2;
  }

  CaptivePortalObserver portal_observer(browser()->profile());
  MultiNavigationObserver navigation_observer;

  // Navigate the tab in the inactive browser to an SSL timeout.  Have to use
  // NavigateParams and NEW_BACKGROUND_TAB to avoid activating the window.
  NavigateParams params(inactive_browser, GURL(kMockHttpsQuickTimeoutUrl),
                        ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  params.window_action = NavigateParams::NO_ACTION;
  ui_test_utils::NavigateToURL(&params);
  navigation_observer.WaitForNavigations(2);

  // Make sure the active window hasn't changed, and its new tab is
  // active.
  ASSERT_EQ(active_browser,
            chrome::FindTabbedBrowser(browser()->profile(), true));
  ASSERT_EQ(1, active_browser->tab_strip_model()->active_index());

  // Check that the only two navigated tabs were the new error tab in the
  // backround windows, and the login tab in the active window.
  EXPECT_EQ(1, navigation_observer.NumNavigationsForTab(
                   inactive_browser->tab_strip_model()->GetWebContentsAt(1)));
  EXPECT_EQ(1, navigation_observer.NumNavigationsForTab(
                   active_browser->tab_strip_model()->GetWebContentsAt(1)));
  EXPECT_EQ(0, NumLoadingTabs());

  // Check captive portal test results.
  portal_observer.WaitForResults(1);
  ASSERT_EQ(captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL,
            portal_observer.captive_portal_result());
  EXPECT_EQ(1, portal_observer.num_results_received());

  // Check the inactive browser.
  EXPECT_EQ(2, inactive_browser->tab_strip_model()->count());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(inactive_browser, 0));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_BROKEN_BY_PORTAL,
            GetStateOfTabReloaderAt(inactive_browser, 1));

  // Check the active browser.
  ASSERT_EQ(2, active_browser->tab_strip_model()->count());
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(active_browser, 0));
  EXPECT_EQ(CaptivePortalTabReloader::STATE_NONE,
            GetStateOfTabReloaderAt(active_browser, 1));
  EXPECT_TRUE(
      IsLoginTab(active_browser->tab_strip_model()->GetWebContentsAt(1)));

  // Simulate logging in.
  Login(active_browser, 0, 1);
}

// An HTTP page redirects to an HTTPS page loads slowly before timing out.  A
// captive portal is found, and then the user logs in before the original page
// times out.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, HttpToHttpsRedirectLogin) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SlowLoadBehindCaptivePortal(
      browser(), true, embedded_test_server()->GetURL(kRedirectToMockHttpsPath),
      1, 1);
  Login(browser(), 1, 0);
  FailLoadsAfterLogin(browser(), 1);
}

// An HTTPS page redirects to an HTTP page.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, HttpsToHttpRedirect) {
  // Use an HTTPS server for the top level page.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  // The redirect points to a non-existant host, instead of using a
  // http://mock.failed.request URL, because with the network service enabled if
  // the initial URL doesn't go through URLLoaderInterceptor (because it's
  // served by the EmbeddedTestServer), then URLLoaderInterceptor (which is what
  // handles mock.failed.request URLs) wouldn't see the redirect.
  GURL http_error_url("http://doesnt.exist/");
  NavigateToPageExpectNoTest(
      browser(),
      https_server.GetURL(CreateServerRedirect(http_error_url.spec())), 1);
}

// Tests the 511 response code, along with an HTML redirect to a login page.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest, Status511) {
  SetUpCaptivePortalService(browser()->profile(),
                            GURL(kMockCaptivePortal511Url));
  SlowLoadBehindCaptivePortal(browser(), true, GURL(kMockHttpsUrl), 2, 2);
  Login(browser(), 1, 0);
  FailLoadsAfterLogin(browser(), 1);
}

// A slow SSL load starts. The reloader triggers a captive portal check, finds a
// captive portal. The SSL commits with a cert error, triggering another captive
// portal check.
// The second check finds no captive portal. The reloader triggers a reload at
// the same time SSL error handler tries to show an interstitial. Should result
// in an SSL interstitial.
IN_PROC_BROWSER_TEST_F(CaptivePortalBrowserTest,
                       InterstitialTimerCertErrorAfterSlowLoad) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);

  GURL cert_error_url;
  // With the network service, the request must be handled in the network
  // process as that's what triggers the NetworkServiceClient methods that
  // call out to SSLManager.
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  cert_error_url = https_server.GetURL(kMockHttpsBadCertPath);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  int broken_tab_index = tab_strip_model->active_index();
  WebContents* broken_tab_contents = tab_strip_model->GetActiveWebContents();
  SlowLoadBehindCaptivePortal(browser(), true, cert_error_url, 1, 1);

  // No longer behind a captive portal. Committing the SSL page should trigger
  // an SSL interstitial which triggers a new captive portal check. Since there
  // is no captive portal anymore, should end up with an SSL interstitial.
  SetBehindCaptivePortal(false);

  CaptivePortalObserver portal_observer(browser()->profile());
  MultiNavigationObserver navigation_observer;
  net::SSLInfo info;
  info.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;
  info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  info.unverified_cert = info.cert;
  FailJobsWithCertError(1, info);
  navigation_observer.WaitForNavigations(1);

  EXPECT_EQ(CaptivePortalTabReloader::STATE_NEEDS_RELOAD,
            GetStateOfTabReloaderAt(browser(), broken_tab_index));

  WaitForInterstitialAttach(broken_tab_contents);
  portal_observer.WaitForResults(1);

  EXPECT_EQ(SSLBlockingPage::kTypeForTesting,
            GetInterstitialType(broken_tab_contents));
}
