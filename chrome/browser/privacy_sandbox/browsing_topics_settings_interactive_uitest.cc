// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/test/browser_test.h"

namespace {

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrivacySandboxTopicsElementId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kIronCollapseFinishedTransitioningEvent);

constexpr char BlockedTopicsListLengthFunc[] =
    "(el) => el.querySelectorAll('privacy-sandbox-interest-item').length";
constexpr char BlockedTopicsListFirstTopicIdFunc[] =
    "(el) => "
    "el.querySelectorAll('privacy-sandbox-interest-item')[0].interest.topic."
    "topicId";

DeepQuery GetManageTopicsPageQuery() {
  return DeepQuery({{"settings-ui", "settings-main", "settings-basic-page",
                     "settings-privacy-page",
                     "settings-privacy-sandbox-manage-topics-subpage"}});
}

DeepQuery GetAdTopicsPageQuery() {
  return DeepQuery(
      {{"settings-ui", "settings-main", "settings-basic-page",
        "settings-privacy-page", "settings-privacy-sandbox-topics-subpage"}});
}

class PrivacySandboxSettingsTopicsInteractiveTest
    : public InteractiveBrowserTest {
 public:
  void SetUpOnMainThread() override {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kPrivacySandboxM1TopicsEnabled, true);
    InteractiveBrowserTest::SetUpOnMainThread();
    browser()->window()->SetBounds(gfx::Rect(600, 700));
  }

  void BlockTopic(int topic_id) {
    auto* const privacy_sandbox_service =
        PrivacySandboxServiceFactory::GetForProfile(browser()->profile());
    privacy_sandbox_service->SetTopicAllowed(
        privacy_sandbox::CanonicalTopic(browsing_topics::Topic(topic_id),
                                        /*taxonomy_version=*/2),
        false);
  }

  size_t GetBlockedTopicsSize() {
    return PrivacySandboxServiceFactory::GetForProfile(browser()->profile())
        ->GetBlockedTopics()
        .size();
  }

  int GetBlockedTopicsFirstTopicId() {
    return PrivacySandboxServiceFactory::GetForProfile(browser()->profile())
        ->GetBlockedTopics()[0]
        .topic_id()
        .value();
  }

  const DeepQuery firstToggle = GetManageTopicsPageQuery() + "#toggle-1";
  const DeepQuery secondToggle = GetManageTopicsPageQuery() + "#toggle-57";
  const DeepQuery blockedTopicsList =
      GetAdTopicsPageQuery() + "#blockedTopicsList";
  const DeepQuery firstBlockedItemButton =
      (blockedTopicsList + "privacy-sandbox-interest-item") + "cr-button";
  const DeepQuery blockedTopicsRow =
      GetAdTopicsPageQuery() + "#blockedTopicsRow";
  const DeepQuery ironCollapse = GetAdTopicsPageQuery() + "cr-collapse";

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Block topic(1) and topic(57) through PS service and validate that it's
// toggled OFF (checked == false) on the Manage Topics Page.
IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsTopicsInteractiveTest,
                       StartWithTwoBlockedTopics) {
  BlockTopic(1);
  BlockTopic(57);
  RunTestSequence(
      InstrumentTab(kPrivacySandboxTopicsElementId),
      NavigateWebContents(kPrivacySandboxTopicsElementId,
                          GURL(chrome::kPrivacySandboxManageTopicsURL)),
      CheckJsResultAt(kPrivacySandboxTopicsElementId, firstToggle,
                      "(el) => el.checked", false),
      CheckJsResultAt(kPrivacySandboxTopicsElementId, secondToggle,
                      "(el) => el.checked", false));
}

// Block first topic on Manage Topics Page. Validate that it is toggled off
// (checked == false). Validate that the PS service returns only 1 blocked topic
// with an ID of 1. Navigate to the Ad Topics Page and validate topic(1) is
// blocked topics list.
IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsTopicsInteractiveTest,
                       BlockFirstTopicOnManageTopicsPage) {
  RunTestSequence(
      InstrumentTab(kPrivacySandboxTopicsElementId),
      NavigateWebContents(kPrivacySandboxTopicsElementId,
                          GURL(chrome::kPrivacySandboxManageTopicsURL)),
      CheckJsResultAt(kPrivacySandboxTopicsElementId, firstToggle,
                      "(el) => el.checked"),
      ExecuteJsAt(kPrivacySandboxTopicsElementId, firstToggle,
                  "(el) => el.click()"),
      CheckJsResultAt(kPrivacySandboxTopicsElementId, firstToggle,
                      "(el) => el.checked", false),
      CheckResult([this]() { return GetBlockedTopicsSize(); }, 1u,
                  "Checking that there is only one blocked topic"),
      CheckResult([this]() { return GetBlockedTopicsFirstTopicId(); }, 1,
                  "Checking that the one blocked topic is topic(1)"),
      NavigateWebContents(kPrivacySandboxTopicsElementId,
                          GURL(chrome::kPrivacySandboxAdTopicsURL)),
      CheckJsResultAt(kPrivacySandboxTopicsElementId, blockedTopicsList,
                      BlockedTopicsListLengthFunc, 1),
      CheckJsResultAt(kPrivacySandboxTopicsElementId, blockedTopicsList,
                      BlockedTopicsListFirstTopicIdFunc, 1));
}

