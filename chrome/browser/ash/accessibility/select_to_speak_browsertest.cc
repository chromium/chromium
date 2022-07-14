// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/accessibility/ui/accessibility_focus_ring_controller_impl.h"
#include "ash/accessibility/ui/accessibility_focus_ring_layer.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/process_manager.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/compositor/layer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "url/url_constants.h"

namespace ash {

class SelectToSpeakTest : public InProcessBrowserTest {
 public:
  SelectToSpeakTest(const SelectToSpeakTest&) = delete;
  SelectToSpeakTest& operator=(const SelectToSpeakTest&) = delete;

  void OnFocusRingChanged() {
    if (loop_runner_) {
      loop_runner_->Quit();
    }
  }

  void SetSelectToSpeakState() {
    if (tray_loop_runner_) {
      tray_loop_runner_->Quit();
    }
  }

 protected:
  SelectToSpeakTest() {}
  ~SelectToSpeakTest() override {}

  void SetUpOnMainThread() override {
    ASSERT_FALSE(AccessibilityManager::Get()->IsSelectToSpeakEnabled());
    console_observer_ = std::make_unique<ExtensionConsoleErrorObserver>(
        browser()->profile(), extension_misc::kSelectToSpeakExtensionId);

    tray_test_api_ = SystemTrayTestApi::Create();

    extensions::ExtensionHostTestHelper host_helper(
        browser()->profile(), extension_misc::kSelectToSpeakExtensionId);
    AccessibilityManager::Get()->SetSelectToSpeakEnabled(true);
    host_helper.WaitForHostCompletedFirstLoad();

    aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
    generator_ = std::make_unique<ui::test::EventGenerator>(root_window);

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

    // Select to speak loads part of itself (eventually all of itself) via a
    // dynamic import. This means that the background page signals a load event
    // prior to the import being fully finished. Wait for it here.
    std::string script =
        R"JS(
          (async function() {
            await import("/select_to_speak/select_to_speak_main.js");
            window.domAutomationController.send('ok');
          })();
        )JS";

    std::string result =
        extensions::browsertest_util::ExecuteScriptInBackgroundPage(
            browser()->profile(), extension_misc::kSelectToSpeakExtensionId,
            script,
            extensions::browsertest_util::ScriptUserActivation::kDontActivate);
    ASSERT_EQ(result, "ok");
  }

  void SetUpInProcessBrowserTestFixture() override {
    // TODO (leileilei@google.com): Provide a way to disable the pop up dialog.
    // Disable kEnhancedNetworkVoices To avoid its pop up dialog.
    scoped_feature_list_.InitAndDisableFeature(
        ::features::kEnhancedNetworkVoices);
  }

  test::SpeechMonitor sm_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  std::unique_ptr<SystemTrayTestApi> tray_test_api_;
  std::unique_ptr<ExtensionConsoleErrorObserver> console_observer_;

  gfx::Rect GetWebContentsBounds() const {
    // TODO(katie): Find a way to get the exact bounds programmatically.
    gfx::Rect bounds = browser()->window()->GetBounds();
    bounds.Inset(gfx::Insets::TLBR(8, 8, 8, 75));
    return bounds;
  }

  void ActivateSelectToSpeakInWindowBounds(std::string url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));
    gfx::Rect bounds = GetWebContentsBounds();

    // Hold down Search and drag over the web contents to select everything.
    generator_->PressKey(ui::VKEY_LWIN, 0 /* flags */);
    generator_->MoveMouseTo(bounds.x(), bounds.y());
    generator_->PressLeftButton();
    generator_->MoveMouseTo(bounds.x() + bounds.width(),
                            bounds.y() + bounds.height());
    generator_->ReleaseLeftButton();
    generator_->ReleaseKey(ui::VKEY_LWIN, 0 /* flags */);
  }

  void PrepareToWaitForSelectToSpeakStatusChanged() {
    tray_loop_runner_ = new content::MessageLoopRunner();
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
    loop_runner_ = new content::MessageLoopRunner();
  }

  // Blocks until the focus ring is changed.
  void WaitForFocusRingChanged() {
    loop_runner_->Run();
    loop_runner_ = nullptr;
  }

  base::WeakPtr<SelectToSpeakTest> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void RunJavaScriptInSelectToSpeakBackgroundPage(const std::string& script) {
    extensions::ExtensionHost* host =
        extensions::ProcessManager::Get(browser()->profile())
            ->GetBackgroundHostForExtension(
                extension_misc::kSelectToSpeakExtensionId);
    CHECK(content::ExecuteScript(host->host_contents(), script));
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void ExecuteJavaScriptAsync(const std::string& script) {
    content::ExecuteScriptAsync(GetWebContents(), script);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<content::MessageLoopRunner> loop_runner_;
  scoped_refptr<content::MessageLoopRunner> tray_loop_runner_;
  base::WeakPtrFactory<SelectToSpeakTest> weak_ptr_factory_{this};
};

/* Test fixture enabling experimental accessibility language detection switch */
class SelectToSpeakTestWithLanguageDetection : public SelectToSpeakTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SelectToSpeakTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        ::switches::kEnableExperimentalAccessibilityLanguageDetection);
  }
};

