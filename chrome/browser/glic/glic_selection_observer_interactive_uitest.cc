// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/test_support/glic_browser_interactive_test.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {
namespace {

// TODO(b/537847327): This test should be migrated to use GlicApiBrowserTest,
// and not the test client.
class GlicSelectionObserverInteractiveUiTest
    : public GlicBrowserInteractiveTest {
 public:
  GlicSelectionObserverInteractiveUiTest() {
    glic_test_environment().SetGlicPagePath("/glic/test_client/index.html");
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicSelectionPrompt, {{"updates_only", "true"}}}}, {});
  }

 protected:
  static constexpr char kCheckContextJs[] = R"JS(
       () => {
          let c = document.querySelector('#additionalContextResult');
          return !!c &&
              c.innerText.includes('Tab ID: ') &&
              c.innerText.includes('MIME Type: application/x-glic-selection') &&
              c.innerText.includes('Data: This page');
       }
    )JS";

  auto SelectAll() {
    return Do([this] {
      content::WebContents* web_contents =
          GetTabListInterface()->GetActiveTab()->GetContents();
      web_contents->SelectAll();
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicSelectionObserverInteractiveUiTest,
                       SelectionUpdatesContext) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  const GURL url = embedded_test_server()->GetURL("/title2.html");
  const DeepQuery kPathToBody{
      "body",
  };

  RunTestSequence(InstrumentTab(kActiveTab),
                  NavigateWebContents(kActiveTab, url));

  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  RunTestSequence(WaitForWebContentsPainted(kActiveTab),
                  MoveMouseTo(kActiveTab, kPathToBody), ClickMouse(),
                  ClickMouse(), SelectAll());

  EXPECT_TRUE(RunUntil(
      [&]() {
        auto* guest_frame = instance->host().GetGuestMainFrame();
        return guest_frame &&
               content::EvalJs(guest_frame, base::StrCat({"(", kCheckContextJs,
                                                          ")()"})) == true;
      },
      "Timeout waiting for selection context in Glic"));
}

// TODO(b/537847327): This test should be migrated to use GlicApiBrowserTest,
// and not the test client.
class GlicSelectionObserverCrossOriginNavigationTest
    : public GlicBrowserInteractiveTest {
 public:
  GlicSelectionObserverCrossOriginNavigationTest() {
    glic_test_environment().SetGlicPagePath("/glic/test_client/index.html");
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicSelectionPrompt, {{"updates_only", "true"}}}}, {});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    GlicBrowserInteractiveTest::SetUpOnMainThread();
  }

 protected:
  // Checks the additional context received by the web client and ensures that
  // it does not context content from origin A.
  static constexpr char kSawNoAdditionalContextFromOriginA[] = R"JS(
       () => {
          let c = document.querySelector('#additionalContextResult');
          return !!c &&
              !c.innerText.includes('application/x-glic-selection') &&
              !c.innerText.includes('origin A content');
       }
    )JS";

  auto SelectAll() {
    return Do([this] {
      content::WebContents* web_contents =
          GetTabListInterface()->GetActiveTab()->GetContents();
      web_contents->SelectAll();
    });
  }

  auto Wait(base::TimeDelta timeout) {
    auto observer = std::make_unique<test::internal::WaitingStateObserver>();
    auto observer_ptr = observer.get();
    return Steps(
        Do(base::BindRepeating(
            [](test::internal::WaitingStateObserver* observer,
               base::TimeDelta timeout) { observer->Start(timeout); },
            base::Unretained(observer_ptr), timeout)),
        ObserveState(glic::test::internal::kDelayState, std::move(observer)),
        WaitForState(glic::test::internal::kDelayState, true));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicSelectionObserverCrossOriginNavigationTest,
                       DoesNotShowStaleSelectionUi) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  const GURL url_a =
      embedded_test_server()->GetURL("a.test", "/glic/selection_origin_a.html");
  const GURL url_b =
      embedded_test_server()->GetURL("b.test", "/glic/selection_origin_b.html");
  ASSERT_NE(url::Origin::Create(url_a), url::Origin::Create(url_b));

  const DeepQuery kPathToBody{"body"};

  RunTestSequence(
      InstrumentTab(kActiveTab),

      // Step 1: Load origin A and let the user "select" text.
      // The mouse clicks ensure is_key_selection_=false so the selection is
      // accepted; the Wait covers the 200ms debounce so last_selected_text_
      // is populated with content from origin A.
      NavigateWebContents(kActiveTab, url_a),
      WaitForWebContentsPainted(kActiveTab),
      MoveMouseTo(kActiveTab, kPathToBody), ClickMouse(), ClickMouse(),
      SelectAll(), Wait(base::Milliseconds(400)),

      // Step 2: Cross-document, cross-origin navigation to origin B in the
      // same tab. PrimaryPageChanged() clears state, ensuring that we will not
      // show stale selection UI in the web client.
      NavigateWebContents(kActiveTab, url_b),
      WaitForWebContentsPainted(kActiveTab));

  // Step 3: Open the Glic panel *without touching origin B's content*. If we
  // did not properly clear state, then OnGlobalPanelShowHide() re-ships
  // last_selected_text_ via SendAdditionalContext(), tagged with the current
  // tab handle.
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  // Step 4: Ensure that we did not get a selection sent to the client. If we
  // had not cleared correctly, we would have additional context from origin A,
  // which is incorrect since we have navigated to origin B.
  EXPECT_TRUE(RunUntil(
      [&]() {
        auto* guest_frame = instance->host().GetGuestMainFrame();
        return guest_frame &&
               content::EvalJs(
                   guest_frame,
                   base::StrCat({"(", kSawNoAdditionalContextFromOriginA,
                                 ")()"})) == true;
      },
      "Timeout waiting for selection context in Glic"));
}

}  // namespace
}  // namespace glic
