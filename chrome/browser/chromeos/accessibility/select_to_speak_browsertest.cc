// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/accessibility/accessibility_focus_ring_controller_impl.h"
#include "ash/accessibility/accessibility_focus_ring_layer.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/mojom/constants.mojom.h"
#include "ash/public/mojom/status_area_widget_test_api.test-mojom-test-utils.h"
#include "ash/public/mojom/status_area_widget_test_api.test-mojom.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/pattern.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/speech_monitor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/system_connector.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/process_manager.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/events/test/event_generator.h"
#include "url/url_constants.h"

namespace chromeos {

class SelectToSpeakTest : public InProcessBrowserTest {
 public:
  void OnFocusRingChanged() {
    if (loop_runner_) {
      loop_runner_->Quit();
    }
  }

  void OnSelectToSpeakStateChanged() {
    if (tray_loop_runner_) {
      tray_loop_runner_->Quit();
    }
  }

 protected:
  SelectToSpeakTest() {}
  ~SelectToSpeakTest() override {}

  void SetUpOnMainThread() override {
    ASSERT_FALSE(AccessibilityManager::Get()->IsSelectToSpeakEnabled());

    // Connect to the ash test interface for the StatusAreaWidget.
    content::GetSystemConnector()->BindInterface(ash::mojom::kServiceName,
                                                 &status_area_widget_test_api_);

    content::WindowedNotificationObserver extension_load_waiter(
        extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD,
        content::NotificationService::AllSources());
    AccessibilityManager::Get()->SetSelectToSpeakEnabled(true);
    extension_load_waiter.Wait();

    aura::Window* root_window = ash::Shell::Get()->GetPrimaryRootWindow();
    generator_.reset(new ui::test::EventGenerator(root_window));

    ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  }

  SpeechMonitor speech_monitor_;
  std::unique_ptr<ui::test::EventGenerator> generator_;

  gfx::Rect GetWebContentsBounds() const {
    // TODO(katie): Find a way to get the exact bounds programmatically.
    gfx::Rect bounds = browser()->window()->GetBounds();
    bounds.Inset(8, 8, 75, 8);
    return bounds;
  }

  void ActivateSelectToSpeakInWindowBounds(std::string url) {
    ui_test_utils::NavigateToURL(browser(), GURL(url));
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
    ash::mojom::StatusAreaWidgetTestApiAsyncWaiter status_area(
        status_area_widget_test_api_.get());
    PrepareToWaitForSelectToSpeakStatusChanged();
    status_area.TapSelectToSpeakTray();
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

  void ExecuteJavaScriptInForeground(const std::string& script) {
    CHECK(content::ExecuteScript(GetWebContents(), script));
  }

 private:
  ash::mojom::StatusAreaWidgetTestApiPtr status_area_widget_test_api_;
  scoped_refptr<content::MessageLoopRunner> loop_runner_;
  scoped_refptr<content::MessageLoopRunner> tray_loop_runner_;
  base::WeakPtrFactory<SelectToSpeakTest> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(SelectToSpeakTest);
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

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, SpeakStatusTray) {
  gfx::Rect tray_bounds = ash::Shell::Get()
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

  EXPECT_TRUE(
      base::MatchPattern(speech_monitor_.GetNextUtterance(), "Status tray*"));
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, ActivatesWithTapOnSelectToSpeakTray) {
  base::RepeatingCallback<void()> callback = base::BindRepeating(
      &SelectToSpeakTest::OnSelectToSpeakStateChanged, GetWeakPtr());
  chromeos::AccessibilityManager::Get()->SetSelectToSpeakStateObserverForTest(
      callback);
  // Click in the tray bounds to start 'selection' mode.
  TapSelectToSpeakTray();

  // We should be in "selection" mode, so clicking with the mouse should
  // start speech.
  ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html;charset=utf-8,<p>This is some text</p>"));
  gfx::Rect bounds = GetWebContentsBounds();
  generator_->MoveMouseTo(bounds.x(), bounds.y());
  generator_->PressLeftButton();
  generator_->MoveMouseTo(bounds.x() + bounds.width(),
                          bounds.y() + bounds.height());
  generator_->ReleaseLeftButton();

  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(),
                                 "This is some text*"));
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, SelectToSpeakTrayNotSpoken) {
  base::RepeatingCallback<void()> callback = base::BindRepeating(
      &SelectToSpeakTest::OnSelectToSpeakStateChanged, GetWeakPtr());
  chromeos::AccessibilityManager::Get()->SetSelectToSpeakStateObserverForTest(
      callback);

  // Tap it once to enter selection mode.
  TapSelectToSpeakTray();

  // Tap again to turn off selection mode.
  TapSelectToSpeakTray();

  // The next should be the first thing spoken -- the tray was not spoken.
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<p>This is some text</p>");
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(),
                                 "This is some text*"));
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, SmoothlyReadsAcrossInlineUrl) {
  // Make sure an inline URL is read smoothly.
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<p>This is some text <a href=\"\">with a"
      " node</a> in the middle");
  // Should combine nodes in a paragraph into one utterance.
  // Includes some wildcards between words because there may be extra
  // spaces. Spaces are not pronounced, so extra spaces do not impact output.
  EXPECT_TRUE(
      base::MatchPattern(speech_monitor_.GetNextUtterance(),
                         "This is some text*with a node*in the middle*"));
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
  EXPECT_TRUE(
      base::MatchPattern(speech_monitor_.GetNextUtterance(),
                         "This is some*text*with*a*node*in*the*middle*"));
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, SmoothlyReadsAcrossFormattedText) {
  // Bold or formatted text
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<p>This is some text <b>with a node"
      "</b> in the middle");

  // Should combine nodes in a paragraph into one utterance.
  // Includes some wildcards between words because there may be extra
  // spaces. Spaces are not pronounced, so extra spaces do not impact output.
  EXPECT_TRUE(
      base::MatchPattern(speech_monitor_.GetNextUtterance(),
                         "This is some text*with a node*in the middle*"));
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest,
                       ReadsStaticTextWithoutInlineTextChildren) {
  // Bold or formatted text
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<canvas>This is some text</canvas>");
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(),
                                 "This is some text*"));
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, BreaksAtParagraphBounds) {
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<div><p>First paragraph</p>"
      "<p>Second paragraph</p></div>");

  // Should keep each paragraph as its own utterance.
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(),
                                 "First paragraph*"));
  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(),
                                 "Second paragraph*"));
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, LanguageBoundsIgnoredByDefault) {
  // Splitting at language bounds is behind a feature flag, test the default
  // behaviour doesn't introduce a regression.
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<div>"
      "<span lang='en-US'>The first paragraph</span>"
      "<span lang='fr-FR'>la deuxième paragraphe</span></div>");

  EXPECT_TRUE(
      base::MatchPattern(speech_monitor_.GetNextUtterance(),
                         "The first paragraph* la deuxième paragraphe*"));
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTestWithLanguageDetection,
                       BreaksAtLanguageBounds) {
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<div>"
      "<span lang='en-US'>The first paragraph</span>"
      "<span lang='fr-FR'>la deuxième paragraphe</span></div>");

  SpeechMonitorUtterance result1 =
      speech_monitor_.GetNextUtteranceWithLanguage();
  EXPECT_TRUE(base::MatchPattern(result1.text, "The first paragraph*"));
  EXPECT_EQ("en-US", result1.lang);

  SpeechMonitorUtterance result2 =
      speech_monitor_.GetNextUtteranceWithLanguage();
  EXPECT_TRUE(base::MatchPattern(result2.text, "la deuxième paragraphe*"));
  EXPECT_EQ("fr-FR", result2.lang);
}

