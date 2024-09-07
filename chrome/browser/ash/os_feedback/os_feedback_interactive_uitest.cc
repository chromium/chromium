// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/test/gtest_tags.h"
#include "build/build_config.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"
#include "components/feedback/features.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

constexpr char kOsFeedbackUrl[] = "chrome://os-feedback";
constexpr char kBlankUrl[] = "about:blank";

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsFeedbackWebContentsId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabWebContentsId);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kAboutChromeOsUrl[] = "chrome://os-settings/help";
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAboutChromeOsWebContentsId);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

class OsFeedbackInteractiveUiTest : public InteractiveAshTest {
 public:
  OsFeedbackInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features =*/
        {::feedback::features::kSkipSendingFeedbackReportInTastTests},
        /*disabled_features =*/{});
  }
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();
    // Ensure the Feedback SWA is installed.
    InstallSystemApps();
  }

 protected:
  // Query to pierce through Shadow DOM to find the feedback page title.
  const DeepQuery kFeedbackSearchPageTitleQuery = {
      "feedback-flow",
      "search-page",
      "h1.page-title",
  };

  auto LaunchOsFeedbackApp() {
    return Do([&]() { CreateBrowserWindow(GURL(kOsFeedbackUrl)); });
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Query to pierce through Shadow DOM to find the send feedback link.
  const DeepQuery kReportIssueMenuItemQuery = {
      "os-settings-ui", "os-settings-main", "main-page-container",
      "os-about-page",  "#reportIssue",
  };

  auto LaunchAboutChromeOsPage() {
    return Do([&]() { CreateBrowserWindow(GURL(kAboutChromeOsUrl)); });
  }

  // Enters lower-case text into the focused html input element.
  auto EnterLowerCaseText(const std::string& text) {
    return Do([&]() {
      for (char c : text) {
        ui_controls::SendKeyPress(
            /*window=*/nullptr,
            static_cast<ui::KeyboardCode>(ui::VKEY_A + (c - 'a')),
            /*control=*/false, /*shift=*/false,
            /*alt=*/false, /*command=*/false);
      }
    });
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // Wait for the Feedback SWA to be present.
  auto WaitForFeedbackSWAReady(const ui::ElementIdentifier& element_id) {
    return Steps(
        Log("Waiting for the os feedback SWA to load"), WaitForShow(element_id),
        WaitForWebContentsReady(element_id),
        InAnyContext(EnsurePresent(element_id, kFeedbackSearchPageTitleQuery)),
        WaitForElementTextContains(element_id, kFeedbackSearchPageTitleQuery,
                                   "Send feedback"));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OsFeedbackInteractiveUiTest,
                       OpenFromAltShiftIShortCuts) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-3f028d06-0100-4b5b-b1f3-99ceeaf3d62b");

  ui::Accelerator open_feedback_accelerator(
      ui::VKEY_I, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);

  ASSERT_TRUE(CreateBrowserWindow(GURL(kBlankUrl)));

  RunTestSequence(
      InstrumentTab(kNewTabWebContentsId),
      InstrumentNextTab(kOsFeedbackWebContentsId, AnyBrowser()),
      Log("Pressing Alt+Shift+I"),
      SendAccelerator(kNewTabWebContentsId, open_feedback_accelerator),
      WaitForFeedbackSWAReady(kOsFeedbackWebContentsId));
}

// crbug.com/1517839
#if defined(MEMORY_SANITIZER)
#define MAYBE_SubmitFeedbackThenExit DISABLED_SubmitFeedbackThenExit
#else
#define MAYBE_SubmitFeedbackThenExit SubmitFeedbackThenExit
#endif
IN_PROC_BROWSER_TEST_F(OsFeedbackInteractiveUiTest,
                       MAYBE_SubmitFeedbackThenExit) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-3f028d06-0100-4b5b-b1f3-99ceeaf3d62b");
  // Query to pierce through Shadow DOM to find the description element on the
  // search page.
  const DeepQuery kDescriptionTextQuery = {"feedback-flow", "search-page",
                                           "textarea#descriptionText"};
  // Query to pierce through Shadow DOM to find the continue button on the
  // search page.
  const DeepQuery kContinueButtonQuery = {"feedback-flow", "search-page",
                                          "cr-button#buttonContinue"};
  // Query to pierce through Shadow DOM to find the send button on the share
  // data page.
  const DeepQuery kSendReportButtonQuery = {"feedback-flow", "share-data-page",
                                            "cr-button#buttonSend"};
  // Query to pierce through Shadow DOM to find the dont button on the
  // confirmation page.
  const DeepQuery kDoneButtonQuery = {"feedback-flow", "confirmation-page",
                                      "cr-button#buttonDone"};

  ASSERT_TRUE(CreateBrowserWindow(GURL(kBlankUrl)));

  RunTestSequence(
      InstrumentTab(kNewTabWebContentsId),
      InstrumentNextTab(kOsFeedbackWebContentsId, AnyBrowser()),
      Log("Launching the os feedback app"), LaunchOsFeedbackApp(),
      WaitForWebContentsReady(kOsFeedbackWebContentsId, GURL(kOsFeedbackUrl)),

      Log("Entering fake description"),
      ExecuteJsAt(kOsFeedbackWebContentsId, kDescriptionTextQuery,
                  " el => el.value = 'Testing only - please ignore'"),

      Log("Clicking the continue button"),
      WaitForElementToRender(kOsFeedbackWebContentsId, kContinueButtonQuery),
      ClickElement(kOsFeedbackWebContentsId, kContinueButtonQuery),

      Log("Clicking the send button"),
      WaitForElementToRender(kOsFeedbackWebContentsId, kSendReportButtonQuery),
      ClickElement(kOsFeedbackWebContentsId, kSendReportButtonQuery),

      Log("Clicking the done button"),
      WaitForElementToRender(kOsFeedbackWebContentsId, kDoneButtonQuery),
      ClickElement(kOsFeedbackWebContentsId, kDoneButtonQuery),

      Log("Waiting for the feedback app to exit"),
      WaitForHide(kOsFeedbackWebContentsId));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// The send report link shows on the About ChromeOS page only for Google brands.
IN_PROC_BROWSER_TEST_F(OsFeedbackInteractiveUiTest, OpenFromAboutChromeOsPage) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-3f028d06-0100-4b5b-b1f3-99ceeaf3d62b");

  RunTestSequence(
      InstrumentNextTab(kAboutChromeOsWebContentsId, AnyBrowser()),
      Log("Opening the about ChromeOS page"), LaunchAboutChromeOsPage(),
      WaitForWebContentsReady(kAboutChromeOsWebContentsId,
                              GURL(kAboutChromeOsUrl)),
      Log("Waiting for the send feedback link ready"),
      WaitForElementExists(kAboutChromeOsWebContentsId,
                           kReportIssueMenuItemQuery),
      InstrumentNextTab(kOsFeedbackWebContentsId, AnyBrowser()),
      Log("Clicking the send feedback link"),
      ClickElement(kAboutChromeOsWebContentsId, kReportIssueMenuItemQuery),
      WaitForFeedbackSWAReady(kOsFeedbackWebContentsId));
}