// The status tray is not active on official builds.
// Disable the test on Chromium due to flaky: crbug.com/1165749
IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, DISABLED_SpeakStatusTray) {
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

// Flaky on ChromeOS MSAN bots: https://crbug.com/1227368
#if defined(MEMORY_SANITIZER)
#define MAYBE_ActivatesWithTapOnSelectToSpeakTray DISABLED_ActivatesWithTapOnSelectToSpeakTray
#else
#define MAYBE_ActivatesWithTapOnSelectToSpeakTray ActivatesWithTapOnSelectToSpeakTray
#endif
IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, MAYBE_ActivatesWithTapOnSelectToSpeakTray) {
  base::RepeatingCallback<void()> callback = base::BindRepeating(
      &SelectToSpeakTest::SetSelectToSpeakState, GetWeakPtr());
  AccessibilityManager::Get()->SetSelectToSpeakStateObserverForTest(callback);
  // Click in the tray bounds to start 'selection' mode.
  TapSelectToSpeakTray();

  // We should be in "selection" mode, so clicking with the mouse should
  // start speech.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html;charset=utf-8,<p>This is some text</p>")));
  gfx::Rect bounds = GetWebContentsBounds();
  generator_->MoveMouseTo(bounds.x(), bounds.y());
  generator_->PressLeftButton();
  generator_->MoveMouseTo(bounds.x() + bounds.width(),
                          bounds.y() + bounds.height());
  generator_->ReleaseLeftButton();

  sm_.ExpectSpeechPattern("This is some text*");
  sm_.Replay();
}

