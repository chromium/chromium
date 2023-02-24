// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
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
namespace {

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() = default;

  MockAutofillClient(const MockAutofillClient&) = delete;
  MockAutofillClient& operator=(const MockAutofillClient&) = delete;

  ~MockAutofillClient() override = default;

  PrefService* GetPrefs() override {
    return const_cast<PrefService*>(std::as_const(*this).GetPrefs());
  }
  const PrefService* GetPrefs() const override { return &prefs_; }

  user_prefs::PrefRegistrySyncable* GetPrefRegistry() {
    return prefs_.registry();
  }

  MOCK_METHOD(void,
              ShowAutofillPopup,
              (const PopupOpenArgs& open_args,
               base::WeakPtr<AutofillPopupDelegate> delegate),
              (override));
  MOCK_METHOD(void, HideAutofillPopup, (PopupHidingReason), (override));

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
};

}  // namespace

class ContentAutofillDriverBrowserTest : public InProcessBrowserTest,
                                         public content::WebContentsObserver {
 public:
  ContentAutofillDriverBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&ContentAutofillDriverBrowserTest::web_contents,
                                base::Unretained(this))) {}
  ~ContentAutofillDriverBrowserTest() override = default;

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    autofill_client_ =
        std::make_unique<testing::NiceMock<MockAutofillClient>>();
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents != nullptr);
    Observe(web_contents);
    prefs::RegisterProfilePrefs(autofill_client().GetPrefRegistry());

    web_contents->RemoveUserData(
        ContentAutofillDriverFactory::
            kContentAutofillDriverFactoryWebContentsUserDataKey);
    ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
        web_contents, &autofill_client(),
        base::BindRepeating(&autofill::BrowserDriverInitHook,
                            &autofill_client(), "en-US"));

    // Serve both a.com and b.com (and any other domain).
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    // Verify the expectations here, because closing the browser may incur
    // other calls in |autofill_client()| e.g., HideAutofillPopup.
    testing::Mock::VerifyAndClearExpectations(&autofill_client());
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

  testing::NiceMock<MockAutofillClient>& autofill_client() {
    return *autofill_client_.get();
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  base::OnceClosure web_contents_hidden_callback_;
  base::OnceClosure nav_entry_committed_callback_;
  base::OnceClosure same_document_navigation_callback_;
  base::OnceClosure subframe_navigation_callback_;

  std::unique_ptr<testing::NiceMock<MockAutofillClient>> autofill_client_;
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(ContentAutofillDriverBrowserTest,
                       SwitchTabAndHideAutofillPopup) {
  EXPECT_CALL(autofill_client(),
              HideAutofillPopup(PopupHidingReason::kTabGone));

  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  web_contents_hidden_callback_ = runner->QuitClosure();
  chrome::AddSelectedTabWithURL(browser(), GURL(url::kAboutBlankURL),
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  runner->Run();
}

IN_PROC_BROWSER_TEST_F(ContentAutofillDriverBrowserTest,
                       SameDocumentNavigationHideAutofillPopup) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/autofill/autofill_test_form.html")));

  // The Autofill popup should be hidden for same document navigations. It may
  // called twice because the zoom changed event may also fire for same-page
  // navigations.
  EXPECT_CALL(autofill_client(),
              HideAutofillPopup(PopupHidingReason::kNavigation))
      .Times(testing::AtLeast(1));

  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  same_document_navigation_callback_ = runner->QuitClosure();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/autofill/autofill_test_form.html#foo")));
  // This will block until a same document navigation is observed.
  runner->Run();
}

IN_PROC_BROWSER_TEST_F(ContentAutofillDriverBrowserTest,
                       PrerenderNavigationDoesntHideAutofillPopup) {
  GURL initial_url =
      embedded_test_server()->GetURL("/autofill/autofill_test_form.html");
  GURL prerender_url = embedded_test_server()->GetURL("/empty.html");
  prerender_helper().NavigatePrimaryPage(initial_url);

  int host_id = content::RenderFrameHost::kNoFrameTreeNodeId;

  {
    EXPECT_CALL(autofill_client(),
                HideAutofillPopup(PopupHidingReason::kNavigation))
        .Times(0);
    host_id = prerender_helper().AddPrerender(prerender_url);
  }

  EXPECT_CALL(autofill_client(),
              HideAutofillPopup(PopupHidingReason::kNavigation))
      .Times(testing::AtLeast(1));

  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  prerender_helper().NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(host_observer.was_activated());
}

IN_PROC_BROWSER_TEST_F(ContentAutofillDriverBrowserTest,
                       SubframeNavigationDoesntHideAutofillPopup) {
  // Main frame is on a.com, iframe is on b.com.
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/autofill/cross_origin_iframe.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // The Autofill popup should NOT be hidden for subframe navigations.
  EXPECT_CALL(autofill_client(), HideAutofillPopup).Times(0);

  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  subframe_navigation_callback_ = runner->QuitClosure();
  GURL iframe_url = embedded_test_server()->GetURL(
      "b.com", "/autofill/autofill_test_form.html");
  EXPECT_TRUE(content::NavigateIframeToURL(
      browser()->tab_strip_model()->GetActiveWebContents(), "crossFrame",
      iframe_url));
  // This will block until a subframe navigation is observed.
  runner->Run();
}

IN_PROC_BROWSER_TEST_F(ContentAutofillDriverBrowserTest,
                       TestPageNavigationHidingAutofillPopup) {
  // HideAutofillPopup is called once for each navigation.
  EXPECT_CALL(autofill_client(),
              HideAutofillPopup(PopupHidingReason::kNavigation))
      .Times(2);

  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  nav_entry_committed_callback_ = runner->QuitClosure();
  browser()->OpenURL(content::OpenURLParams(
      GURL(chrome::kChromeUIBookmarksURL), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
  browser()->OpenURL(content::OpenURLParams(
      GURL(chrome::kChromeUIAboutURL), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
  runner->Run();
}

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
  ContentAutofillDriverFactory::FromWebContents(web_contents())
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