IN_PROC_BROWSER_TEST_F(OsFeedbackInteractiveUiTest, OpenFromSetingsSearch) {
  // Query to pierce through Shadow DOM to find the search input element.
  const DeepQuery kSearchInputElementQuery = {
      "os-settings-ui",          "settings-toolbar", "os-settings-search-box",
      "cr-toolbar-search-field", "#searchInput",
  };
  // Query to pierce through Shadow DOM to find the selected search result row.
  const DeepQuery kSelectedSearchResultRowQuery = {
      "os-settings-ui",
      "settings-toolbar",
      "os-settings-search-box",
      "os-search-result-row[selected]",
  };

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsWebContentsId);
  GURL os_settings_url("chrome://os-settings");

  RunTestSequence(
      Log("Opening the Os Settings app"),
      InstrumentNextTab(kOsSettingsWebContentsId, AnyBrowser()),
      Do([&]() { CreateBrowserWindow(GURL(os_settings_url)); }),
      WaitForWebContentsReady(kOsSettingsWebContentsId, GURL(os_settings_url)),
      WaitForElementExists(kOsSettingsWebContentsId, kSearchInputElementQuery),

      Log("Searching for \"send feedback\""),
      ExecuteJsAt(kOsSettingsWebContentsId, kSearchInputElementQuery,
                  " el => el.focus()"),
      EnterLowerCaseText("send feedback"),

      Log("Clicking the selected search result"),
      WaitForElementExists(kOsSettingsWebContentsId,
                           kSelectedSearchResultRowQuery),
      ClickElement(kOsSettingsWebContentsId, kSelectedSearchResultRowQuery),

      Log("Waiting for the send feedback link ready"),
      WaitForElementExists(kOsSettingsWebContentsId, kReportIssueMenuItemQuery),

      Log("Clicking the send feedback link"),
      ClickElement(kOsSettingsWebContentsId, kReportIssueMenuItemQuery),

      InstrumentNextTab(kOsFeedbackWebContentsId, AnyBrowser()),
      WaitForFeedbackSWAReady(kOsFeedbackWebContentsId));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace
}  // namespace ash
