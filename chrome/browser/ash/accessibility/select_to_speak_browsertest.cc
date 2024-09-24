// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/accessibility/ui/accessibility_focus_ring_controller_impl.h"
#include "ash/accessibility/ui/accessibility_focus_ring_layer.h"
#include "ash/accessibility/ui/accessibility_highlight_layer.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/accessibility/select_to_speak/select_to_speak_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_util.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/accessibility/accessibility_feature_browsertest.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/automation_test_utils.h"
#include "chrome/browser/ash/accessibility/fullscreen_magnifier_test_helper.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/accessibility/select_to_speak_test_utils.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/process_manager.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/compositor/layer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "url/url_constants.h"

namespace ash {
namespace {
constexpr char kSpeechDurationMetric[] =
    "Accessibility.CrosSelectToSpeak.SpeechDuration";
}  // namespace

class SelectToSpeakTest : public AccessibilityFeatureBrowserTest {
 public:
  SelectToSpeakTest(const SelectToSpeakTest&) = delete;
  SelectToSpeakTest& operator=(const SelectToSpeakTest&) = delete;

  void OnFocusRingChanged() {
    if (loop_runner_ && loop_runner_->running()) {
      loop_runner_->Quit();
    }
  }

  void OnHighlightsAdded() {
    if (highlights_runner_ && highlights_runner_->running()) {
      highlights_runner_->Quit();
    }
  }

  void SetSelectToSpeakState() {
    if (tray_loop_runner_ && tray_loop_runner_->running()) {
      tray_loop_runner_->Quit();
    }
  }

  void ExpectTotalSpeechDurationSamples(int expected_count) {
    histogram_tester_.ExpectTotalCount(kSpeechDurationMetric, expected_count);
  }

 protected:
  SelectToSpeakTest() {}
  ~SelectToSpeakTest() override {}

  // Note that we do not enable Select to Speak in the SetUp method because
  // tests are less flaky if we load the page URL before loading up the
  // Select to Speak extension.
  void SetUpOnMainThread() override {
    AccessibilityFeatureBrowserTest::SetUpOnMainThread();

    ASSERT_FALSE(AccessibilityManager::Get()->IsSelectToSpeakEnabled());
    console_observer_ = std::make_unique<ExtensionConsoleErrorObserver>(
        AccessibilityManager::Get()->profile(),
        extension_misc::kSelectToSpeakExtensionId);

    tray_test_api_ = SystemTrayTestApi::Create();
    aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
    generator_ = std::make_unique<ui::test::EventGenerator>(root_window);

    automation_test_utils_ = std::make_unique<AutomationTestUtils>(
        extension_misc::kSelectToSpeakExtensionId);

    NavigateToUrl(GURL(url::kAboutBlankURL));

    // Pretend that enhanced network voices dialog has been accepted so that the
    // dialog does not block.
    AccessibilityManager::Get()->profile()->GetPrefs()->SetBoolean(
        prefs::kAccessibilitySelectToSpeakEnhancedVoicesDialogShown, true);
  }

  test::SpeechMonitor sm_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  std::unique_ptr<SystemTrayTestApi> tray_test_api_;
  std::unique_ptr<ExtensionConsoleErrorObserver> console_observer_;
  std::unique_ptr<AutomationTestUtils> automation_test_utils_;

  gfx::Rect GetWebContentsBounds(const std::string& url) const {
    // TODO(katie): Find a way to get the exact bounds programmatically.
    gfx::Rect bounds = automation_test_utils_->GetBoundsOfRootWebArea(url);
    return bounds;
  }

  void LoadURLAndSelectToSpeak(const std::string& url) {
    if (!AccessibilityManager::Get()->IsSelectToSpeakEnabled()) {
      sts_test_utils::TurnOnSelectToSpeakForTest(
          AccessibilityManager::Get()->profile());
    }
    automation_test_utils_->SetUpTestSupport();
    NavigateToUrl(GURL(url));
    automation_test_utils_->WaitForPageLoad(url);
  }

  virtual void ActivateSelectToSpeakInWindowBounds(const std::string& url) {
    // Load the URL before Select to Speak to avoid flakes.
    LoadURLAndSelectToSpeak(url);

    sts_test_utils::StartSelectToSpeakInBrowserWithUrl(
        url, automation_test_utils_.get(), generator_.get());
  }

