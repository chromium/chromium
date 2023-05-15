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
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/magnifier_animation_waiter.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/process_manager.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkColor.h"
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

 protected:
  SelectToSpeakTest() {}
  ~SelectToSpeakTest() override {}

  // Note that we do not enable Select to Speak in the SetUp method because
  // tests are less flaky if we load the page URL before loading up the
  // Select to Speak extension.
  void SetUpOnMainThread() override {
    ASSERT_FALSE(AccessibilityManager::Get()->IsSelectToSpeakEnabled());
    console_observer_ = std::make_unique<ExtensionConsoleErrorObserver>(
        browser()->profile(), extension_misc::kSelectToSpeakExtensionId);

    tray_test_api_ = SystemTrayTestApi::Create();
    aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
    generator_ = std::make_unique<ui::test::EventGenerator>(root_window);

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

    // Pretend that enhanced network voices dialog has been accepted so that the
    // dialog does not block.
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kAccessibilitySelectToSpeakEnhancedVoicesDialogShown, true);
  }

  test::SpeechMonitor sm_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  std::unique_ptr<SystemTrayTestApi> tray_test_api_;
  std::unique_ptr<ExtensionConsoleErrorObserver> console_observer_;

  // Turns on Select to Speak and waits for the extension to signal it is ready.
  // Virtual so that subclasses of this test can do other set-up on Select to
  // Speak.
  virtual void TurnOnSelectToSpeak() {
    extensions::ExtensionHostTestHelper host_helper(
        browser()->profile(), extension_misc::kSelectToSpeakExtensionId);
    AccessibilityManager::Get()->SetSelectToSpeakEnabled(true);
    host_helper.WaitForHostCompletedFirstLoad();
    WaitForSTSReady();
  }

  gfx::Rect GetWebContentsBounds() const {
    // TODO(katie): Find a way to get the exact bounds programmatically.
    gfx::Rect bounds = browser()->window()->GetBounds();
    bounds.Inset(gfx::Insets::TLBR(8, 8, 8, 75));
    return bounds;
  }

  void LoadURLAndSelectToSpeak(std::string url) {
    content::AccessibilityNotificationWaiter waiter(
        GetWebContents(), ui::kAXModeComplete, ax::mojom::Event::kLoadComplete);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));
    std::ignore = waiter.WaitForNotification();

    if (!AccessibilityManager::Get()->IsSelectToSpeakEnabled())
      TurnOnSelectToSpeak();
  }

  virtual void ActivateSelectToSpeakInWindowBounds(std::string url) {
    // Load the URL before Select to Speak to avoid flakes.
    LoadURLAndSelectToSpeak(url);

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

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void ExecuteJavaScriptAsync(const std::string& script) {
    content::ExecuteScriptAsync(GetWebContents(), script);
  }

 private:
  void WaitForSTSReady() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string script = base::StringPrintf(R"JS(
      (async function() {
        let module = await import('./select_to_speak_main.js');
        module.selectToSpeak.setOnLoadDesktopCallbackForTest(() => {
            chrome.test.sendScriptResult('ready');
          });
        // Set enhanced network voices dialog as shown, because the pref
        // change takes some time to propagate.
        module.selectToSpeak.prefsManager_.enhancedVoicesDialogShown_ = true;
      })();
    )JS");
    base::Value result =
        extensions::browsertest_util::ExecuteScriptInBackgroundPage(
            browser()->profile(), extension_misc::kSelectToSpeakExtensionId,
            script);
    ASSERT_EQ("ready", result);
  }

  std::unique_ptr<base::RunLoop> loop_runner_;
  std::unique_ptr<base::RunLoop> highlights_runner_;
  std::unique_ptr<base::RunLoop> tray_loop_runner_;
  base::WeakPtrFactory<SelectToSpeakTest> weak_ptr_factory_{this};
};