// Flaky on ChromeOS MSAN bots: https://crbug.com/1227368
#if defined(MEMORY_SANITIZER)
#define MAYBE_WorksWithTouchSelection DISABLED_WorksWithTouchSelection
#else
#define MAYBE_WorksWithTouchSelection WorksWithTouchSelection
#endif
IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, MAYBE_WorksWithTouchSelection) {
  base::RepeatingCallback<void()> callback = base::BindRepeating(
      &SelectToSpeakTest::SetSelectToSpeakState, GetWeakPtr());
  AccessibilityManager::Get()->SetSelectToSpeakStateObserverForTest(callback);
  // Click in the tray bounds to start 'selection' mode.
  TapSelectToSpeakTray();

  // We should be in "selection" mode, so tapping and dragging should
  // start speech.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html;charset=utf-8,<p>This is some text</p>")));
  gfx::Rect bounds = GetWebContentsBounds();
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

  ash::ShellTestApi shell_test_api;
  display::test::DisplayManagerTestApi(shell_test_api.display_manager())
      .UpdateDisplay("1+0-800x800,801+1-800x800");
  ASSERT_EQ(2u, shell_test_api.display_manager()->GetNumDisplays());
  display::test::DisplayManagerTestApi display_manager_test_api(
      shell_test_api.display_manager());

  display::Screen* screen = display::Screen::GetScreen();
  int64_t display2 = display_manager_test_api.GetSecondaryDisplay().id();
  screen->SetDisplayForNewWindows(display2);
  Browser* browser_on_secondary_display = CreateBrowser(browser()->profile());

  base::RepeatingCallback<void()> callback = base::BindRepeating(
      &SelectToSpeakTest::SetSelectToSpeakState, GetWeakPtr());
  AccessibilityManager::Get()->SetSelectToSpeakStateObserverForTest(callback);

  // Create a window on the non-primary display.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser_on_secondary_display,
      GURL("data:text/html;charset=utf-8,<p>This is some text</p>")));
  // Click in the tray bounds to start 'selection' mode.
  TapSelectToSpeakTray();
  // We should be in "selection" mode, so tapping and dragging should
  // start speech.
  gfx::Rect bounds = GetWebContentsBounds();
  generator_->PressTouch(gfx::Point(bounds.x() + 800, bounds.y()));
  generator_->PressMoveAndReleaseTouchTo(bounds.x() + 800 + bounds.width(),
                                         bounds.y() + bounds.height());
  generator_->ReleaseLeftButton();

  sm_.ExpectSpeechPattern("This is some text*");
  sm_.Replay();

  CloseBrowserSynchronously(browser_on_secondary_display);
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, SelectToSpeakTrayNotSpoken) {
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

// Flaky on ChromeOS MSAN bots: https://crbug.com/1227368
#if defined(MEMORY_SANITIZER)
#define MAYBE_SmoothlyReadsAcrossInlineUrl DISABLED_SmoothlyReadsAcrossInlineUrl
#else
#define MAYBE_SmoothlyReadsAcrossInlineUrl SmoothlyReadsAcrossInlineUrl
#endif
IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, MAYBE_SmoothlyReadsAcrossInlineUrl) {
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

// Flaky on ChromeOS MSAN bots: https://crbug.com/1227368
#if defined(MEMORY_SANITIZER)
#define MAYBE_SmoothlyReadsAcrossMultipleLines DISABLED_SmoothlyReadsAcrossMultipleLines
#else
#define MAYBE_SmoothlyReadsAcrossMultipleLines SmoothlyReadsAcrossMultipleLines
#endif
IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, MAYBE_SmoothlyReadsAcrossMultipleLines) {
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

// TODO(crbug.com/1225388): Flaky on ChromeOS MSAN bots
#if defined(MEMORY_SANITIZER)
#define MAYBE_SmoothlyReadsAcrossFormattedText \
  DISABLED_SmoothlyReadsAcrossFormattedText
#else
#define MAYBE_SmoothlyReadsAcrossFormattedText SmoothlyReadsAcrossFormattedText
#endif
IN_PROC_BROWSER_TEST_F(SelectToSpeakTest,
                       MAYBE_SmoothlyReadsAcrossFormattedText) {
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

// Flaky on ChromeOS MSAN bots: https://crbug.com/1227368
#if defined(MEMORY_SANITIZER)
#define MAYBE_ReadsStaticTextWithoutInlineTextChildren \
  DISABLED_ReadsStaticTextWithoutInlineTextChildren
#else
#define MAYBE_ReadsStaticTextWithoutInlineTextChildren \
  ReadsStaticTextWithoutInlineTextChildren
#endif
IN_PROC_BROWSER_TEST_F(SelectToSpeakTest,
                       MAYBE_ReadsStaticTextWithoutInlineTextChildren) {
  // Bold or formatted text
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<canvas>This is some text</canvas>");

  sm_.ExpectSpeechPattern("This is some text*");
  sm_.Replay();
}

// Flaky on ChromeOS MSAN bots: https://crbug.com/1227368
#if defined(MEMORY_SANITIZER)
#define MAYBE_BreaksAtParagraphBounds DISABLED_BreaksAtParagraphBounds
#else
#define MAYBE_BreaksAtParagraphBounds BreaksAtParagraphBounds
#endif
IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, MAYBE_BreaksAtParagraphBounds) {
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<div><p>First paragraph</p>"
      "<p>Second paragraph</p></div>");

  // Should keep each paragraph as its own utterance.
  sm_.ExpectSpeechPattern("First paragraph*");
  sm_.ExpectSpeechPattern("Second paragraph*");
  sm_.Replay();
}

#if defined(MEMORY_SANITIZER)
// TODO(crbug.com/1184714): Flaky timeout on MSAN.
#define MAYBE_LanguageBoundsIgnoredByDefault \
  DISABLED_LanguageBoundsIgnoredByDefault
#else
#define MAYBE_LanguageBoundsIgnoredByDefault \
  DISABLED_LanguageBoundsIgnoredByDefault
#endif
IN_PROC_BROWSER_TEST_F(SelectToSpeakTest,
                       MAYBE_LanguageBoundsIgnoredByDefault) {
  // Splitting at language bounds is behind a feature flag, test the default
  // behaviour doesn't introduce a regression.
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<div>"
      "<span lang='en-US'>The first paragraph</span>"
      "<span lang='fr-FR'>le deuxième paragraphe</span></div>");

  sm_.ExpectSpeechPattern("The first paragraph* le deuxième paragraphe*");
  sm_.Replay();
}