  // Set document selection to be the node with text `text` using Automation
  // API.
  void SelectNodeWithText(const std::string& text) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string script =
        base::StringPrintf(R"JS(
        (async function() {
          chrome.automation.getDesktop(desktop => {
            const textNode = desktop.find(
                {role: 'staticText', attributes: {name: '%s'}});
            chrome.automation.setDocumentSelection({
              anchorObject: textNode,
              anchorOffset: %d,
              focusObject: textNode,
              focusOffset: %d,
            });
            const callback = (() => {
              desktop.removeEventListener('documentSelectionChanged',
                  callback, /*capture=*/false);
              chrome.test.sendScriptResult('ready');
            });
            desktop.addEventListener('documentSelectionChanged',
                callback, /*capture=*/false);
          });
        })();
      )JS",
                           text.c_str(), 0, static_cast<int>(text.size()));
    base::Value result =
        extensions::browsertest_util::ExecuteScriptInBackgroundPage(
            AccessibilityManager::Get()->profile(),
            extension_misc::kSelectToSpeakExtensionId, script);
    ASSERT_EQ("ready", result);
  }

  void PrepareToWaitForHighlightAdded() {
    highlights_runner_ = std::make_unique<base::RunLoop>();
    base::RepeatingCallback<void()> callback = base::BindRepeating(
        &SelectToSpeakTest::OnHighlightsAdded, GetWeakPtr());
    AccessibilityManager::Get()->SetHighlightsObserverForTest(callback);
  }

  void WaitForHighlightAdded() {
    DCHECK(highlights_runner_);
    highlights_runner_->Run();
    highlights_runner_ = nullptr;
  }

  void PrepareToWaitForSelectToSpeakStatusChanged() {
    tray_loop_runner_ = std::make_unique<base::RunLoop>();
  }

  // Blocks until the select-to-speak tray status is changed.
  void WaitForSelectToSpeakStatusChanged() {
    tray_loop_runner_->Run();
    tray_loop_runner_ = nullptr;
  }

  void TapSelectToSpeakTray() {
    PrepareToWaitForSelectToSpeakStatusChanged();
    tray_test_api_->TapSelectToSpeakTray();
    WaitForSelectToSpeakStatusChanged();
  }

  void PrepareToWaitForFocusRingChanged() {
    loop_runner_ = std::make_unique<base::RunLoop>();
  }

  // Blocks until the focus ring is changed.
  void WaitForFocusRingChanged() {
    loop_runner_->Run();
    loop_runner_ = nullptr;
  }

  base::WeakPtr<SelectToSpeakTest> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void ExecuteJavaScriptAsync(const std::string& script) {
    content::ExecuteScriptAsync(
        browser()->tab_strip_model()->GetActiveWebContents(), script);
  }

 private:
  std::unique_ptr<base::RunLoop> loop_runner_;
  std::unique_ptr<base::RunLoop> highlights_runner_;
  std::unique_ptr<base::RunLoop> tray_loop_runner_;
  base::WeakPtrFactory<SelectToSpeakTest> weak_ptr_factory_{this};
};

class SelectToSpeakTestWithVoiceSwitching : public SelectToSpeakTest {
 protected:
  void SetUpOnMainThread() override {
    PrefService* prefs = AccessibilityManager::Get()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kAccessibilitySelectToSpeakVoiceSwitching, true);
    SelectToSpeakTest::SetUpOnMainThread();
  }
};

class SelectToSpeakTestWithMagnifierFollowing : public SelectToSpeakTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityMagnifierFollowsSts);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, SpeakStatusTray) {
  sts_test_utils::TurnOnSelectToSpeakForTest(
      AccessibilityManager::Get()->profile());
  gfx::Rect tray_bounds = Shell::Get()
                              ->GetPrimaryRootWindowController()
                              ->GetStatusAreaWidget()
                              ->unified_system_tray()
                              ->GetBoundsInScreen();

  // Hold down Search and click a few pixels into the status tray bounds.
  generator_->PressKey(ui::VKEY_LWIN, 0 /* flags */);
  generator_->MoveMouseTo(tray_bounds.x() + 8, tray_bounds.y() + 8);
  generator_->PressLeftButton();
  generator_->ReleaseLeftButton();
  generator_->ReleaseKey(ui::VKEY_LWIN, 0 /* flags */);

