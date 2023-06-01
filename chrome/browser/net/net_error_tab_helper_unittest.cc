// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_error_tab_helper.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/error_page/common/net_error_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#undef NO_ERROR  // Defined in winerror.h.

using chrome_browser_net::NetErrorTabHelper;
using error_page::DnsProbeStatus;

class TestNetErrorTabHelper : public NetErrorTabHelper {
 public:
  explicit TestNetErrorTabHelper(content::WebContents* web_contents)
      : NetErrorTabHelper(web_contents),
        mock_probe_running_(false),
        last_status_sent_(error_page::DNS_PROBE_MAX),
        mock_sent_count_(0),
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
        times_download_page_later_invoked_(0),
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
        times_diagnostics_dialog_invoked_(0) {
  }

  void FinishProbe(DnsProbeStatus status) {
    EXPECT_TRUE(mock_probe_running_);
    OnDnsProbeFinished(status);
    mock_probe_running_ = false;
  }

  bool mock_probe_running() const { return mock_probe_running_; }
  DnsProbeStatus last_status_sent() const { return last_status_sent_; }
  int mock_sent_count() const { return mock_sent_count_; }

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  using NetErrorTabHelper::DownloadPageLater;

  const GURL& download_page_later_url() const {
    return download_page_later_url_;
  }

  int times_download_page_later_invoked() const {
    return times_download_page_later_invoked_;
  }
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

  const std::string& network_diagnostics_url() const {
    return network_diagnostics_url_;
  }

  int times_diagnostics_dialog_invoked() const {
    return times_diagnostics_dialog_invoked_;
  }

  void SetCurrentTargetFrame(content::RenderFrameHost* render_frame_host) {
    network_diagnostics_receivers_for_testing().SetCurrentTargetFrameForTesting(
        render_frame_host);
  }

  chrome::mojom::NetworkDiagnostics* network_diagnostics_interface() {
    return this;
  }

 private:
  // NetErrorTabHelper implementation:

  void StartDnsProbe() override {
    EXPECT_FALSE(mock_probe_running_);
    mock_probe_running_ = true;
  }

  void SendInfo() override {
    last_status_sent_ = dns_probe_status();
    mock_sent_count_++;
  }

  void RunNetworkDiagnosticsHelper(const std::string& sanitized_url) override {
    network_diagnostics_url_ = sanitized_url;
    times_diagnostics_dialog_invoked_++;
  }

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  void DownloadPageLaterHelper(const GURL& url) override {
    download_page_later_url_ = url;
    times_download_page_later_invoked_++;
  }
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

  bool mock_probe_running_;
  DnsProbeStatus last_status_sent_;
  int mock_sent_count_;
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  GURL download_page_later_url_;
  int times_download_page_later_invoked_;
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
  std::string network_diagnostics_url_;
  int times_diagnostics_dialog_invoked_;
};

class NetErrorTabHelperTest : public ChromeRenderViewHostTestHarness {
 protected:
  enum MainFrame { SUB_FRAME, MAIN_FRAME };
  enum ErrorType { DNS_ERROR, OTHER_ERROR, NO_ERROR };

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // This will simulate the initialization of the RenderFrame in the renderer
    // process. This is needed because WebContents does not initialize a
    // RenderFrame on construction, and the tests expect one to exist.
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();

    subframe_ = content::RenderFrameHostTester::For(main_rfh())
                    ->AppendChild("subframe");

