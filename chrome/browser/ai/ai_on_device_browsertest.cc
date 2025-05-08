// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "third_party/blink/public/common/features_generated.h"

class AIOnDeviceBrowserTest : public InProcessBrowserTest {
 public:
  AIOnDeviceBrowserTest() {
    feature_list_.InitWithFeatures(
        {blink::features::kAIPromptAPIMultimodalInput,
         blink::features::kAIRewriterAPI, blink::features::kAISummarizationAPI,
         blink::features::kAIWriterAPI},
        {});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(&embedded_https_test_server());
    ASSERT_TRUE(embedded_https_test_server().Start());

    auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
    GURL url(embedded_https_test_server().GetURL("a.test", "/empty.html"));
    ASSERT_TRUE(NavigateToURL(tab, url));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AIOnDeviceBrowserTest, APIsExposedToWindowNotWorker) {
  static constexpr char kWindow[] = "try { %s; 'OK'; } catch (e) { e.name; }";
  static constexpr char kWorker[] =
      R"JS(
      const workerCode = `try { %s; self.postMessage('OK'); }
                          catch (e) { self.postMessage(e.name); }`;
      const blob = new Blob([workerCode], { type: 'text/javascript' });
      const worker = new Worker(URL.createObjectURL(blob));
      new Promise(r => { worker.onmessage = e => { r(e.data); }});
    )JS";
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  for (const auto& id : {"LanguageModel", "Rewriter", "Summarizer", "Writer"}) {
    EXPECT_EQ("OK", content::EvalJs(tab, absl::StrFormat(kWindow, id))) << id;
    EXPECT_EQ("ReferenceError",
              content::EvalJs(tab, absl::StrFormat(kWorker, id)))
        << id;
  }
}