  sm_.ExpectSpeechPattern("*Status tray*");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, ActivatesWithTapOnSelectToSpeakTray) {
  LoadURLAndSelectToSpeak(
      "data:text/html;charset=utf-8,<p>This is some text</p>");

  base::RepeatingCallback<void()> callback = base::BindRepeating(
      &SelectToSpeakTest::SetSelectToSpeakState, GetWeakPtr());
  AccessibilityManager::Get()->SetSelectToSpeakStateObserverForTest(callback);
  // Click in the tray bounds to start 'selection' mode.
  TapSelectToSpeakTray();

  // We should be in "selection" mode, so clicking with the mouse should
  // start speech.
  gfx::Rect bounds = automation_test_utils_->GetNodeBoundsInRoot(
      "This is some text", "staticText");
  generator_->MoveMouseTo(bounds.x(), bounds.y());
  generator_->PressLeftButton();
  generator_->MoveMouseTo(bounds.x() + bounds.width(),
                          bounds.y() + bounds.height());
  generator_->ReleaseLeftButton();

  sm_.ExpectSpeechPattern("This is some text*");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, WorksWithTouchSelection) {
  LoadURLAndSelectToSpeak(
      "data:text/html;charset=utf-8,<p>This is some text</p>");

  base::RepeatingCallback<void()> callback = base::BindRepeating(
      &SelectToSpeakTest::SetSelectToSpeakState, GetWeakPtr());
  AccessibilityManager::Get()->SetSelectToSpeakStateObserverForTest(callback);

  // Click in the tray bounds to start 'selection' mode.
  TapSelectToSpeakTray();

  // We should be in "selection" mode, so tapping and dragging should
  // start speech.
  gfx::Rect bounds = automation_test_utils_->GetNodeBoundsInRoot(
      "This is some text", "staticText");
  generator_->PressTouch(gfx::Point(bounds.x(), bounds.y()));
  generator_->PressMoveAndReleaseTouchTo(bounds.x() + bounds.width(),
                                         bounds.y() + bounds.height());
  generator_->ReleaseLeftButton();

  sm_.ExpectSpeechPattern("This is some text*");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest,
                       WorksWithTouchSelectionOnNonPrimaryMonitor) {
  // Don't observe error messages.
  // An error message is observed consistently on MSAN, see crbug.com/1201212,
  // and flakily on other builds, see crbug.com/1213451.
  // Run the rest of this test on but don't try to catch console errors.
  // TODO: Figure out why the "unable to load tab" error is occurring
  // and bring back the console observer.
  console_observer_.reset();

  ShellTestApi shell_test_api;
  display::test::DisplayManagerTestApi(shell_test_api.display_manager())
      .UpdateDisplay("1+0-800x700,801+1-800x700");
  ASSERT_EQ(2u, shell_test_api.display_manager()->GetNumDisplays());
  display::test::DisplayManagerTestApi display_manager_test_api(
      shell_test_api.display_manager());

  display::Screen* screen = display::Screen::GetScreen();
  int64_t display2 = display_manager_test_api.GetSecondaryDisplay().id();
  screen->SetDisplayForNewWindows(display2);

  // Ctrl+N to open a new browser window. This will load on the new display.
  generator_->PressAndReleaseKey(ui::VKEY_N, ui::EF_CONTROL_DOWN);

  std::string url = "data:text/html;charset=utf-8,<p>This is some text</p>";
  NavigateToUrl(GURL(url));

  sts_test_utils::TurnOnSelectToSpeakForTest(
      AccessibilityManager::Get()->profile());
  automation_test_utils_->SetUpTestSupport();
  base::RepeatingCallback<void()> callback = base::BindRepeating(
      &SelectToSpeakTest::SetSelectToSpeakState, GetWeakPtr());
  AccessibilityManager::Get()->SetSelectToSpeakStateObserverForTest(callback);

  // Also waits for load complete.
  gfx::Rect bounds = GetWebContentsBounds(url);

  // Click in the tray bounds to start 'selection' mode.
  TapSelectToSpeakTray();
  // We should be in "selection" mode, so tapping and dragging should
  // start speech.
  generator_->PressTouch(gfx::Point(bounds.x(), bounds.y()));
  generator_->PressMoveAndReleaseTouchTo(bounds.x() + bounds.width(),
                                         bounds.y() + bounds.height());
  generator_->ReleaseLeftButton();

  sm_.ExpectSpeechPattern("This is some text*");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, SelectToSpeakTrayNotSpoken) {
  sts_test_utils::TurnOnSelectToSpeakForTest(
      AccessibilityManager::Get()->profile());

  base::RepeatingCallback<void()> callback = base::BindRepeating(
      &SelectToSpeakTest::SetSelectToSpeakState, GetWeakPtr());
  AccessibilityManager::Get()->SetSelectToSpeakStateObserverForTest(callback);

  // Tap it once to enter selection mode.
  TapSelectToSpeakTray();

  // Tap again to turn off selection mode.
  TapSelectToSpeakTray();

  // The next should be the first thing spoken -- the tray was not spoken.
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<p>This is some text</p>");

  sm_.ExpectSpeechPattern("This is some text*");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, SmoothlyReadsAcrossInlineUrl) {
  // Make sure an inline URL is read smoothly.
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<p>This is some text <a href=\"\">with a"
      " node</a> in the middle");
  // Should combine nodes in a paragraph into one utterance.
  // Includes some wildcards between words because there may be extra
  // spaces. Spaces are not pronounced, so extra spaces do not impact output.
  sm_.ExpectSpeechPattern("This is some text*with a node*in the middle*");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, SetsWordHighlights) {
  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();
  EXPECT_FALSE(controller->highlight_layer_for_testing());
  PrepareToWaitForHighlightAdded();
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<p>Highlight me");
  sm_.ExpectSpeechPattern("*Highlight me*");
  sm_.Replay();

  // Some highlighting should have occurred. OK to do this after speech as
  // Select to Speak refreshes the UI intermittently.
  WaitForHighlightAdded();

  // Check the highlight exists and the color is as expected.
  AccessibilityHighlightLayer* highlight_layer =
      controller->highlight_layer_for_testing();
  EXPECT_TRUE(highlight_layer);
  EXPECT_EQ(1u, highlight_layer->rects_for_test().size());
  EXPECT_EQ(SkColorSetRGB(94, 155, 255), highlight_layer->color_for_test());
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, SmoothlyReadsAcrossMultipleLines) {
  // Sentences spanning multiple lines.
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<div style=\"width:100px\">This"
      " is some text with a node in the middle");
  // Should combine nodes in a paragraph into one utterance.
  // Includes some wildcards between words because there may be extra
  // spaces, for example at line wraps. Extra wildcards included to
  // reduce flakyness in case wrapping is not consistent.
  // Spaces are not pronounced, so extra spaces do not impact output.
  sm_.ExpectSpeechPattern("This is some*text*with*a*node*in*the*middle*");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, SmoothlyReadsAcrossFormattedText) {
  // Bold or formatted text
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<p>This is some text <b>with a node"
      "</b> in the middle");

  // Should combine nodes in a paragraph into one utterance.
  // Includes some wildcards between words because there may be extra
  // spaces. Spaces are not pronounced, so extra spaces do not impact output.
  sm_.ExpectSpeechPattern("This is some text*with a node*in the middle*");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest,
                       ReadsStaticTextWithoutInlineTextChildren) {
  // Bold or formatted text
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<canvas>This is some text</canvas>");

  sm_.ExpectSpeechPattern("This is some text*");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, BreaksAtParagraphBounds) {
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<div><p>First paragraph</p>"
      "<p>Second paragraph</p></div>");

  // Should keep each paragraph as its own utterance.
  sm_.ExpectSpeechPattern("First paragraph*");
  sm_.ExpectSpeechPattern("Second paragraph*");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, LanguageBoundsIgnoredByDefault) {
  // Splitting at language bounds is behind a feature flag, test the default
  // behaviour doesn't introduce a regression.
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<div>"
      "<span lang='en-US'>The first paragraph</span>"
      "<span lang='fr-FR'>le deuxieme paragraphe</span></div>");

  sm_.ExpectSpeechPattern("The first paragraph* le deuxieme paragraphe*");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTestWithVoiceSwitching,
                       BreaksAtLanguageBounds) {
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<div>"
      "<span lang='en-US'>The first paragraph</span>"
      "<span lang='fr-FR'>le deuxieme paragraphe</span></div>");

  sm_.ExpectSpeech(test::SpeechMonitor::Expectation("The first paragraph*")
                       .AsPattern()
                       .WithLocale("en-US"));
  sm_.ExpectSpeech(test::SpeechMonitor::Expectation("le deuxieme paragraphe*")
                       .AsPattern()
                       .WithLocale("fr-FR"));
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, DoesNotCrashWithMousewheelEvent) {
  std::string url = "data:text/html;charset=utf-8,<p>This is some text</p>";
  LoadURLAndSelectToSpeak(url);
  gfx::Rect bounds = GetWebContentsBounds(url);

  // Hold down Search and drag over the web contents to select everything.
  generator_->PressKey(ui::VKEY_LWIN, 0 /* flags */);
  generator_->MoveMouseTo(bounds.x(), bounds.y());
  generator_->PressLeftButton();
  // Ensure this does not crash. It should have no effect.
  generator_->MoveMouseWheel(10, 10);
  generator_->MoveMouseTo(bounds.x() + bounds.width(),
                          bounds.y() + bounds.height());
  generator_->MoveMouseWheel(100, 5);
  generator_->ReleaseLeftButton();
  generator_->ReleaseKey(ui::VKEY_LWIN, 0 /* flags */);

  sm_.ExpectSpeechPattern("This is some text*");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, FocusRingMovesWithMouse) {
  std::string url =
      "data:text/html;charset=utf-8,"
      "<p>This is some text</p>";
  LoadURLAndSelectToSpeak(url);

  // Create a callback for the focus ring observer.
  base::RepeatingCallback<void()> callback =
      base::BindRepeating(&SelectToSpeakTest::OnFocusRingChanged, GetWeakPtr());
  AccessibilityManager::Get()->SetFocusRingObserverForTest(callback);

  std::string focus_ring_id = AccessibilityManager::Get()->GetFocusRingId(
      ax::mojom::AssistiveTechnologyType::kSelectToSpeak, "");

  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();
  controller->SetNoFadeForTesting();
  const AccessibilityFocusRingGroup* focus_ring_group =
      controller->GetFocusRingGroupForTesting(focus_ring_id);
  // No focus rings to start.
  EXPECT_EQ(nullptr, focus_ring_group);

  gfx::Rect bounds = GetWebContentsBounds(url);
  PrepareToWaitForFocusRingChanged();
  generator_->PressKey(ui::VKEY_LWIN, 0 /* flags */);
  generator_->MoveMouseTo(bounds.x(), bounds.y());
  generator_->PressLeftButton();

  // Expect a focus ring to have been drawn.
  WaitForFocusRingChanged();
  focus_ring_group = controller->GetFocusRingGroupForTesting(focus_ring_id);
  ASSERT_NE(nullptr, focus_ring_group);
  std::vector<std::unique_ptr<AccessibilityFocusRingLayer>> const& focus_rings =
      focus_ring_group->focus_layers_for_testing();
  EXPECT_EQ(focus_rings.size(), 1u);

  gfx::Rect target_bounds = focus_rings.at(0)->layer()->GetTargetBounds();

  // Make sure it's in a reasonable position.
  EXPECT_LT(abs(target_bounds.x() - bounds.x()), 50);
  EXPECT_LT(abs(target_bounds.y() - bounds.y()), 50);
  EXPECT_LT(target_bounds.width(), 50);
  EXPECT_LT(target_bounds.height(), 50);

  // Move the mouse.
  PrepareToWaitForFocusRingChanged();
  generator_->MoveMouseTo(bounds.x() + 100, bounds.y() + 100);

  // Expect focus ring to have moved with the mouse.
  // The size should have grown to be over 100 (the rect is now size 100,
  // and the focus ring has some buffer). Position should be unchanged.
  WaitForFocusRingChanged();
  target_bounds = focus_rings.at(0)->layer()->GetTargetBounds();
  EXPECT_LT(abs(target_bounds.x() - bounds.x()), 50);
  EXPECT_LT(abs(target_bounds.y() - bounds.y()), 50);
  EXPECT_GT(target_bounds.width(), 100);
  EXPECT_GT(target_bounds.height(), 100);

  // Move the mouse smaller again, it should shrink.
  PrepareToWaitForFocusRingChanged();
  generator_->MoveMouseTo(bounds.x() + 10, bounds.y() + 18);
  WaitForFocusRingChanged();
  target_bounds = focus_rings.at(0)->layer()->GetTargetBounds();
  EXPECT_LT(target_bounds.width(), 50);
  EXPECT_LT(target_bounds.height(), 50);

  // Cancel this by releasing the key before the mouse.
  PrepareToWaitForFocusRingChanged();
  generator_->ReleaseKey(ui::VKEY_LWIN, 0 /* flags */);
  generator_->ReleaseLeftButton();

  // Expect focus ring to have been cleared, this was canceled in STS
  // by releasing the key before the button.
  WaitForFocusRingChanged();
  EXPECT_EQ(focus_rings.size(), 0u);
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest,
                       SelectToSpeakSelectionPansFullscreenMagnifier) {
  FullscreenMagnifierController* fullscreen_magnifier_controller =
      Shell::Get()->fullscreen_magnifier_controller();
  fullscreen_magnifier_controller->SetEnabled(true);

  // Wait for Fullscreen magnifier to initialize.
  MagnifierAnimationWaiter waiter(fullscreen_magnifier_controller);
  waiter.Wait();
  gfx::Point const initial_window_position =
      fullscreen_magnifier_controller->GetWindowPosition();
  std::string url =
      "data:text/html;charset=utf-8,"
      "<p>This is some text</p>";
  LoadURLAndSelectToSpeak(url);
  gfx::Rect bounds = GetWebContentsBounds(url);
  PrepareToWaitForFocusRingChanged();

  // Hold down Search, and move mouse to start of text.
  generator_->PressKey(ui::VKEY_LWIN, 0 /* flags */);
  generator_->MoveMouseTo(bounds.x(), bounds.y());

  // FullscreenMagnifierController moves the magnifier window with animation
  // when the magnifier is set to be enabled. Wait until the animation
  // completes, so that the mouse movement controls the position of magnifier
  // window later.
  waiter.Wait();

  // Press and drag mouse past bounds of magnified screen, to move the viewport.
  generator_->PressLeftButton();

  // Move mouse to bottom right area of screen. Multiply by scale as fullscreen
  // magnifier is enabled, so input needs to be transformed.
  const float scale = fullscreen_magnifier_controller->GetScale();
  generator_->MoveMouseTo(
      (bounds.right() - initial_window_position.x()) * scale,
      (bounds.bottom() - initial_window_position.y()) * scale);

  gfx::Point const final_window_position =
      fullscreen_magnifier_controller->GetWindowPosition();

  // Expect Magnifier window to move with mouse drag during STS selection.
  EXPECT_GT(final_window_position.x(), initial_window_position.x());
  EXPECT_GT(final_window_position.y(), initial_window_position.y());
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTestWithMagnifierFollowing,
                       FullscreenMagnifierFollowsTextBoundsWhenPrefOn) {
  sm_.send_word_events_and_wait_to_finish(true);
  Profile* profile = AccessibilityManager::Get()->profile();
  // Turn off navigation controls as focus on these buttons changes magnifier
  // bounds, and then there's a delay before focus can move to the highlighted
  // area. In real life, focus would then update to the spoken text a short time
  // after navigation controls, but speech monitor speaks everything instantly
  // so we cannot test that.
  profile->GetPrefs()->SetBoolean(
      prefs::kAccessibilitySelectToSpeakNavigationControls, false);

  // Set magnifier following STS Pref on
  profile->GetPrefs()->SetBoolean(prefs::kAccessibilityMagnifierFollowsSts,
                                  true);

  std::string text = "Read me first!";
  std::string second_text = "Read me last!";
  LoadURLAndSelectToSpeak(
      base::StringPrintf("data:text/html;charset=utf-8,<p>Not me!</p>"
                         "<p>Skip me!</p><p>%s</p><p>Nor me!</p><p>%s</p>",
                         text.c_str(), second_text.c_str()));
  SelectNodeWithText(text);

  // Set magnifier scale to something quite big so that the initial bounds of
  // the text are not within the magnifier bounds.
  profile->GetPrefs()->SetDouble(prefs::kAccessibilityScreenMagnifierScale,
                                 8.0);

  // Wait for Fullscreen magnifier to initialize.
  extensions::ExtensionHostTestHelper host_helper(
      profile, extension_misc::kAccessibilityCommonExtensionId);
  profile->GetPrefs()->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled,
                                  true);

  FullscreenMagnifierController* fullscreen_magnifier_controller =
      Shell::Get()->fullscreen_magnifier_controller();
  MagnifierAnimationWaiter waiter(fullscreen_magnifier_controller);
  waiter.Wait();

  host_helper.WaitForHostCompletedFirstLoad();
  FullscreenMagnifierTestHelper::WaitForMagnifierJSReady(profile);

  gfx::Rect initial_viewport =
      fullscreen_magnifier_controller->GetViewportRect();

  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();
  EXPECT_FALSE(controller->highlight_layer_for_testing());
  PrepareToWaitForHighlightAdded();
  // Activate select to speak on the selection (which is outside the magnifier
  // bounds) using search+s.
  generator_->PressKey(ui::VKEY_LWIN, /*flags=*/0);
  generator_->PressKey(ui::VKEY_S, /*flags=*/0);
  generator_->ReleaseKey(ui::VKEY_LWIN, /*flags=*/0);
  generator_->ReleaseKey(ui::VKEY_S, /*flags=*/0);

  // Some highlighting should have occurred. OK to do this after speech as
  // Select to Speak refreshes the UI intermittently.
  WaitForHighlightAdded();

  // Check the highlight exists.
  AccessibilityHighlightLayer* highlight_layer =
      controller->highlight_layer_for_testing();
  ASSERT_TRUE(highlight_layer);
  EXPECT_EQ(1u, highlight_layer->rects_for_test().size());
  gfx::Rect highlight_bounds = highlight_layer->rects_for_test()[0];
  EXPECT_FALSE(initial_viewport.Intersects(highlight_bounds));

  // Magnifier should now move to the highlighted area.
  while (!fullscreen_magnifier_controller->GetViewportRect().Intersects(
      highlight_bounds)) {
    waiter.Wait();
  }
  gfx::Rect final_viewport = fullscreen_magnifier_controller->GetViewportRect();
  EXPECT_FALSE(initial_viewport.Intersects(final_viewport));
  EXPECT_TRUE(final_viewport.Intersects(highlight_bounds));

  // Finish speech and make sure the right thing was actually read.
  sm_.FinishSpeech();
  sm_.ExpectSpeechPattern("*Read me first!*");
  sm_.Replay();

  SelectNodeWithText(second_text);
  PrepareToWaitForHighlightAdded();
  generator_->PressKey(ui::VKEY_LWIN, /*flags=*/0);
  generator_->PressKey(ui::VKEY_S, /*flags=*/0);
  generator_->ReleaseKey(ui::VKEY_LWIN, /*flags=*/0);
  generator_->ReleaseKey(ui::VKEY_S, /*flags=*/0);

  WaitForHighlightAdded();

  // Highlight should have updated.
  // First it will be cleared when the previous utterance is completed.
  // Then make sure it's no longer in the magnifier's viewport.
  while (!highlight_layer->rects_for_test().size() ||
         final_viewport.Intersects(highlight_layer->rects_for_test()[0])) {
    PrepareToWaitForHighlightAdded();
    WaitForHighlightAdded();
  }
  highlight_bounds = highlight_layer->rects_for_test()[0];
  EXPECT_FALSE(final_viewport.Intersects(highlight_bounds));

  // Magnifier should update enough to cover the new highlighted text.
  while (!fullscreen_magnifier_controller->GetViewportRect().Intersects(
      highlight_bounds)) {
    waiter.Wait();
  }
  gfx::Rect second_final_viewport =
      fullscreen_magnifier_controller->GetViewportRect();
  EXPECT_TRUE(second_final_viewport.Intersects(highlight_bounds));

  // Finish speech and make sure the right thing was actually read.
  sm_.FinishSpeech();
  sm_.ExpectSpeechPattern("*Read me last!*");
  sm_.Replay();

  // Reset state.
  sm_.send_word_events_and_wait_to_finish(false);
  profile->GetPrefs()->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled,
                                  false);
}