// Flaky test. https://crbug.com/950049
IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, DISABLED_FocusRingMovesWithMouse) {
  // Create a callback for the focus ring observer.
  base::RepeatingCallback<void()> callback =
      base::BindRepeating(&SelectToSpeakTest::OnFocusRingChanged, GetWeakPtr());
  chromeos::AccessibilityManager::Get()->SetFocusRingObserverForTest(callback);

  std::string focus_ring_id =
      chromeos::AccessibilityManager::Get()->GetFocusRingId(
          extension_misc::kSelectToSpeakExtensionId, "");

  ash::AccessibilityFocusRingControllerImpl* controller =
      ash::Shell::Get()->accessibility_focus_ring_controller();
  controller->SetNoFadeForTesting();
  const ash::AccessibilityFocusRingGroup* focus_ring_group =
      controller->GetFocusRingGroupForTesting(focus_ring_id);
  // No focus rings to start.
  EXPECT_EQ(nullptr, focus_ring_group);

  ui_test_utils::NavigateToURL(browser(), GURL("data:text/html;charset=utf-8,"
                                               "<p>This is some text</p>"));
  gfx::Rect bounds = GetWebContentsBounds();
  PrepareToWaitForFocusRingChanged();
  generator_->PressKey(ui::VKEY_LWIN, 0 /* flags */);
  generator_->MoveMouseTo(bounds.x(), bounds.y());
  generator_->PressLeftButton();

  // Expect a focus ring to have been drawn.
  WaitForFocusRingChanged();
  focus_ring_group = controller->GetFocusRingGroupForTesting(focus_ring_id);
  ASSERT_NE(nullptr, focus_ring_group);
  std::vector<std::unique_ptr<ash::AccessibilityFocusRingLayer>> const&
      focus_rings = focus_ring_group->focus_layers_for_testing();
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

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, ContinuesReadingDuringResize) {
  ActivateSelectToSpeakInWindowBounds(
      "data:text/html;charset=utf-8,<p>First paragraph</p>"
      "<div id='resize' style='width:300px; font-size: 10em'>"
      "<p>Second paragraph is longer than 300 pixels and will wrap when "
      "resized</p></div>");

  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(),
                                 "First paragraph*"));

  // Resize before second is spoken. If resizing caused errors finding the
  // inlineTextBoxes in the node, speech would be stopped early.
  ExecuteJavaScriptInForeground(
      "document.getElementById('resize').style.width='100px'");
  EXPECT_TRUE(
      base::MatchPattern(speech_monitor_.GetNextUtterance(), "*when*resized*"));
}

IN_PROC_BROWSER_TEST_F(SelectToSpeakTest, WorksWithStickyKeys) {
  AccessibilityManager::Get()->EnableStickyKeys(true);

  ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html;charset=utf-8,<p>This is some text</p>"));

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

  EXPECT_TRUE(base::MatchPattern(speech_monitor_.GetNextUtterance(),
                                 "This is some text*"));

  // Reset state.
  AccessibilityManager::Get()->EnableStickyKeys(false);
}

}  // namespace chromeos