    tab_helper_ = std::make_unique<TestNetErrorTabHelper>(web_contents());
    NetErrorTabHelper::set_state_for_testing(
        NetErrorTabHelper::TESTING_FORCE_ENABLED);
  }

  void TearDown() override {
    // Have to shut down the helper before the profile.
    tab_helper_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void DidFinishNavigation(MainFrame main_frame,
                           ErrorType error_type) {
    net::Error net_error = net::OK;
    if (error_type == DNS_ERROR)
      net_error = net::ERR_NAME_NOT_RESOLVED;
    else
      net_error = net::ERR_TIMED_OUT;
    content::MockNavigationHandle navigation_handle(
        bogus_url_, (main_frame == MAIN_FRAME) ? main_rfh() : subframe_.get());
    navigation_handle.set_is_in_primary_main_frame(main_frame == MAIN_FRAME);
    navigation_handle.set_net_error_code(net_error);
    navigation_handle.set_has_committed(true);
    navigation_handle.set_is_error_page(true);
    tab_helper_->DidFinishNavigation(&navigation_handle);
  }

  void FinishProbe(DnsProbeStatus status) { tab_helper_->FinishProbe(status); }

  void LoadURL(const GURL& url, bool succeeded) {
    if (succeeded) {
      content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                                 url);
    } else {
      content::NavigationSimulator::NavigateAndFailFromBrowser(
          web_contents(), url, net::ERR_TIMED_OUT);
    }
  }

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  void NoDownloadPageLaterForNonHttpSchemes(const char* url_string,
                                            bool succeeded) {
    GURL url(url_string);
    LoadURL(url, succeeded);
    tab_helper()->DownloadPageLater();
    EXPECT_EQ(0, tab_helper()->times_download_page_later_invoked());
  }
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

  bool probe_running() { return tab_helper_->mock_probe_running(); }
  DnsProbeStatus last_status_sent() { return tab_helper_->last_status_sent(); }
  int sent_count() { return tab_helper_->mock_sent_count(); }

  TestNetErrorTabHelper* tab_helper() { return tab_helper_.get(); }

 private:
  raw_ptr<content::RenderFrameHost, DanglingUntriaged> subframe_;
  std::unique_ptr<TestNetErrorTabHelper> tab_helper_;
  GURL bogus_url_;
};

TEST_F(NetErrorTabHelperTest, Null) {
  EXPECT_FALSE(probe_running());
}

TEST_F(NetErrorTabHelperTest, MainFrameNonDnsError) {
  DidFinishNavigation(MAIN_FRAME, OTHER_ERROR);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(0, sent_count());
}

TEST_F(NetErrorTabHelperTest, NonMainFrameDnsError) {
  DidFinishNavigation(SUB_FRAME, DNS_ERROR);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(0, sent_count());
}

// Test complete DNS error page loads.  Note that the helper can see two error
// page loads: Link Doctor loads an empty HTML page so the user knows something
// is going on, then fails over to the normal error page if and when Link
// Doctor fails to load or declines to provide a page.