// Block topic(1) through PS service and validate it is shown in the blocked
// topics list of the Ad Topics Page. Unblock the topic and validate PS service
// has 0 blocked topics. Navigate to Manage Topics Page and make sure the first
// topic toggle is ON (checked == true).
IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsTopicsInteractiveTest,
                       UnblockOneTopicOnAdTopicsPage) {
  BlockTopic(1);
  RunTestSequence(
      InstrumentTab(kPrivacySandboxTopicsElementId),
      NavigateWebContents(kPrivacySandboxTopicsElementId,
                          GURL(chrome::kPrivacySandboxAdTopicsURL)),
      CheckJsResultAt(kPrivacySandboxTopicsElementId, blockedTopicsList,
                      BlockedTopicsListLengthFunc, 1),
      CheckJsResultAt(kPrivacySandboxTopicsElementId, blockedTopicsList,
                      BlockedTopicsListFirstTopicIdFunc, 1),
      ExecuteJsAt(kPrivacySandboxTopicsElementId, firstBlockedItemButton,
                  "(el) => el.click()"),
      CheckResult([this]() { return GetBlockedTopicsSize(); }, 0u,
                  "Checking that there is 0 blocked topics"),
      NavigateWebContents(kPrivacySandboxTopicsElementId,
                          GURL(chrome::kPrivacySandboxManageTopicsURL)),
      CheckJsResultAt(kPrivacySandboxTopicsElementId, firstToggle,
                      "(el) => el.checked"));
}

// Validate that first icon is shown to confirm we are querying correctly, then
// check all icons to make sure default one is not used.
IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsTopicsInteractiveTest,
                       ConfirmDefaultIconIsNotUsed) {
  RunTestSequence(
      InstrumentTab(kPrivacySandboxTopicsElementId),
      NavigateWebContents(kPrivacySandboxTopicsElementId,
                          GURL(chrome::kPrivacySandboxManageTopicsURL)),
      CheckJsResultAt(kPrivacySandboxTopicsElementId,
                      GetManageTopicsPageQuery(),
                      R"(
        (el) => Array.from(el.shadowRoot.querySelectorAll('cr-icon')).some(
                    el => el.icon === 'firstLevelTopics20:artist')
        )"),
      CheckJsResultAt(kPrivacySandboxTopicsElementId,
                      GetManageTopicsPageQuery(),
                      R"(
        (el) => Array.from(el.shadowRoot.querySelectorAll('cr-icon')).some(
                el => el.icon === 'firstLevelTopics20:category')
        )",
                      false));
}

// TODO(b/334129738): clean up / re-enable tests

// Pixel test for Manage Topics Page with all topics set to their default state.
IN_PROC_BROWSER_TEST_F(PrivacySandboxSettingsTopicsInteractiveTest,
                       DISABLED_ValidateScreenshotDefaultManageTopicsPage) {
  RunTestSequence(
      InstrumentTab(kPrivacySandboxTopicsElementId),
      NavigateWebContents(kPrivacySandboxTopicsElementId,
                          GURL(chrome::kPrivacySandboxManageTopicsURL)),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(ContentsWebView::kContentsWebViewElementId, "", "5405426"));
}

// Pixel test for Manage Topics Page and Ad Topics Page with blocking the first
// topic through JS commands.
IN_PROC_BROWSER_TEST_F(
    PrivacySandboxSettingsTopicsInteractiveTest,
    DISABLED_ValidateScreenshotsWithFirstTopicBlockedWithJS) {
  StateChange ironCollapseFinishedTransitioning;
  ironCollapseFinishedTransitioning.event =
      kIronCollapseFinishedTransitioningEvent;
  ironCollapseFinishedTransitioning.type =
      StateChange::Type::kExistsAndConditionTrue;
  ironCollapseFinishedTransitioning.where = ironCollapse;
  ironCollapseFinishedTransitioning.test_function =
      "(el) => { return el.transitioning === false }";

  RunTestSequence(
      InstrumentTab(kPrivacySandboxTopicsElementId),
      NavigateWebContents(kPrivacySandboxTopicsElementId,
                          GURL(chrome::kPrivacySandboxManageTopicsURL)),
      ExecuteJsAt(kPrivacySandboxTopicsElementId, firstToggle,
                  "(el) => { el.click(); }"),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot not supported in all test modes."),
      Screenshot(ContentsWebView::kContentsWebViewElementId,
                 "ManageTopicsPageAfterFirstTopicBlocked", "5405426"),
      NavigateWebContents(kPrivacySandboxTopicsElementId,
                          GURL(chrome::kPrivacySandboxAdTopicsURL)),
      ExecuteJsAt(kPrivacySandboxTopicsElementId, ironCollapse,
                  "(el) => { el.noAnimation = true; }"),
      ExecuteJsAt(kPrivacySandboxTopicsElementId, blockedTopicsRow,
                  "(el) => { el.click(); }"),
      WaitForStateChange(kPrivacySandboxTopicsElementId,
                         ironCollapseFinishedTransitioning),
      Screenshot(ContentsWebView::kContentsWebViewElementId,
                 "AdTopicsPageAfterFirstTopicBlocked", "5405426"),
      CheckResult([this]() { return GetBlockedTopicsSize(); }, 1u,
                  "Checking that there is only one blocked topic"),
      CheckResult([this]() { return GetBlockedTopicsFirstTopicId(); }, 1,
                  "Checking that the one blocked topic is topic(1)"));
}
}  // namespace
