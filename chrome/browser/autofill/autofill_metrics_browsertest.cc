// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/path_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "url/gurl.h"

namespace {

bool ContainsHost(
    const std::map<ukm::SourceId, std::unique_ptr<ukm::UkmSource>>& sources,
    const std::string& host) {
  for (const auto& kv : sources) {
    if (host == kv.second->url().host())
      return true;
  }
  return false;
}

class AutofillMetricsBrowserTest : public InProcessBrowserTest {
 public:
  AutofillMetricsBrowserTest() {}

  AutofillMetricsBrowserTest(const AutofillMetricsBrowserTest&) = delete;
  AutofillMetricsBrowserTest& operator=(const AutofillMetricsBrowserTest&) =
      delete;

  ~AutofillMetricsBrowserTest() override {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed
    // to load pages from other hosts without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();

    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    content::SetupCrossSiteRedirector(embedded_test_server());
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data/autofill");
    embedded_test_server()->StartAcceptingConnections();
  }

  autofill::test::AutofillBrowserTestEnvironment autofill_test_environment_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(AutofillMetricsBrowserTest,
                       CorrectSourceForCrossSiteEmbeddedAddressForm) {
  GURL main_frame_url =
      embedded_test_server()->GetURL("a.com", "/autofill_iframe_embedder.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL iframe_url =
      embedded_test_server()->GetURL("b.com", "/autofill_address_form.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  content::RenderFrameHost* frame = ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(frame);

  // Make sure the UKM were logged for the main frame url and none for the
  // iframe url.
  EXPECT_TRUE(
      ContainsHost(test_ukm_recorder_->GetSources(), main_frame_url.host()));
  EXPECT_FALSE(
      ContainsHost(test_ukm_recorder_->GetSources(), iframe_url.host()));
}

IN_PROC_BROWSER_TEST_F(AutofillMetricsBrowserTest,
                       CorrectSourceForCrossSiteEmbeddedCreditCardForm) {
  GURL main_frame_url =
      embedded_test_server()->GetURL("a.com", "/autofill_iframe_embedder.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL iframe_url = embedded_test_server()->GetURL(
      "b.com", "/autofill_credit_card_form.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  content::RenderFrameHost* frame = ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(frame);

  // Make sure the UKM were logged for the main frame url and none for the
  // iframe url.
  EXPECT_TRUE(
      ContainsHost(test_ukm_recorder_->GetSources(), main_frame_url.host()));
  EXPECT_FALSE(
      ContainsHost(test_ukm_recorder_->GetSources(), iframe_url.host()));
}

IN_PROC_BROWSER_TEST_F(AutofillMetricsBrowserTest,
                       CorrectSourceForUnownedAddressCheckout) {
  GURL main_frame_url = embedded_test_server()->GetURL(
      "a.com", "/autofill_unowned_address_checkout.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  // Make sure the UKM were logged for the main frame url.
  EXPECT_TRUE(
      ContainsHost(test_ukm_recorder_->GetSources(), main_frame_url.host()));
}

IN_PROC_BROWSER_TEST_F(AutofillMetricsBrowserTest,
                       CorrectSourceForUnownedCreditCardCheckout) {
  GURL main_frame_url = embedded_test_server()->GetURL(
      "a.com", "/autofill_unowned_credit_card_checkout.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  // Make sure the UKM were logged for the main frame url.
  EXPECT_TRUE(
      ContainsHost(test_ukm_recorder_->GetSources(), main_frame_url.host()));
}

IN_PROC_BROWSER_TEST_F(
    AutofillMetricsBrowserTest,
    CorrectSourceForCrossSiteEmbeddedUnownedAddressCheckout) {
  GURL main_frame_url =
      embedded_test_server()->GetURL("a.com", "/autofill_iframe_embedder.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL iframe_url = embedded_test_server()->GetURL(
      "b.com", "/autofill_unowned_address_checkout.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  content::RenderFrameHost* frame = ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(frame);

  // Make sure the UKM were logged for the main frame url and none for the
  // iframe url.
  EXPECT_TRUE(
      ContainsHost(test_ukm_recorder_->GetSources(), main_frame_url.host()));
  EXPECT_FALSE(
      ContainsHost(test_ukm_recorder_->GetSources(), iframe_url.host()));
}

IN_PROC_BROWSER_TEST_F(
    AutofillMetricsBrowserTest,
    CorrectSourceForCrossSiteEmbeddedUnownedCreditCardCheckout) {
  GURL main_frame_url =
      embedded_test_server()->GetURL("a.com", "/autofill_iframe_embedder.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL iframe_url = embedded_test_server()->GetURL(
      "b.com", "/autofill_unowned_credit_card_checkout.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  content::RenderFrameHost* frame = ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(frame);

  // Make sure the UKM were logged for the main frame url and none for the
  // iframe url.
  EXPECT_TRUE(
      ContainsHost(test_ukm_recorder_->GetSources(), main_frame_url.host()));
  EXPECT_FALSE(
      ContainsHost(test_ukm_recorder_->GetSources(), iframe_url.host()));
}

class SitePerProcessAutofillMetricsBrowserTest
    : public AutofillMetricsBrowserTest {
 public:
  SitePerProcessAutofillMetricsBrowserTest() {}

  SitePerProcessAutofillMetricsBrowserTest(
      const SitePerProcessAutofillMetricsBrowserTest&) = delete;
  SitePerProcessAutofillMetricsBrowserTest& operator=(
      const SitePerProcessAutofillMetricsBrowserTest&) = delete;

  ~SitePerProcessAutofillMetricsBrowserTest() override {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    AutofillMetricsBrowserTest::SetUpCommandLine(command_line);

    // Append --site-per-process flag.
    content::IsolateAllSitesForTesting(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(SitePerProcessAutofillMetricsBrowserTest,
                       CorrectSourceForCrossSiteEmbeddedAddressForm) {
  GURL main_frame_url =
      embedded_test_server()->GetURL("a.com", "/autofill_iframe_embedder.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL iframe_url =
      embedded_test_server()->GetURL("b.com", "/autofill_address_form.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  content::RenderFrameHost* frame = ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(frame);
  EXPECT_NE(frame->GetSiteInstance(),
            tab->GetPrimaryMainFrame()->GetSiteInstance());

  // Make sure the UKM were logged for the main frame url and none for the
  // iframe url.
  EXPECT_TRUE(
      ContainsHost(test_ukm_recorder_->GetSources(), main_frame_url.host()));
  EXPECT_FALSE(
      ContainsHost(test_ukm_recorder_->GetSources(), iframe_url.host()));
}

IN_PROC_BROWSER_TEST_F(SitePerProcessAutofillMetricsBrowserTest,
                       CorrectSourceForCrossSiteEmbeddedCreditCardForm) {
  GURL main_frame_url =
      embedded_test_server()->GetURL("a.com", "/autofill_iframe_embedder.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL iframe_url = embedded_test_server()->GetURL(
      "b.com", "/autofill_credit_card_form.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  content::RenderFrameHost* frame = ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(frame);
  EXPECT_NE(frame->GetSiteInstance(),
            tab->GetPrimaryMainFrame()->GetSiteInstance());

  // Make sure the UKM were logged for the main frame url and none for the
  // iframe url.
  EXPECT_TRUE(
      ContainsHost(test_ukm_recorder_->GetSources(), main_frame_url.host()));
  EXPECT_FALSE(
      ContainsHost(test_ukm_recorder_->GetSources(), iframe_url.host()));
}

IN_PROC_BROWSER_TEST_F(SitePerProcessAutofillMetricsBrowserTest,
                       CorrectSourceForUnownedAddressCheckout) {
  GURL main_frame_url = embedded_test_server()->GetURL(
      "a.com", "/autofill_unowned_address_checkout.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  // Make sure the UKM were logged for the main frame url.
  EXPECT_TRUE(
      ContainsHost(test_ukm_recorder_->GetSources(), main_frame_url.host()));
}

IN_PROC_BROWSER_TEST_F(SitePerProcessAutofillMetricsBrowserTest,
                       CorrectSourceForUnownedCreditCardCheckout) {
  GURL main_frame_url = embedded_test_server()->GetURL(
      "a.com", "/autofill_unowned_credit_card_checkout.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  // Make sure the UKM were logged for the main frame url.
  EXPECT_TRUE(
      ContainsHost(test_ukm_recorder_->GetSources(), main_frame_url.host()));
}

IN_PROC_BROWSER_TEST_F(
    SitePerProcessAutofillMetricsBrowserTest,
    CorrectSourceForCrossSiteEmbeddedUnownedAddressCheckout) {
  GURL main_frame_url =
      embedded_test_server()->GetURL("a.com", "/autofill_iframe_embedder.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL iframe_url = embedded_test_server()->GetURL(
      "b.com", "/autofill_unowned_address_checkout.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  content::RenderFrameHost* frame = ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(frame);
  EXPECT_NE(frame->GetSiteInstance(),
            tab->GetPrimaryMainFrame()->GetSiteInstance());

  // Make sure the UKM were logged for the main frame url and none for the
  // iframe url.
  EXPECT_TRUE(
      ContainsHost(test_ukm_recorder_->GetSources(), main_frame_url.host()));
  EXPECT_FALSE(
      ContainsHost(test_ukm_recorder_->GetSources(), iframe_url.host()));
}

IN_PROC_BROWSER_TEST_F(
    SitePerProcessAutofillMetricsBrowserTest,
    CorrectSourceForCrossSiteEmbeddedUnownedCreditCardCheckout) {
  GURL main_frame_url =
      embedded_test_server()->GetURL("a.com", "/autofill_iframe_embedder.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL iframe_url = embedded_test_server()->GetURL(
      "b.com", "/autofill_unowned_credit_card_checkout.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  content::RenderFrameHost* frame = ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(frame);
  EXPECT_NE(frame->GetSiteInstance(),
            tab->GetPrimaryMainFrame()->GetSiteInstance());

  // Make sure the UKM were logged for the main frame url and none for the
  // iframe url.
  EXPECT_TRUE(
      ContainsHost(test_ukm_recorder_->GetSources(), main_frame_url.host()));
  EXPECT_FALSE(
      ContainsHost(test_ukm_recorder_->GetSources(), iframe_url.host()));
}

}  // namespace