// TODO(b/259363112): Add a test that Select to Speak follows focus for nodes
// with no inline text boxes.

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, ContinuesReadingDuringResize) {
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<p>First paragraph</p>"
      "<div id='resize' style='width:300px; font-size: 1em'>"
      "<p>Second paragraph is longer than 300 pixels and will wrap when "
      "resized</p></div>");

  sm_.ExpectSpeechPattern("First paragraph*");

  // Resize before second is spoken. If resizing caused errors finding the
  // inlineTextBoxes in the node, speech would be stopped early.
  sm_.Call([this]() {
    ExecuteJavaScriptAsync(
        "document.getElementById('resize').style.width='100px'");
  });
  sm_.ExpectSpeechPattern("*when*resized*");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, WorksWithStickyKeys) {
  AccessibilityManager::Get()->EnableStickyKeys(true);

  std::string url = "data:text/html;charset=utf-8,<p>This is some text</p>";
  LoadURLAndSelectToSpeak(url);

  // Tap Search and click a few pixels into the window bounds.
  generator_->PressKey(ui::VKEY_LWIN, 0 /* flags */);
  generator_->ReleaseKey(ui::VKEY_LWIN, 0 /* flags */);

  // Sticky keys should remember the 'search' key was clicked, so STS is
  // actually in a capturing mode now.
  gfx::Rect bounds = GetWebContentsBounds(url);
  generator_->MoveMouseTo(bounds.x(), bounds.y());
  generator_->PressLeftButton();
  generator_->MoveMouseTo(bounds.x() + bounds.width(),
                          bounds.y() + bounds.height());
  generator_->ReleaseLeftButton();

  sm_.ExpectSpeechPattern("This is some text*");

  // Reset state.
  sm_.Call([]() { AccessibilityManager::Get()->EnableStickyKeys(false); });

  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest,
                       SelectToSpeakDoesNotDismissTrayBubble) {
  sts_test_utils::TurnOnSelectToSpeakForTest(
      AccessibilityManager::Get()->profile());

  // Open tray bubble menu.
  tray_test_api_->ShowBubble();

  // Search key + click the settings button.
  generator_->PressKey(ui::VKEY_LWIN, 0 /* flags */);
  tray_test_api_->ClickBubbleView(ViewID::VIEW_ID_QS_SETTINGS_BUTTON);
  generator_->ReleaseKey(ui::VKEY_LWIN, 0 /* flags */);

  // Should read out text.
  sm_.ExpectSpeechPattern("*Settings*");
  sm_.Replay();

  // Tray bubble menu should remain open.
  ASSERT_TRUE(tray_test_api_->IsTrayBubbleOpen());
}

