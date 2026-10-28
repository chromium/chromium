// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/extension_actor_tracker.h"

#include <memory>
#include <optional>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/reporting_test_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace enterprise_connectors {
namespace {

using ::testing::NiceMock;
using ::testing::Return;

class ExtensionActorTrackerTest : public ChromeRenderViewHostTestHarness {
 public:
  ExtensionActorTrackerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kEnterpriseExtensionActorAttribution);
  }
  ~ExtensionActorTrackerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-token"));
    test::SetOnSecurityEventReporting(profile()->GetPrefs(), /*enabled=*/true);
    ExtensionActorTracker::CreateForWebContents(web_contents());
  }

  ExtensionActorTracker* tracker() {
    return ExtensionActorTracker::FromWebContents(web_contents());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ExtensionActorTrackerTest, SetAndClearActiveActorLifecycle) {
  EXPECT_EQ(std::nullopt,
            ExtensionActorTracker::GetActiveActor(web_contents()));

  ExtensionActorTracker::SetActiveActor(web_contents(), "extension_alpha",
                                        "Runtime.evaluate");
  EXPECT_EQ("extension_alpha",
            ExtensionActorTracker::GetActiveActor(web_contents()));

  // Last-writer-wins when a second extension issues a command.
  ExtensionActorTracker::SetActiveActor(web_contents(), "extension_beta",
                                        "Runtime.evaluate");
  EXPECT_EQ("extension_beta",
            ExtensionActorTracker::GetActiveActor(web_contents()));

  // Untracked read-only CDP commands do not overwrite the active actor.
  ExtensionActorTracker::SetActiveActor(web_contents(), "extension_gamma",
                                        "Network.enable");
  ExtensionActorTracker::SetActiveActor(web_contents(), "extension_gamma",
                                        "DOM.getDocument");
  EXPECT_EQ("extension_beta",
            ExtensionActorTracker::GetActiveActor(web_contents()));

  // Tracked CDP commands update the active actor.
  ExtensionActorTracker::SetActiveActor(web_contents(), "extension_gamma",
                                        "DOM.focus");
  EXPECT_EQ("extension_gamma",
            ExtensionActorTracker::GetActiveActor(web_contents()));
  ExtensionActorTracker::SetActiveActor(web_contents(), "extension_alpha",
                                        "DOM.setFileInputFiles");
  EXPECT_EQ("extension_alpha",
            ExtensionActorTracker::GetActiveActor(web_contents()));
  ExtensionActorTracker::SetActiveActor(web_contents(), "extension_beta",
                                        "Input.dispatchMouseEvent");
  EXPECT_EQ("extension_beta",
            ExtensionActorTracker::GetActiveActor(web_contents()));

  ExtensionActorTracker::ClearActiveActor(web_contents());
  EXPECT_EQ(std::nullopt,
            ExtensionActorTracker::GetActiveActor(web_contents()));
}

TEST_F(ExtensionActorTrackerTest,
       UserInteractionClearsActorUnlessDebuggerOrScroll) {
  ExtensionActorTracker::SetActiveActor(web_contents(), "extension_alpha",
                                        "Runtime.evaluate");

  blink::WebMouseEvent debugger_event;
  debugger_event.SetType(blink::WebInputEvent::Type::kMouseDown);
  debugger_event.SetModifiers(blink::WebInputEvent::kFromDebugger);
  tracker()->DidGetUserInteraction(debugger_event);
  EXPECT_EQ("extension_alpha",
            ExtensionActorTracker::GetActiveActor(web_contents()));

  blink::WebGestureEvent scroll_event;
  scroll_event.SetType(blink::WebInputEvent::Type::kGestureScrollBegin);
  tracker()->DidGetUserInteraction(scroll_event);
  EXPECT_EQ("extension_alpha",
            ExtensionActorTracker::GetActiveActor(web_contents()));

  blink::WebMouseEvent mouse_down_event;
  mouse_down_event.SetType(blink::WebInputEvent::Type::kMouseDown);
  tracker()->DidGetUserInteraction(mouse_down_event);
  EXPECT_EQ(std::nullopt,
            ExtensionActorTracker::GetActiveActor(web_contents()));
}

TEST_F(ExtensionActorTrackerTest, DidFinishNavigationClearsCrossDocumentOnly) {
  ExtensionActorTracker::SetActiveActor(web_contents(), "extension_alpha",
                                        "Runtime.evaluate");

  NiceMock<content::MockNavigationHandle> same_doc_nav(web_contents());
  same_doc_nav.set_has_committed(true);
  same_doc_nav.set_is_in_primary_main_frame(true);
  same_doc_nav.set_is_same_document(true);
  tracker()->DidFinishNavigation(&same_doc_nav);
  EXPECT_EQ("extension_alpha",
            ExtensionActorTracker::GetActiveActor(web_contents()));

  NiceMock<content::MockNavigationHandle> cross_doc_nav(web_contents());
  cross_doc_nav.set_has_committed(true);
  cross_doc_nav.set_is_in_primary_main_frame(true);
  cross_doc_nav.set_is_same_document(false);
  tracker()->DidFinishNavigation(&cross_doc_nav);
  EXPECT_EQ(std::nullopt,
            ExtensionActorTracker::GetActiveActor(web_contents()));
}

