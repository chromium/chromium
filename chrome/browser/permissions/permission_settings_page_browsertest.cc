// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/views/interaction/interaction_test_util_views.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(
    kWebContentsInteractionTestUtilCustomEventId);

const WebContentsInteractionTestUtil::DeepQuery kAskButton{
    "settings-ui", "settings-main", "settings-basic-page",
    "settings-privacy-page", "#notification-ask-radio-button"};

const WebContentsInteractionTestUtil::DeepQuery kQuietButton{
    "settings-ui", "settings-main", "settings-basic-page",
    "settings-privacy-page", "#notification-ask-quiet"};

const WebContentsInteractionTestUtil::DeepQuery kCpssButton{
    "settings-ui", "settings-main", "settings-basic-page",
    "settings-privacy-page", "#notification-ask-cpss"};

const WebContentsInteractionTestUtil::DeepQuery kLoudButton{
    "settings-ui", "settings-main", "settings-basic-page",
    "settings-privacy-page", "#notification-ask-loud"};

const WebContentsInteractionTestUtil::DeepQuery kBlockButton{
    "settings-ui", "settings-main", "settings-basic-page",
    "settings-privacy-page", "#notification-block"};

}  // namespace

class PredictionSettingsPageBrowserTest : public InteractiveBrowserTest {
 public:

  ~PredictionSettingsPageBrowserTest() override = default;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  ui::ElementContext context() const {
    return browser()->window()->GetElementContext();
  }

  GURL GetNotificationSettingsUrl() {
    return GURL("chrome://settings/content/notifications");
  }

  auto WaitFor(const WebContentsInteractionTestUtil::DeepQuery& element,
               const ui::InteractionSequence::StepType type =
                   ui::InteractionSequence::StepType::kShown) {
    return ui::InteractionSequence::StepBuilder()
        .SetElementID(kWebContentsElementId)
        .SetStartCallback(base::BindLambdaForTesting(
            // FIXME: type has to be copied.
            [&, type](ui::InteractionSequence*,
                      ui::TrackedElement* tracked_elem) {
              auto* util =
                  tracked_elem->AsA<TrackedElementWebContents>()->owner();

              WebContentsInteractionTestUtil::StateChange state_change;
              state_change.where = element;
              if (type == ui::InteractionSequence::StepType::kShown) {
                state_change.test_function =
                    "(el, err) => el && el.offsetParent !== null";
              } else if (type == ui::InteractionSequence::StepType::kHidden) {
                state_change.test_function =
                    "(el, err) => !el || el.offsetParent === null";
              }
              state_change.event = kWebContentsInteractionTestUtilCustomEventId;
              util->SendEventOnStateChange(state_change);
            }))
        .Build();
  }

  auto SetPrefs(bool isAllowed, bool isQuiet, bool isCpss) {
    DCHECK(!(isCpss && isQuiet));
    return ui::InteractionSequence::StepBuilder()
        .SetElementID(kWebContentsElementId)
        .SetStartCallback(base::BindLambdaForTesting(
            [&, isAllowed, isQuiet, isCpss](ui::InteractionSequence* sequence,
                                            ui::TrackedElement* element) {
              auto* pref_service = browser()->profile()->GetPrefs();
              auto* settings_map = HostContentSettingsMapFactory::GetForProfile(
                  browser()->profile());

              settings_map->SetDefaultContentSetting(
                  ContentSettingsType::NOTIFICATIONS,
                  isAllowed ? CONTENT_SETTING_ASK : CONTENT_SETTING_BLOCK);

              pref_service->SetBoolean(
                  prefs::kEnableQuietNotificationPermissionUi, isQuiet);
              pref_service->SetBoolean(prefs::kEnableNotificationCPSS, isCpss);
            }))
        .Build();
  }

