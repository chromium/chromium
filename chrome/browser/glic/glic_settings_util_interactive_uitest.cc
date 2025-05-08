// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_settings_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/common/user_education_features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_tracker.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kThirdTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSettingsTab);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kOsToggleIsVisible);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kKeyboardShortcutIsVisible);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kBubbleIsVisible);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kBubbleIsHidden);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kBasicPageIsVisible);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kGlicSectionIsVisible);

auto ElementIsVisibleStateChange(
    ui::CustomElementEventType event,
    WebContentsInteractionTestUtil::DeepQuery query) {
  WebContentsInteractionTestUtil::StateChange element_is_visible;
  element_is_visible.event = event;
  element_is_visible.where = query;
  element_is_visible.type =
      WebContentsInteractionTestUtil::StateChange::Type::kExists;
  return element_is_visible;
}

auto ElementIsHiddenStateChange(
    ui::CustomElementEventType event,
    WebContentsInteractionTestUtil::DeepQuery query) {
  WebContentsInteractionTestUtil::StateChange element_is_hidden;
  element_is_hidden.event = event;
  element_is_hidden.where = query;
  element_is_hidden.type =
      WebContentsInteractionTestUtil::StateChange::Type::kDoesNotExist;
  return element_is_hidden;
}
}  // namespace

class GlicSettingsUtilUiTest
    : public glic::test::InteractiveGlicFeaturePromoTest {
 public:
  GlicSettingsUtilUiTest() = default;
  ~GlicSettingsUtilUiTest() override = default;

  void SetUpOnMainThread() override {
    glic::test::InteractiveGlicFeaturePromoTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(
        glic::prefs::kGlicLauncherEnabled, true);
  }

  // Navigates the initial tab to the glic settings page using
  // chrome::ShowSettingsSubPage, opens 2 more tabs, then calls f and verifies
  // that only 3 tabs are open.
  auto VerifyOpensGlicSettings(auto f) {
    return Steps(
        InstrumentTab(kFirstTab), Do([this] {
          chrome::ShowSettingsSubPage(browser(), chrome::kGlicSettingsSubpage);
        }),
        WaitForWebContentsNavigation(
            kFirstTab, chrome::GetSettingsUrl(chrome::kGlicSettingsSubpage)),
        AddInstrumentedTab(kSecondTab , GURL(chrome::kChromeUICreditsURL)),
        AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUIAboutURL)),
        Do([this, f] { f(browser()->profile()); }), InstrumentTab(kSettingsTab),
        WaitForWebContentsReady(
            kSettingsTab, chrome::GetSettingsUrl(chrome::kGlicSettingsSubpage)),
        CheckResult(
            [this] { return browser()->tab_strip_model()->GetTabCount(); }, 3,
            "CheckTabCount"));
  }

  auto SetFRECompletion(glic::prefs::FreStatus status) {
    return Steps(Do(
        [this, status] { glic_test_environment().SetFRECompletion(status); }));
  }

  auto NavigateToSettingsPage(std::string_view path) {
    return Steps(
        Do([this, path] { chrome::ShowSettingsSubPage(browser(), path); }),
        WaitForWebContentsNavigation(kFirstTab, chrome::GetSettingsUrl(path)));
  }

  auto ReloadTab(ui::ElementIdentifier id) {
    return Steps(Do([this]() {
                   chrome::Reload(browser(),
                                  WindowOpenDisposition::CURRENT_TAB);
                 }),
                 WaitForWebContentsNavigation(id));
  }

  const DeepQuery kOsToggleHelpBubbleQuery{"settings-ui",
                                           "settings-main",
                                           "settings-basic-page",
                                           "settings-glic-page",
                                           "#launcherToggle",
                                           "help-bubble",
                                           "#close"};

  const DeepQuery kKeyboardShortcutHelpBubbleQuery{
      "settings-ui",        "settings-main", "settings-basic-page",
      "settings-glic-page", "help-bubble",   "#close"};

  const DeepQuery kOpenSettingsButton = {"#openGlicSettings"};
};

IN_PROC_BROWSER_TEST_F(GlicSettingsUtilUiTest, OpenSettings) {
  RunTestSequence(VerifyOpensGlicSettings(glic::OpenGlicSettingsPage));
}

IN_PROC_BROWSER_TEST_F(GlicSettingsUtilUiTest, OpenOsToggleSetting) {
  RunTestSequence(
      VerifyOpensGlicSettings(glic::OpenGlicOsToggleSetting),
      WaitForStateChange(
          kFirstTab, ElementIsVisibleStateChange(kBubbleIsVisible,
                                                    kOsToggleHelpBubbleQuery)));
}