TEST_F(ExtensionActorTrackerTest, AboutBlankDoesNotConsumeInitialNavigation) {
  ExtensionActorTracker::SetActiveActor(web_contents(), "extension_alpha",
                                        "tabs.create");

  NiceMock<content::MockNavigationHandle> blank_nav(web_contents());
  blank_nav.set_url(GURL("about:blank"));
  blank_nav.set_has_committed(true);
  blank_nav.set_is_in_primary_main_frame(true);
  tracker()->DidFinishNavigation(&blank_nav);
  EXPECT_EQ("extension_alpha",
            ExtensionActorTracker::GetActiveActor(web_contents()));

  // Non-navigation commands while the initial navigation is in flight do not
  // reset `is_initial_navigation_`.
  ExtensionActorTracker::SetActiveActor(web_contents(), "extension_alpha",
                                        "Runtime.evaluate");

  NiceMock<content::MockNavigationHandle> target_nav(web_contents());
  target_nav.set_url(GURL("https://example.com"));
  target_nav.set_has_committed(true);
  target_nav.set_is_in_primary_main_frame(true);
  tracker()->DidFinishNavigation(&target_nav);
  EXPECT_EQ("extension_alpha",
            ExtensionActorTracker::GetActiveActor(web_contents()));

  NiceMock<content::MockNavigationHandle> user_nav(web_contents());
  user_nav.set_url(GURL("https://other.com"));
  user_nav.set_has_committed(true);
  user_nav.set_is_in_primary_main_frame(true);
  tracker()->DidFinishNavigation(&user_nav);
  EXPECT_EQ(std::nullopt,
            ExtensionActorTracker::GetActiveActor(web_contents()));

  // Same-document navigations consume `is_initial_navigation_`.
  ExtensionActorTracker::SetActiveActor(web_contents(), "extension_alpha",
                                        "tabs.update");
  NiceMock<content::MockNavigationHandle> hash_nav(web_contents());
  hash_nav.set_url(GURL("https://other.com#section"));
  hash_nav.set_has_committed(true);
  hash_nav.set_is_in_primary_main_frame(true);
  hash_nav.set_is_same_document(true);
  tracker()->DidFinishNavigation(&hash_nav);
  EXPECT_EQ("extension_alpha",
            ExtensionActorTracker::GetActiveActor(web_contents()));

  tracker()->DidFinishNavigation(&user_nav);
  EXPECT_EQ(std::nullopt,
            ExtensionActorTracker::GetActiveActor(web_contents()));
}

TEST_F(ExtensionActorTrackerTest,
       ExtensionFormAndDownloadNavigationsPreserveAttribution) {
  ExtensionActorTracker::SetActiveActor(web_contents(), "extension_alpha",
                                        "Runtime.evaluate");

  NiceMock<content::MockNavigationHandle> extension_nav(web_contents());
  extension_nav.set_url(GURL("https://example.com/target"));
  extension_nav.set_has_committed(true);
  extension_nav.set_is_in_primary_main_frame(true);
  extension_nav.set_initiator_origin(
      url::Origin::Create(GURL("chrome-extension://extension_alpha")));
  tracker()->DidFinishNavigation(&extension_nav);
  EXPECT_EQ("extension_alpha",
            ExtensionActorTracker::GetActiveActor(web_contents()));

  NiceMock<content::MockNavigationHandle> form_nav(web_contents());
  form_nav.set_url(GURL("https://example.com/login_submit"));
  form_nav.set_has_committed(true);
  form_nav.set_is_in_primary_main_frame(true);
  form_nav.set_is_form_submission(true);
  tracker()->DidFinishNavigation(&form_nav);
  EXPECT_EQ("extension_alpha",
            ExtensionActorTracker::GetActiveActor(web_contents()));

  NiceMock<content::MockNavigationHandle> download_nav(web_contents());
  download_nav.set_url(GURL("https://example.com/file.zip"));
  download_nav.set_has_committed(false);
  download_nav.set_is_in_primary_main_frame(true);
  ON_CALL(download_nav, IsDownload()).WillByDefault(Return(true));
  tracker()->DidFinishNavigation(&download_nav);
  EXPECT_EQ("extension_alpha",
            ExtensionActorTracker::GetActiveActor(web_contents()));
}

TEST_F(ExtensionActorTrackerTest,
       DidOpenRequestedURLPropagatesActorToChildWebContents) {
  std::unique_ptr<content::WebContents> child_wc = CreateTestWebContents();
  ExtensionActorTracker::SetActiveActor(web_contents(), "extension_alpha",
                                        "Runtime.evaluate");

  tracker()->DidOpenRequestedURL(
      child_wc.get(), main_rfh(), GURL("https://example.com/popup"),
      content::Referrer(), WindowOpenDisposition::kNewForegroundTab,
      ui::PAGE_TRANSITION_LINK, /*started_from_context_menu=*/false,
      /*renderer_initiated=*/true);

  EXPECT_EQ("extension_alpha",
            ExtensionActorTracker::GetActiveActor(child_wc.get()));
}

TEST_F(ExtensionActorTrackerTest, DisabledWhenFeatureOrEnterprisePolicyOff) {
  test::SetOnSecurityEventReporting(profile()->GetPrefs(), /*enabled=*/false);
  EXPECT_FALSE(ExtensionActorTracker::IsEnabled(browser_context()));

  std::unique_ptr<content::WebContents> unmanaged_tab = CreateTestWebContents();
  ExtensionActorTracker::SetActiveActor(unmanaged_tab.get(), "extension_alpha",
                                        "Runtime.evaluate");
  EXPECT_EQ(nullptr,
            ExtensionActorTracker::FromWebContents(unmanaged_tab.get()));

  test::SetOnSecurityEventReporting(profile()->GetPrefs(), /*enabled=*/true);
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(
      kEnterpriseExtensionActorAttribution);
  EXPECT_FALSE(ExtensionActorTracker::IsEnabled(browser_context()));
}

}  // namespace
}  // namespace enterprise_connectors