  auto TestBlockStatePreferences() {
    return ui::InteractionSequence::StepBuilder()
        .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                 kWebContentsInteractionTestUtilCustomEventId)
        .SetElementID(kWebContentsElementId)
        .SetStartCallback(
            base::BindLambdaForTesting([&](ui::InteractionSequence* sequence,
                                           ui::TrackedElement* element) {
              auto util =
                  WebContentsInteractionTestUtil::ForExistingTabInBrowser(
                      browser(), kWebContentsElementId);

              util->EvaluateAt(kBlockButton,
                               "blockButton => blockButton.click()");
              auto* settings_map = HostContentSettingsMapFactory::GetForProfile(
                  browser()->profile());
              EXPECT_EQ(CONTENT_SETTING_BLOCK,
                        settings_map->GetDefaultContentSetting(
                            ContentSettingsType::NOTIFICATIONS, nullptr));
              auto* pref_service = browser()->profile()->GetPrefs();
              EXPECT_FALSE(pref_service->GetBoolean(
                  prefs::kEnableQuietNotificationPermissionUi));
              EXPECT_FALSE(
                  pref_service->GetBoolean(prefs::kEnableNotificationCPSS));
            }))
        .Build();
  }

  auto TestAskStatePreferences() {
    return ui::InteractionSequence::StepBuilder()
        .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                 kWebContentsInteractionTestUtilCustomEventId)
        .SetElementID(kWebContentsElementId)
        .SetStartCallback(
            base::BindLambdaForTesting([&](ui::InteractionSequence* sequence,
                                           ui::TrackedElement* element) {
              auto util =
                  WebContentsInteractionTestUtil::ForExistingTabInBrowser(
                      browser(), kWebContentsElementId);

              util->EvaluateAt(kAskButton, "askButton => askButton.click()");
              auto* settings_map = HostContentSettingsMapFactory::GetForProfile(
                  browser()->profile());
              EXPECT_EQ(CONTENT_SETTING_ASK,
                        settings_map->GetDefaultContentSetting(
                            ContentSettingsType::NOTIFICATIONS, nullptr));
              auto* pref_service = browser()->profile()->GetPrefs();
              EXPECT_FALSE(pref_service->GetBoolean(
                  prefs::kEnableQuietNotificationPermissionUi));
              EXPECT_TRUE(
                  pref_service->GetBoolean(prefs::kEnableNotificationCPSS));

              const WebContentsInteractionTestUtil::DeepQuery kAskQuiet{
                  "settings-ui", "settings-main", "settings-basic-page",
                  "settings-privacy-page", "#notification-ask-quiet"};
              util->EvaluateAt(kAskQuiet, "kAskQuiet => kAskQuiet.click()");
              EXPECT_EQ(CONTENT_SETTING_ASK,
                        settings_map->GetDefaultContentSetting(
                            ContentSettingsType::NOTIFICATIONS, nullptr));
              EXPECT_TRUE(pref_service->GetBoolean(
                  prefs::kEnableQuietNotificationPermissionUi));
              EXPECT_FALSE(
                  pref_service->GetBoolean(prefs::kEnableNotificationCPSS));

              util->EvaluateAt(kLoudButton, "loudButton => loudButton.click()");
              EXPECT_EQ(CONTENT_SETTING_ASK,
                        settings_map->GetDefaultContentSetting(
                            ContentSettingsType::NOTIFICATIONS, nullptr));
              EXPECT_FALSE(pref_service->GetBoolean(
                  prefs::kEnableQuietNotificationPermissionUi));
              EXPECT_FALSE(
                  pref_service->GetBoolean(prefs::kEnableNotificationCPSS));

              const WebContentsInteractionTestUtil::DeepQuery kAskCpss{
                  "settings-ui", "settings-main", "settings-basic-page",
                  "settings-privacy-page", "#notification-ask-cpss"};
              util->EvaluateAt(kAskCpss, "kAskCpss => kAskCpss.click()");
              EXPECT_EQ(CONTENT_SETTING_ASK,
                        settings_map->GetDefaultContentSetting(
                            ContentSettingsType::NOTIFICATIONS, nullptr));
              EXPECT_FALSE(pref_service->GetBoolean(
                  prefs::kEnableQuietNotificationPermissionUi));
              EXPECT_TRUE(
                  pref_service->GetBoolean(prefs::kEnableNotificationCPSS));
            }))
        .Build();
  }

  auto TestRadioGroupState(bool expectedAskButtonChecked,
                           bool expectedAskSubGroupVisible,
                           bool expectedQuietButtonChecked,
                           bool expectedCpssButtonChecked,
                           bool expectedLoudButtonChecked) {
    return ui::InteractionSequence::StepBuilder()
        .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                 kWebContentsInteractionTestUtilCustomEventId)
        .SetElementID(kWebContentsElementId)
        .SetStartCallback(base::BindLambdaForTesting(
            [&, expectedAskButtonChecked, expectedAskSubGroupVisible,
             expectedQuietButtonChecked, expectedCpssButtonChecked,
             expectedLoudButtonChecked](ui::InteractionSequence* sequence,
                                        ui::TrackedElement* element) {
              auto util =
                  WebContentsInteractionTestUtil::ForExistingTabInBrowser(
                      browser(), kWebContentsElementId);

              auto isAskButtonChecked = util->EvaluateAt(
                  kAskButton, "askButton => askButton.checked");
              EXPECT_EQ(true, isAskButtonChecked.is_bool());
              EXPECT_EQ(expectedAskButtonChecked, isAskButtonChecked.GetBool());

              auto isBlockButtonChecked = util->EvaluateAt(
                  kBlockButton, "blockButton => blockButton.checked");
              EXPECT_EQ(true, isBlockButtonChecked.is_bool());
              EXPECT_EQ(!expectedAskButtonChecked,
                        isBlockButtonChecked.GetBool());

              auto isQuietButtonVisible = util->EvaluateAt(
                  kQuietButton,
                  "quietButton => quietButton.offsetParent !== null");
              EXPECT_EQ(true, isQuietButtonVisible.is_bool());
              EXPECT_EQ(expectedAskSubGroupVisible,
                        isQuietButtonVisible.GetBool());
              auto isQuietButtonChecked = util->EvaluateAt(
                  kQuietButton, "quietButton => quietButton.checked");
              EXPECT_EQ(true, isQuietButtonChecked.is_bool());
              EXPECT_EQ(expectedQuietButtonChecked,
                        isQuietButtonChecked.GetBool());

              auto isCpssButtonVisible = util->EvaluateAt(
                  kCpssButton,
                  "cpssButton => cpssButton.offsetParent !== null");
              EXPECT_EQ(true, isCpssButtonVisible.is_bool());
              EXPECT_EQ(expectedAskSubGroupVisible,
                        isCpssButtonVisible.GetBool());
              auto isCpssButtonChecked = util->EvaluateAt(
                  kCpssButton, "cpssButton => cpssButton.checked");
              EXPECT_EQ(true, isCpssButtonChecked.is_bool());
              EXPECT_EQ(expectedCpssButtonChecked,
                        isCpssButtonChecked.GetBool());

              auto isLoudButtonVisible = util->EvaluateAt(
                  kLoudButton,
                  "loudButton => loudButton.offsetParent !== null");
              EXPECT_EQ(true, isLoudButtonVisible.is_bool());
              EXPECT_EQ(expectedAskSubGroupVisible,
                        isLoudButtonVisible.GetBool());
              auto isLoudButtonChecked = util->EvaluateAt(
                  kLoudButton, "loudButton => loudButton.checked");
              EXPECT_EQ(true, isLoudButtonChecked.is_bool());
              EXPECT_EQ(expectedLoudButtonChecked,
                        isLoudButtonChecked.GetBool());
            }))
        .Build();
  }
};