// TODO(anastasi): Test that metrics record duration here.
IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, ReadsSelectedTextWithSearchS) {
  std::string text = "This is some selected text";
  LoadURLAndSelectToSpeak(base::StringPrintf(
      "data:text/html;charset=utf-8,<p>Not me!</p><p>%s</p><p>Nor me!</p>",
      text.c_str()));
  SelectNodeWithText(text);

  generator_->PressKey(ui::VKEY_LWIN, /*flags=*/0);
  generator_->PressKey(ui::VKEY_S, /*flags=*/0);
  generator_->ReleaseKey(ui::VKEY_LWIN, /*flags=*/0);
  generator_->ReleaseKey(ui::VKEY_S, /*flags=*/0);

  sm_.ExpectSpeechPattern(text);
  sm_.Call([this]() {
    generator_->PressKey(ui::VKEY_CONTROL, /*flags=*/0);
    generator_->ReleaseKey(ui::VKEY_CONTROL, /*flags=*/0);
  });
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest,
                       ReadsSelectedTextFromContextMenuClick) {
  std::string text = "This is some selected text";
  LoadURLAndSelectToSpeak(base::StringPrintf(
      "data:text/html;charset=utf-8,<p>Not me!</p><p>%s</p><p>Nor me!</p>",
      text.c_str()));

  SelectNodeWithText(text);

  gfx::Rect text_bounds =
      automation_test_utils_->GetNodeBoundsInRoot(text, "staticText");
  text_bounds.Inset(2.0f);
  generator_->MoveMouseTo(text_bounds.right_center());

  const std::string name = "Listen to selected text";

  // Right-click the selected region.
  generator_->PressRightButton();
  generator_->ReleaseRightButton();

  // Wait for the copy context menu item to be shown,
  // this means the menu is displayed.
  automation_test_utils_->GetNodeBoundsInRoot("Copy Ctrl+C", "menuItem");
  ASSERT_TRUE(automation_test_utils_->NodeExistsNoWait(name, "menuItem"));

  // Click the Select to Speak menu item.
  gfx::Rect menu_item_bounds =
      automation_test_utils_->GetNodeBoundsInRoot(name, "menuItem");
  generator_->MoveMouseTo(menu_item_bounds.CenterPoint());
  generator_->PressLeftButton();
  generator_->ReleaseLeftButton();

  sm_.ExpectSpeechPattern(text);
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest,
                       ReadsSelectedTextWithContextMenuNotification) {
  std::string text = "Pick me! Read me!";
  LoadURLAndSelectToSpeak(base::StringPrintf(
      "data:text/html;charset=utf-8,<p>Not me!</p><p>%s</p><p>Nor me!</p>",
      text.c_str()));
  SelectNodeWithText(text);

  AccessibilityManager::Get()->OnSelectToSpeakContextMenuClick();

  sm_.ExpectSpeechPattern(text);
  sm_.Replay();
}

}  // namespace ash
