// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/buildflag.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "url/url_constants.h"

namespace {

const char kEditPhoneAndEmailFieldScript[] = R"(
    let phone_input = document.getElementById('PHONE_HOME_WHOLE_NUMBER');
    phone_input.focus();
    phone_input.value = '408-871-4567';
    phone_input.blur();

    let email_input = document.getElementById('EMAIL_ADDRESS');
    email_input.focus();
    email_input.value = 'abc@def.com';
    email_input.blur();
  )";

}  // namespace

class FormfillPageLoadMetricsObserverBrowserTest : public InProcessBrowserTest {
 public:
  class TestAutofillManager : public autofill::BrowserAutofillManager {
   public:
    explicit TestAutofillManager(autofill::ContentAutofillDriver* driver)
        : BrowserAutofillManager(driver, "en-US") {}

    autofill::TestAutofillManagerWaiter& waiter() { return waiter_; }

   private:
    autofill::TestAutofillManagerWaiter waiter_{
        *this,
        {autofill::AutofillManagerEvent::kFormsSeen}};
  };

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void ClearBrowsingData(uint64_t remove_mask) {
    content::BrowsingDataRemover* remover =
        browser()->profile()->GetBrowsingDataRemover();
    content::BrowsingDataRemoverCompletionObserver observer(remover);
    remover->RemoveAndReply(
        base::Time(), base::Time::Max(), remove_mask,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, &observer);
    observer.BlockUntilCompletion();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  TestAutofillManager* autofill_manager() {
    return autofill_manager_injector_[web_contents()];
  }

 private:
  autofill::TestAutofillManagerInjector<TestAutofillManager>
      autofill_manager_injector_;
};

IN_PROC_BROWSER_TEST_F(FormfillPageLoadMetricsObserverBrowserTest,
                       UserDataFieldFilledUseCounter) {
  base::HistogramTester histogram_tester;

  // When loading the page, wait until OnFormsSeen().
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/autofill/autofill_test_form.html")));
  ASSERT_TRUE(autofill_manager()->waiter().Wait(1));

  ASSERT_TRUE(content::ExecJs(web_contents(), kEditPhoneAndEmailFieldScript));

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kUserDataFieldFilled_PredictedTypeMatch, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kPhoneFieldFilled_PredictedTypeMatch, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kEmailFieldFilled_PredictedTypeMatch, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kEmailFieldFilled_PatternMatch, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kUserDataFieldFilledPreviously, 0);
}

// TODO(crbug.com/40242092): test is flaky across platforms.
IN_PROC_BROWSER_TEST_F(FormfillPageLoadMetricsObserverBrowserTest,
                       DISABLED_UserDataFieldFilledPreviouslyUseCounter) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "a.com", "/autofill/autofill_test_form.html")));

  ASSERT_TRUE(content::ExecJs(web_contents(), kEditPhoneAndEmailFieldScript));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kUserDataFieldFilledPreviously, 1);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kUserDataFieldFilledPreviously, 2);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kUserDataFieldFilledPreviously, 2);
}

#if BUILDFLAG(IS_LINUX)
#define MAYBE_ClearBrowsingData DISABLED_ClearBrowsingData
#else
#define MAYBE_ClearBrowsingData ClearBrowsingData
#endif
IN_PROC_BROWSER_TEST_F(FormfillPageLoadMetricsObserverBrowserTest,
                       MAYBE_ClearBrowsingData) {
  // TODO(crbug.com/40283328): Re-enable this test on bfcache bot.
  if (content::BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    return;
  }
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/autofill/autofill_test_form.html")));

  ASSERT_TRUE(content::ExecJs(web_contents(), kEditPhoneAndEmailFieldScript));

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  ClearBrowsingData(chrome_browsing_data_remover::DATA_TYPE_SITE_USAGE_DATA);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kUserDataFieldFilledPreviously, 0);
}