IN_PROC_BROWSER_TEST_F(PredictionSettingsPageBrowserTest,
                       TestNotificationSettingsPrefs) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  auto util = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);

  util->LoadPage(GURL("chrome://settings/content/notifications"));

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          .AddStep(WaitFor(kBlockButton))
          .AddStep(TestBlockStatePreferences())
          .AddStep(
              WaitFor(kLoudButton, ui::InteractionSequence::StepType::kHidden))
          .AddStep(TestAskStatePreferences())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(PredictionSettingsPageBrowserTest,
                       TestDefaultRadioGroupState) {
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetNotificationSettingsUrl()),
      WaitFor(kBlockButton),
      TestRadioGroupState(true, true, false, true, false));
}

IN_PROC_BROWSER_TEST_F(PredictionSettingsPageBrowserTest,
                       TestQuiteRadioGroupState) {
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      SetPrefs(true, true, false),
      NavigateWebContents(kWebContentsElementId, GetNotificationSettingsUrl()),
      WaitFor(kBlockButton),
      TestRadioGroupState(true, true, true, false, false));
}

IN_PROC_BROWSER_TEST_F(PredictionSettingsPageBrowserTest,
                       TestLoudRadioGroupState) {
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      SetPrefs(true, false, false),
      NavigateWebContents(kWebContentsElementId, GetNotificationSettingsUrl()),
      WaitFor(kBlockButton),
      TestRadioGroupState(true, true, false, false, true));
}

IN_PROC_BROWSER_TEST_F(PredictionSettingsPageBrowserTest,
                       TestBlockRadioGroupState) {
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      SetPrefs(false, false, false),
      NavigateWebContents(kWebContentsElementId, GetNotificationSettingsUrl()),
      WaitFor(kBlockButton),
      TestRadioGroupState(false, false, false, false, false));
}