// TODO(crbug.com/401248290): Flaky on "Linux MSan Tests" bot.
#if BUILDFLAG(IS_LINUX) && defined(MEMORY_SANITIZER)
#define MAYBE_OpenKeyboardShortcutSetting DISABLED_OpenKeyboardShortcutSetting
#else
#define MAYBE_OpenKeyboardShortcutSetting OpenKeyboardShortcutSetting
#endif
IN_PROC_BROWSER_TEST_F(GlicSettingsUtilUiTest,
                       MAYBE_OpenKeyboardShortcutSetting) {
  RunTestSequence(
      VerifyOpensGlicSettings(glic::OpenGlicKeyboardShortcutSetting),
      WaitForStateChange(kFirstTab, ElementIsVisibleStateChange(
                                           kBubbleIsVisible,
                                           kKeyboardShortcutHelpBubbleQuery)));
}

IN_PROC_BROWSER_TEST_F(GlicSettingsUtilUiTest, ThrottleOpenOsToggleSetting) {
  for (int i = 0; i < user_education::features::GetNewBadgeFeatureUsedCount();
       i++) {
    UserEducationService::MaybeNotifyNewBadgeFeatureUsed(browser()->profile(),
                                                         features::kGlic);
  }
  RunTestSequence(
      VerifyOpensGlicSettings(glic::OpenGlicOsToggleSetting),
      WaitForStateChange(
          kFirstTab,
          ElementIsVisibleStateChange(
              kOsToggleIsVisible,
              {"settings-ui", "settings-main", "settings-basic-page",
               "settings-glic-page", "#launcherToggle"})),
      WaitForStateChange(kFirstTab,
                         ElementIsHiddenStateChange(kBubbleIsHidden,
                                                    kOsToggleHelpBubbleQuery)));
}

IN_PROC_BROWSER_TEST_F(GlicSettingsUtilUiTest,
                       ThrottleOpenKeyboardShortcutSetting) {
  for (int i = 0; i < user_education::features::GetNewBadgeFeatureUsedCount();
       i++) {
    UserEducationService::MaybeNotifyNewBadgeFeatureUsed(
        browser()->profile(), features::kGlicKeyboardShortcutNewBadge);
  }
  RunTestSequence(
      VerifyOpensGlicSettings(glic::OpenGlicKeyboardShortcutSetting),
      WaitForStateChange(kSettingsTab,
                         ElementIsVisibleStateChange(
                             kKeyboardShortcutIsVisible,
                             {"settings-ui", "settings-main",
                              "settings-basic-page", "settings-glic-page",
                              "#mainShortcutSetting", ".shortcut-input"})),
      WaitForStateChange(kSettingsTab, ElementIsHiddenStateChange(
                                           kBubbleIsHidden,
                                           kKeyboardShortcutHelpBubbleQuery)));
}

IN_PROC_BROWSER_TEST_F(GlicSettingsUtilUiTest, OpenSettingsFromGlicUi) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached,
                     GlicInstrumentMode::kHostAndContents),
      InstrumentNextTab(kSettingsTab),
      ClickMockGlicElement(kOpenSettingsButton),
      WaitForWebContentsReady(
          kSettingsTab, chrome::GetSettingsUrl(chrome::kGlicSettingsSubpage)));
}

IN_PROC_BROWSER_TEST_F(GlicSettingsUtilUiTest,
                       RefreshSettingsAfterAcceptingFRE) {
  // This specifies the sequence of Polymer elements required to locate the
  // "more actions" button in the Downloads page.
  const DeepQuery kPathToBasicPage{"settings-ui", "settings-main",
                                   "settings-basic-page"};
  const DeepQuery kPathToGlicSection{"settings-ui", "settings-main",
                                     "settings-basic-page",
                                     "settings-section[section=glicSection]"};
  RunTestSequence(
      InstrumentTab(kFirstTab),
      SetFRECompletion(glic::prefs::FreStatus::kNotStarted),
      NavigateToSettingsPage(chrome::kExperimentalAISettingsSubPage),
      WaitForStateChange(kFirstTab, ElementIsVisibleStateChange(
                                        kBasicPageIsVisible, kPathToBasicPage)),
      WaitForStateChange(kFirstTab,
                         ElementIsHiddenStateChange(kGlicSectionIsVisible,
                                                    kPathToGlicSection)),
      SetFRECompletion(glic::prefs::FreStatus::kCompleted),
      ReloadTab(kFirstTab),
      WaitForStateChange(kFirstTab, ElementIsVisibleStateChange(
                                        kBasicPageIsVisible, kPathToBasicPage)),
      WaitForStateChange(kFirstTab,
                         ElementIsVisibleStateChange(kGlicSectionIsVisible,
                                                     kPathToGlicSection)));
}