class SelectToSpeakTestWithVoiceSwitching : public SelectToSpeakTest {
 protected:
  void SetUpOnMainThread() override {
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kAccessibilitySelectToSpeakVoiceSwitching, true);
    SelectToSpeakTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, SpeakStatusTray) {
  TurnOnSelectToSpeak();
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
  gfx::Rect bounds = GetWebContentsBounds();
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

  ShellTestApi shell_test_api;
  display::test::DisplayManagerTestApi(shell_test_api.display_manager())
      .UpdateDisplay("1+0-800x800,801+1-800x800");
  ASSERT_EQ(2u, shell_test_api.display_manager()->GetNumDisplays());
  display::test::DisplayManagerTestApi display_manager_test_api(
      shell_test_api.display_manager());

  display::Screen* screen = display::Screen::GetScreen();
  int64_t display2 = display_manager_test_api.GetSecondaryDisplay().id();
  screen->SetDisplayForNewWindows(display2);
  Browser* browser_on_secondary_display = CreateBrowser(browser()->profile());

  content::AccessibilityNotificationWaiter waiter(
      browser_on_secondary_display->tab_strip_model()->GetActiveWebContents(),
      ui::kAXModeComplete, ax::mojom::Event::kLayoutComplete);
  // Create a window on the non-primary display.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser_on_secondary_display,
      GURL("data:text/html;charset=utf-8,<p>This is some text</p>")));
  std::ignore = waiter.WaitForNotification();

  TurnOnSelectToSpeak();
  base::RepeatingCallback<void()> callback = base::BindRepeating(
      &SelectToSpeakTest::SetSelectToSpeakState, GetWeakPtr());
  AccessibilityManager::Get()->SetSelectToSpeakStateObserverForTest(callback);

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
  TurnOnSelectToSpeak();
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
      "<span lang='fr-FR'>le deuxième paragraphe</span></div>");

  sm_.ExpectSpeechPattern("The first paragraph* le deuxième paragraphe*");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTestWithVoiceSwitching,
                       BreaksAtLanguageBounds) {
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<div>"
      "<span lang='en-US'>The first paragraph</span>"
      "<span lang='fr-FR'>le deuxième paragraphe</span></div>");

  sm_.ExpectSpeechPatternWithLocale("The first paragraph*", "en-US");
  sm_.ExpectSpeechPatternWithLocale("le deuxième paragraphe*", "fr-FR");
  sm_.Replay();
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, DoesNotCrashWithMousewheelEvent) {
  LoadURLAndSelectToSpeak(
      "data:text/html;charset=utf-8,<p>This is some text</p>");
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

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, FocusRingMovesWithMouse) {
  LoadURLAndSelectToSpeak(
      "data:text/html;charset=utf-8,"
      "<p>This is some text</p>");

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

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest,
                       SelectToSpeakPansFullscreenMagnifier) {
  FullscreenMagnifierController* fullscreen_magnifier_controller =
      Shell::Get()->fullscreen_magnifier_controller();
  fullscreen_magnifier_controller->SetEnabled(true);

  // Wait for Fullscreen magnifier to initialize.
  MagnifierAnimationWaiter waiter(fullscreen_magnifier_controller);
  waiter.Wait();
  gfx::Point const initial_window_position =
      fullscreen_magnifier_controller->GetWindowPosition();
  LoadURLAndSelectToSpeak(
      "data:text/html;charset=utf-8,"
      "<p>This is some text</p>");
  gfx::Rect bounds = GetWebContentsBounds();
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

  // Expect Magnifier window to move with mouse drag.
  EXPECT_GT(final_window_position.x(), initial_window_position.x());
  EXPECT_GT(final_window_position.y(), initial_window_position.y());
}

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

  LoadURLAndSelectToSpeak(
      "data:text/html;charset=utf-8,<p>This is some text</p>");

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

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest,
                       SelectToSpeakDoesNotDismissTrayBubble) {
  TurnOnSelectToSpeak();

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

}  // namespace ash
