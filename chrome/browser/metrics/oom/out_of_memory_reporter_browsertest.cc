// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/oom/out_of_memory_reporter.h"

#include <set>
#include <utility>

#include "base/macros.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/service_manager/embedder/switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class OutOfMemoryReporterBrowserTest : public InProcessBrowserTest,
                                       public OutOfMemoryReporter::Observer {
 public:
  OutOfMemoryReporterBrowserTest() {}
  ~OutOfMemoryReporterBrowserTest() override {}

  // InProcessBrowserTest:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUp();
  }
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Disable stack traces during this test since DbgHelp is unreliable in
    // low-memory conditions (see crbug.com/692564).
    command_line->AppendSwitch(
        service_manager::switches::kDisableInProcessStackTraces);
  }

  // OutOfMemoryReporter::Observer:
  void OnForegroundOOMDetected(const GURL& url,
                               ukm::SourceId source_id) override {
    last_oom_url_ = url;
  }

 protected:
  base::Optional<GURL> last_oom_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OutOfMemoryReporterBrowserTest);
};

// No current reliable way to determine OOM on Linux/Mac. Sanitizers also
// interfere with the exit code on OOM, making this detection unreliable.
#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(ADDRESS_SANITIZER)
#define MAYBE_MemoryExhaust DISABLED_MemoryExhaust
#else
#define MAYBE_MemoryExhaust MemoryExhaust
#endif
IN_PROC_BROWSER_TEST_F(OutOfMemoryReporterBrowserTest, MAYBE_MemoryExhaust) {

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  OutOfMemoryReporter::FromWebContents(web_contents)->AddObserver(this);

  const GURL crash_url = embedded_test_server()->GetURL("/title1.html");
  ui_test_utils::NavigateToURL(browser(), crash_url);

  // Careful, this doesn't actually commit the navigation. So, navigating to
  // this URL will cause an OOM associated with the previous committed URL.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(
      browser()->tab_strip_model()->GetActiveWebContents());
  ui_test_utils::NavigateToURL(browser(),
                               GURL(content::kChromeUIMemoryExhaustURL));
  EXPECT_EQ(crash_url, last_oom_url_.value());
}
