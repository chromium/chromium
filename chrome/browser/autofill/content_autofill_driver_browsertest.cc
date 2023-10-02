// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory_test_api.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace autofill {

class ContentAutofillDriverBrowserTest : public InProcessBrowserTest,
                                         public content::WebContentsObserver {
 public:
  ContentAutofillDriverBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&ContentAutofillDriverBrowserTest::web_contents,
                                base::Unretained(this))) {}
  ~ContentAutofillDriverBrowserTest() override = default;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    Observe(web_contents());
    // Serve both a.com and b.com (and any other domain).
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void OnVisibilityChanged(content::Visibility visibility) override {
    if (visibility == content::Visibility::HIDDEN &&
        web_contents_hidden_callback_) {
      std::move(web_contents_hidden_callback_).Run();
    }
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->HasCommitted())
      return;

    if (nav_entry_committed_callback_)
      std::move(nav_entry_committed_callback_).Run();

    if (navigation_handle->IsSameDocument() &&
        same_document_navigation_callback_) {
      std::move(same_document_navigation_callback_).Run();
    }

    if (!navigation_handle->IsInMainFrame() && subframe_navigation_callback_) {
      std::move(subframe_navigation_callback_).Run();
    }
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  testing::NiceMock<TestContentAutofillClient>& autofill_client() {
    auto* client = autofill_client_injector_[web_contents()];
    CHECK(client);
    return *client;
  }

  ContentAutofillDriverFactory* autofill_driver_factory() {
    return autofill_client().GetAutofillDriverFactory();
  }

 protected:
  base::OnceClosure web_contents_hidden_callback_;
  base::OnceClosure nav_entry_committed_callback_;
  base::OnceClosure same_document_navigation_callback_;
  base::OnceClosure subframe_navigation_callback_;

  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  TestAutofillClientInjector<testing::NiceMock<TestContentAutofillClient>>
      autofill_client_injector_;
  content::test::PrerenderTestHelper prerender_helper_;
};

class ContentAutofillDriverPrerenderBrowserTest
    : public ContentAutofillDriverBrowserTest {
 public:
  ContentAutofillDriverPrerenderBrowserTest() {
    scoped_features_.InitAndEnableFeature(
        features::kAutofillProbableFormSubmissionInBrowser);
  }
  ~ContentAutofillDriverPrerenderBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_features_;
};

IN_PROC_BROWSER_TEST_F(ContentAutofillDriverPrerenderBrowserTest,
                       PrerenderingDoesNotSubmitForm) {
  GURL initial_url =
      embedded_test_server()->GetURL("/autofill/autofill_test_form.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Set a dummy form data to simulate to submit a form. And, OnFormSubmitted
  // method will be called upon navigation.
  autofill_driver_factory()
      ->DriverForFrame(web_contents()->GetPrimaryMainFrame())
      ->renderer_events()
      .SetFormToBeProbablySubmitted(absl::make_optional<FormData>());

  base::HistogramTester histogram_tester;

  // Load a page in the prerendering.
  GURL prerender_url = embedded_test_server()->GetURL("/empty.html");
  int host_id = prerender_helper().AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  EXPECT_FALSE(host_observer.was_activated());
  // TODO(crbug.com/1200511): use a mock AutofillManager and
  // EXPECT_CALL(manager, OnFormSubmitted(_, _, _)).
  histogram_tester.ExpectTotalCount("Autofill.FormSubmission.PerProfileType",
                                    0);

  // Activate the page from the prerendering.
  prerender_helper().NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(host_observer.was_activated());
  histogram_tester.ExpectTotalCount("Autofill.FormSubmission.PerProfileType",
                                    1);
}

}  // namespace autofill