// TODO(crbug.com/1107958): Re-enable this test after fixing flakes.
IN_PROC_BROWSER_TEST_F(SelectToSpeakTestWithLanguageDetection,
                       DISABLED_BreaksAtLanguageBounds) {
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<div>"
      "<span lang='en-US'>The first paragraph</span>"
      "<span lang='fr-FR'>le deuxième paragraphe</span></div>");

  sm_.ExpectSpeechPatternWithLocale("The first paragraph*", "en-US");
  sm_.ExpectSpeechPatternWithLocale("le deuxième paragraphe*", "fr-FR");
  sm_.Replay();
}

// Flaky on ChromeOS MSAN bots: https://crbug.com/1227368
#if defined(MEMORY_SANITIZER)
#define MAYBE_DoesNotCrashWithMousewheelEvent DISABLED_DoesNotCrashWithMousewheelEvent
#else
#define MAYBE_DoesNotCrashWithMousewheelEvent DoesNotCrashWithMousewheelEvent
#endif
IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, MAYBE_DoesNotCrashWithMousewheelEvent) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html;charset=utf-8,<p>This is some text</p>")));
  gfx::Rect bounds = GetWebContentsBounds();

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

// Flaky on ChromeOS MSAN bots: https://crbug.com/1227368
#if defined(MEMORY_SANITIZER)
#define MAYBE_FocusRingMovesWithMouse DISABLED_FocusRingMovesWithMouse
#else
#define MAYBE_FocusRingMovesWithMouse FocusRingMovesWithMouse
#endif
IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, MAYBE_FocusRingMovesWithMouse) {
  // Create a callback for the focus ring observer.
  base::RepeatingCallback<void()> callback =
      base::BindRepeating(&SelectToSpeakTest::OnFocusRingChanged, GetWeakPtr());
  AccessibilityManager::Get()->SetFocusRingObserverForTest(callback);

  std::string focus_ring_id = AccessibilityManager::Get()->GetFocusRingId(
      extension_misc::kSelectToSpeakExtensionId, "");

  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();
  controller->SetNoFadeForTesting();
  const AccessibilityFocusRingGroup* focus_ring_group =
      controller->GetFocusRingGroupForTesting(focus_ring_id);
  // No focus rings to start.
  EXPECT_EQ(nullptr, focus_ring_group);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("data:text/html;charset=utf-8,"
                                                "<p>This is some text</p>")));
  gfx::Rect bounds = GetWebContentsBounds();
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

// crbug.com/1114854 - Times out on MSAN bots.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ContinuesReadingDuringResize DISABLED_ContinuesReadingDuringResize
#else
#define MAYBE_ContinuesReadingDuringResize ContinuesReadingDuringResize
#endif
IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, MAYBE_ContinuesReadingDuringResize) {
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

// Flaky on ChromeOS MSAN bots: https://crbug.com/1227368
#if defined(MEMORY_SANITIZER)
#define MAYBE_WorksWithStickyKeys DISABLED_WorksWithStickyKeys
#else
#define MAYBE_WorksWithStickyKeys WorksWithStickyKeys
#endif
IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, MAYBE_WorksWithStickyKeys) {
  AccessibilityManager::Get()->EnableStickyKeys(true);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html;charset=utf-8,<p>This is some text</p>")));

  // Tap Search and click a few pixels into the window bounds.
  generator_->PressKey(ui::VKEY_LWIN, 0 /* flags */);
  generator_->ReleaseKey(ui::VKEY_LWIN, 0 /* flags */);

  // Sticky keys should remember the 'search' key was clicked, so STS is
  // actually in a capturing mode now.
  gfx::Rect bounds = GetWebContentsBounds();
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

// TODO(crbug.com/1227368): Flaky on ChromeOS MSAN bots.
// TODO(crbug.com/1344562): Flaky on other CrOS bots too.
IN_PROC_BROWSER_TEST_F(SelectToSpeakTest,
                       DISABLED_SelectToSpeakDoesNotDismissTrayBubble) {
  // Open tray bubble menu.
  tray_test_api_->ShowBubble();

  // Search key + click the avatar button.
  generator_->PressKey(ui::VKEY_LWIN, 0 /* flags */);
  tray_test_api_->ClickBubbleView(ViewID::VIEW_ID_USER_AVATAR_BUTTON);
  generator_->ReleaseKey(ui::VKEY_LWIN, 0 /* flags */);

  // Should read out text.
  sm_.ExpectSpeechPattern("*stub-user@example.com*");
  sm_.Replay();

  // Tray bubble menu should remain open.
  ASSERT_TRUE(tray_test_api_->IsTrayBubbleOpen());
}

}  // namespace ash