TEST_F(NetErrorTabHelperTest, ProbeResponse) {
  DidFinishNavigation(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(1, sent_count());

  FinishProbe(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(2, sent_count());

  DidFinishNavigation(MAIN_FRAME, NO_ERROR);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(3, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_FINISHED_NXDOMAIN, last_status_sent());
}

// Send result even if a new page load has started; the error page is still
// visible, and the user might cancel the load.
TEST_F(NetErrorTabHelperTest, ProbeResponseAfterNewStart) {
  DidFinishNavigation(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(1, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_STARTED, last_status_sent());

  DidFinishNavigation(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(2, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_STARTED, last_status_sent());

  DidFinishNavigation(MAIN_FRAME, NO_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(3, sent_count());

  FinishProbe(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(4, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_FINISHED_NXDOMAIN, last_status_sent());
}

// Don't send result if a new page has committed; the result would go to the
// wrong page, and the error page is gone anyway.
TEST_F(NetErrorTabHelperTest, ProbeResponseAfterNewCommit) {
  DidFinishNavigation(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(1, sent_count());

  DidFinishNavigation(MAIN_FRAME, NO_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(2, sent_count());

  FinishProbe(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(3, sent_count());
}

TEST_F(NetErrorTabHelperTest, MultipleDnsErrorsWithProbesWithoutErrorPages) {
  DidFinishNavigation(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(1, sent_count());

  FinishProbe(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(2, sent_count());

  DidFinishNavigation(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(3, sent_count());

  FinishProbe(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(4, sent_count());
}

TEST_F(NetErrorTabHelperTest, MultipleDnsErrorsWithProbesAndErrorPages) {
  DidFinishNavigation(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(1, sent_count());

  DidFinishNavigation(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(2, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_STARTED, last_status_sent());

  FinishProbe(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(3, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_FINISHED_NXDOMAIN, last_status_sent());

  DidFinishNavigation(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(4, sent_count());

  DidFinishNavigation(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(5, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_STARTED, last_status_sent());

  FinishProbe(error_page::DNS_PROBE_FINISHED_NO_INTERNET);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(6, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_FINISHED_NO_INTERNET,
            last_status_sent());
}

// If multiple DNS errors occur in a row before a probe result, don't start
// multiple probes.
TEST_F(NetErrorTabHelperTest, CoalesceFailures) {
  DidFinishNavigation(MAIN_FRAME, DNS_ERROR);
  DidFinishNavigation(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(2, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_STARTED, last_status_sent());

  DidFinishNavigation(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(3, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_STARTED, last_status_sent());

  DidFinishNavigation(MAIN_FRAME, DNS_ERROR);
  EXPECT_TRUE(probe_running());
  EXPECT_EQ(4, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_STARTED, last_status_sent());

  FinishProbe(error_page::DNS_PROBE_FINISHED_NXDOMAIN);
  EXPECT_FALSE(probe_running());
  EXPECT_EQ(5, sent_count());
  EXPECT_EQ(error_page::DNS_PROBE_FINISHED_NXDOMAIN, last_status_sent());
}

// Makes sure that URLs are sanitized before running the platform network
// diagnostics tool.
TEST_F(NetErrorTabHelperTest, SanitizeDiagnosticsUrl) {
  tab_helper()->SetCurrentTargetFrame(web_contents()->GetPrimaryMainFrame());
  tab_helper()->network_diagnostics_interface()->RunNetworkDiagnostics(
      GURL("http://foo:bar@somewhere:123/hats?for#goats"));
  EXPECT_EQ("http://somewhere:123/",
            tab_helper()->network_diagnostics_url());
  EXPECT_EQ(1, tab_helper()->times_diagnostics_dialog_invoked());
}

// Makes sure that diagnostics aren't run on invalid URLs or URLs with
// non-HTTP/HTTPS schemes.
TEST_F(NetErrorTabHelperTest, NoDiagnosticsForNonHttpSchemes) {
  const char* kUrls[] = {
    "",
    "http",
    "file:///blah/blah",
    "chrome://blah/",
    "about:blank",
    "file://foo/bar",
  };

  for (const char* url : kUrls) {
    tab_helper()->SetCurrentTargetFrame(web_contents()->GetPrimaryMainFrame());
    tab_helper()->network_diagnostics_interface()
        ->RunNetworkDiagnostics(GURL(url));
    EXPECT_EQ(0, tab_helper()->times_diagnostics_dialog_invoked());
  }
}

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
TEST_F(NetErrorTabHelperTest, DownloadPageLater) {
  GURL url("http://somewhere:123/");
  LoadURL(url, false /*succeeded*/);
  tab_helper()->DownloadPageLater();
  EXPECT_EQ(url, tab_helper()->download_page_later_url());
  EXPECT_EQ(1, tab_helper()->times_download_page_later_invoked());
}

TEST_F(NetErrorTabHelperTest, NoDownloadPageLaterOnNonErrorPage) {
  GURL url("http://somewhere:123/");
  LoadURL(url, true /*succeeded*/);
  tab_helper()->DownloadPageLater();
  EXPECT_EQ(0, tab_helper()->times_download_page_later_invoked());
}

// Makes sure that "Download page later" isn't run on URLs with non-HTTP/HTTPS
// schemes.
// NOTE: the test harness code in this file and in TestRendererHost don't always
// deal with pending RFH correctly. This works because most tests only load
// once. So workaround it by puting each test case in a separate test.
TEST_F(NetErrorTabHelperTest, NoDownloadPageLaterForNonHttpSchemes1) {
  NoDownloadPageLaterForNonHttpSchemes("file:///blah/blah", false);
}

TEST_F(NetErrorTabHelperTest, NoDownloadPageLaterForNonHttpSchemes2) {
  NoDownloadPageLaterForNonHttpSchemes("chrome://blah/", false);
}

TEST_F(NetErrorTabHelperTest, NoDownloadPageLaterForNonHttpSchemes3) {
  // about:blank always succeeds, and the test harness won't handle URLs that
  // don't go to the network failing.
  NoDownloadPageLaterForNonHttpSchemes("about:blank", true);
}

#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
