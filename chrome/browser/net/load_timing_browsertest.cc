// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "services/network/public/cpp/features.h"
#include "url/gurl.h"

// This file tests that net::LoadTimingInfo is correctly hooked up to the
// NavigationTiming API.  It depends on behavior in a large number of files
// spread across multiple projects, so is somewhat arbitrarily put in
// chrome/browser/net.

namespace {

// Structure used for retrieved from the renderer process.
// These are milliseconds after fetch start, or -1 if not
// present
struct TimingDeltas {
  int domain_lookup_start;
  int domain_lookup_end;
  int connect_start;
  int ssl_start;
  int connect_end;
  int send_start;
  // Must be non-negative and greater than all other times.
  int receive_headers_end;
};

class LoadTimingBrowserTest : public InProcessBrowserTest {
 public:
  LoadTimingBrowserTest() = default;
  ~LoadTimingBrowserTest() override = default;

  // Reads applicable times from performance.timing and writes them to
  // |navigation_deltas|.  Proxy times and send end cannot be read from the
  // Navigation Timing API, so those are all left as null.
  void GetResultDeltas(TimingDeltas* navigation_deltas) {
    *navigation_deltas = TimingDeltas();
    navigation_deltas->domain_lookup_start =
        GetResultDelta("domainLookupStart");
    navigation_deltas->domain_lookup_end = GetResultDelta("domainLookupEnd");
    navigation_deltas->connect_start = GetResultDelta("connectStart");
    navigation_deltas->connect_end = GetResultDelta("connectEnd");
    navigation_deltas->send_start = GetResultDelta("requestStart");
    navigation_deltas->receive_headers_end = GetResultDelta("responseStart");

    // Unlike the above times, secureConnectionStart will be zero when not
    // applicable.  In that case, leave ssl_start as null.
    bool ssl_start_zero =
        content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                        "performance.timing.secureConnectionStart == 0;")
            .ExtractBool();
    if (!ssl_start_zero)
      navigation_deltas->ssl_start = GetResultDelta("secureConnectionStart");
    else
      navigation_deltas->ssl_start = -1;

    // Simple sanity checks.  Make sure times that correspond to LoadTimingInfo
    // occur between fetchStart and loadEventEnd.  Relationships between
    // intervening times are handled by the test bodies.
    int fetch_start = GetResultDelta("fetchStart");
    // While the input dns_start is sometimes null, when read from the
    // NavigationTiming API, it's always non-null.
    EXPECT_LE(fetch_start, navigation_deltas->domain_lookup_start);

    int load_event_end = GetResultDelta("loadEventEnd");
    EXPECT_LE(navigation_deltas->receive_headers_end, load_event_end);
  }

  // Returns the time between performance.timing.fetchStart and the time with
  // the specified name.  This time must be non-negative.
  int GetResultDelta(const std::string& name) {
    std::string command(base::StringPrintf(
        "performance.timing.%s - performance.timing.fetchStart;",
        name.c_str()));
    int time_ms =
        content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                        command.c_str())
            .ExtractInt();
    // Basic sanity check.
    EXPECT_GE(time_ms, 0);
    return time_ms;
  }
};

IN_PROC_BROWSER_TEST_F(LoadTimingBrowserTest, HTTP) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/chunked?waitBeforeHeaders=100");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  TimingDeltas navigation_deltas;
  GetResultDeltas(&navigation_deltas);

  EXPECT_LE(navigation_deltas.domain_lookup_start,
            navigation_deltas.domain_lookup_end);
  EXPECT_LE(navigation_deltas.domain_lookup_end,
            navigation_deltas.connect_start);
  EXPECT_LE(navigation_deltas.connect_start, navigation_deltas.connect_end);
  EXPECT_LE(navigation_deltas.connect_end, navigation_deltas.send_start);
  EXPECT_LT(navigation_deltas.send_start,
            navigation_deltas.receive_headers_end);

  EXPECT_EQ(navigation_deltas.ssl_start, -1);
}

// TODO(crbug.com/40719387): Flaky on all platforms
IN_PROC_BROWSER_TEST_F(LoadTimingBrowserTest, DISABLED_HTTPS) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers();
  ASSERT_TRUE(https_server.Start());
  GURL url = https_server.GetURL("/chunked?waitBeforeHeaders=100");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  TimingDeltas navigation_deltas;
  GetResultDeltas(&navigation_deltas);

  EXPECT_LE(navigation_deltas.domain_lookup_start,
            navigation_deltas.domain_lookup_end);
  EXPECT_LE(navigation_deltas.domain_lookup_end,
            navigation_deltas.connect_start);
  EXPECT_LE(navigation_deltas.connect_start, navigation_deltas.ssl_start);
  EXPECT_LE(navigation_deltas.ssl_start, navigation_deltas.connect_end);
  EXPECT_LE(navigation_deltas.connect_end, navigation_deltas.send_start);
  EXPECT_LT(navigation_deltas.send_start,
            navigation_deltas.receive_headers_end);
}

IN_PROC_BROWSER_TEST_F(LoadTimingBrowserTest, Proxy) {
  ASSERT_TRUE(embedded_test_server()->Start());

  browser()->profile()->GetPrefs()->SetDict(
      proxy_config::prefs::kProxy,
      ProxyConfigDictionary::CreateFixedServers(
          embedded_test_server()->host_port_pair().ToString(), std::string()));
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->FlushProxyConfigMonitorForTesting();

  // This request will fail if it doesn't go through proxy.
  GURL url("http://does.not.resolve.test/chunked?waitBeforeHeaders=100");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  TimingDeltas navigation_deltas;
  GetResultDeltas(&navigation_deltas);

  EXPECT_LE(navigation_deltas.domain_lookup_start,
            navigation_deltas.domain_lookup_end);
  EXPECT_LE(navigation_deltas.domain_lookup_end,
            navigation_deltas.connect_start);
  EXPECT_LE(navigation_deltas.connect_start, navigation_deltas.connect_end);
  EXPECT_LE(navigation_deltas.connect_end, navigation_deltas.send_start);
  EXPECT_LT(navigation_deltas.send_start,
            navigation_deltas.receive_headers_end);

  EXPECT_EQ(navigation_deltas.ssl_start, -1);
}

}  // namespace
