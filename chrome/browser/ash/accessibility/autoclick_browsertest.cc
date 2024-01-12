// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/autoclick/autoclick_controller.h"
#include "ash/accessibility/ui/accessibility_focus_ring_controller_impl.h"
#include "ash/accessibility/ui/accessibility_focus_ring_layer.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/shell.h"
#include "base/test/bind.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/accessibility/service/accessibility_service_router_factory.h"
#include "chrome/browser/ash/accessibility/accessibility_feature_browsertest.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/autoclick_test_utils.h"
#include "chrome/browser/ash/accessibility/service/fake_accessibility_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/test/event_generator.h"
#include "url/url_constants.h"

namespace ash {

namespace {

const char* kShowButtonOnClickUrl =
    "data:text/html,"
    "<input type='button' value='click me'"
    "onclick=\"document.getElementById('result').removeAttribute('hidden')\">"
    "<input type='button' id='result' hidden value='show me'>";

}  // namespace

// Tests that Automatic clicks works with elements in the browser.
class AutoclickBrowserTest : public AccessibilityFeatureBrowserTest {
 public:
  AutoclickBrowserTest(const AutoclickBrowserTest&) = delete;
  AutoclickBrowserTest& operator=(const AutoclickBrowserTest&) = delete;

 protected:
  AutoclickBrowserTest() = default;
  ~AutoclickBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
    generator_ = std::make_unique<ui::test::EventGenerator>(root_window);
    autoclick_test_utils_ = std::make_unique<AutoclickTestUtils>(GetProfile());
    AccessibilityFeatureBrowserTest::SetUpOnMainThread();
    NavigateToUrl(GURL(url::kAboutBlankURL));
  }

  void TearDownOnMainThread() override { autoclick_test_utils_.reset(); }

  PrefService* GetPrefs() { return GetProfile()->GetPrefs(); }

  // Loads a page with the given URL and then starts up Autoclick.
  void LoadURLAndAutoclick(const std::string& url) {
    NavigateToUrl(GURL(url));
    autoclick_test_utils_->LoadAutoclick();
    autoclick_test_utils_->WaitForPageLoad(url);
  }

  ui::test::EventGenerator* generator() { return generator_.get(); }
  AutoclickTestUtils* utils() { return autoclick_test_utils_.get(); }

 private:
  std::unique_ptr<ui::test::EventGenerator> generator_;
  std::unique_ptr<AutoclickTestUtils> autoclick_test_utils_;
};

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest, LeftClickButtonOnHover) {
  LoadURLAndAutoclick(kShowButtonOnClickUrl);
  // No need to change click type: Default should be right-click.
  utils()->HoverOverHtmlElement(generator(), "click me", "button");

  // Wait for button to be shown.
  utils()->GetNodeBoundsInRoot("show me", "button");
}

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest, DoubleClickHover) {
  LoadURLAndAutoclick(
      "data:text/html;charset=utf-8,"
      "<input type='text' id='text_field'"
      "value='peanutbuttersandwichmadewithjam'>");
  utils()->SetAutoclickEventTypeWithHover(generator(),
                                          AutoclickEventType::kDoubleClick);

  // Double-clicking over the text field should result in the text being
  // selected.
  utils()->HoverOverHtmlElement(generator(), "peanutbuttersandwichmadewithjam",
                                "staticText");

  utils()->WaitForTextSelectionChangedEvent();
}

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest, ClickAndDrag) {
  LoadURLAndAutoclick(
      "data:text/html;charset=utf-8,"
      "<input type='text' id='text_field'"
      "value='peanutbuttersandwichmadewithjam'>");
  utils()->SetAutoclickEventTypeWithHover(generator(),
                                          AutoclickEventType::kDragAndDrop);

  gfx::Rect bounds = utils()->GetNodeBoundsInRoot(
      "peanutbuttersandwichmadewithjam", "staticText");

  // First hover causes a down click even that changes the caret.
  generator()->MoveMouseTo(
      gfx::Point(bounds.left_center().y(), bounds.x() + 10));
  utils()->WaitForTextSelectionChangedEvent();

  // Second hover causes a selection.
  generator()->MoveMouseTo(bounds.right_center());
  utils()->WaitForTextSelectionChangedEvent();
}

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest,
                       RightClickOnHoverOpensContextMenu) {
  LoadURLAndAutoclick(
      "data:text/html;charset=utf-8,"
      "<input type='text' id='text_field' value='stop copying me'>");
  utils()->SetAutoclickEventTypeWithHover(generator(),
                                          AutoclickEventType::kRightClick);

  // Right clicking over the text field should result in a context menu.
  utils()->HoverOverHtmlElement(generator(), "stop copying me", "staticText");

  // When the context menu is shown, it has options for copy/paste
  // because this is a textarea.
  utils()->GetNodeBoundsInRoot("Copy Ctrl+C", "menuItem");
  utils()->GetNodeBoundsInRoot("Paste Ctrl+V", "menuItem");
}

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest,
                       ScrollHoverHighlightsScrollableArea) {
  utils()->ObserveFocusRings();

  const std::string kQuoteText =
      "'Whatever you choose to do, leave tracks. That means don't do it just "
      "for yourself. You will want to leave the world a little better for your "
      "having lived.'";

  LoadURLAndAutoclick(
      "data:text/html;charset=utf-8,"
      "<textarea id='test_textarea' class='scrollableField' rows='2'' "
      "cols='20'>" +
      kQuoteText + "</textarea>");

  gfx::Rect bounds =
      utils()->GetBoundsForNodeInRootByClassName("scrollableField");
  gfx::Rect found_bounds;
  base::RunLoop waiter;
  Shell::Get()->autoclick_controller()->SetScrollableBoundsCallbackForTesting(
      base::BindLambdaForTesting([&waiter, &bounds, &found_bounds](
                                     const gfx::Rect& scrollable_bounds) {
        found_bounds = scrollable_bounds;
        if (scrollable_bounds == bounds && waiter.running()) {
          waiter.Quit();
        }
      }));

  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();
  std::string focus_ring_id = AccessibilityManager::Get()->GetFocusRingId(
      ax::mojom::AssistiveTechnologyType::kAutoClick, "");
  const AccessibilityFocusRingGroup* focus_ring_group =
      controller->GetFocusRingGroupForTesting(focus_ring_id);
  // No focus rings to start.
  EXPECT_EQ(nullptr, focus_ring_group);

  utils()->SetAutoclickEventTypeWithHover(generator(),
                                          AutoclickEventType::kScroll);

  utils()->HoverOverHtmlElement(generator(), kQuoteText, "staticText");
  utils()->WaitForFocusRingChanged();

  focus_ring_group = controller->GetFocusRingGroupForTesting(focus_ring_id);
  ASSERT_NE(nullptr, focus_ring_group);
  std::vector<std::unique_ptr<AccessibilityFocusRingLayer>> const& focus_rings =
      focus_ring_group->focus_layers_for_testing();
  ASSERT_EQ(focus_rings.size(), 1u);

  if (found_bounds != bounds) {
    // Wait for bounds changed.
    waiter.Run();
  }
}

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest, LongDelay) {
  utils()->SetAutoclickDelayMs(500);
  LoadURLAndAutoclick(kShowButtonOnClickUrl);

  base::ElapsedTimer timer;
  utils()->HoverOverHtmlElement(generator(), "click me", "button");
  utils()->GetNodeBoundsInRoot("show me", "button");
  EXPECT_GT(timer.Elapsed().InMilliseconds(), 500);
}

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest, PauseAutoclick) {
  utils()->SetAutoclickDelayMs(5);
  LoadURLAndAutoclick(
      "data:text/html,"
      "<input type='button' value='click me'"
      "onclick='window.close()'>");
  utils()->SetAutoclickEventTypeWithHover(generator(),
                                          AutoclickEventType::kNoAction);

  base::OneShotTimer timer;
  base::RunLoop runner;
  utils()->HoverOverHtmlElement(generator(), "click me", "button");
  timer.Start(FROM_HERE, base::Milliseconds(2000),
              base::BindLambdaForTesting([&runner, this]() {
                runner.Quit();
                // If autoclick was enabled, the webpage would have
                // been closed, and this would fail.
                utils()->GetNodeBoundsInRoot("click me", "button");
              }));
  runner.Run();
}

class AutoclickWithAccessibilityServiceTest : public AutoclickBrowserTest {
 public:
  AutoclickWithAccessibilityServiceTest() = default;
  ~AutoclickWithAccessibilityServiceTest() override = default;
  AutoclickWithAccessibilityServiceTest(
      const AutoclickWithAccessibilityServiceTest&) = delete;
  AutoclickWithAccessibilityServiceTest& operator=(
      const AutoclickWithAccessibilityServiceTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityService);
  }

  void SetUpOnMainThread() override {
    AutoclickBrowserTest::SetUpOnMainThread();
    // Replaces normal AccessibilityService with a fake one.
    ax::AccessibilityServiceRouterFactory::GetInstanceForTest()
        ->SetTestingFactoryAndUse(
            ash::AccessibilityManager::Get()->profile(),
            base::BindRepeating(&AutoclickWithAccessibilityServiceTest::
                                    CreateTestAccessibilityService,
                                base::Unretained(this)));
  }

 protected:
  // Unowned.
  raw_ptr<FakeAccessibilityService, DanglingUntriaged> fake_service_ = nullptr;

 private:
  std::unique_ptr<KeyedService> CreateTestAccessibilityService(
      content::BrowserContext* context) {
    std::unique_ptr<FakeAccessibilityService> fake_service =
        std::make_unique<FakeAccessibilityService>();
    fake_service_ = fake_service.get();
    return std::move(fake_service);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(b/262637071): When the AccessibilityService is on (instead of a fake),
// check the focus ring bounds too, as autoclick JS should set these.
IN_PROC_BROWSER_TEST_F(AutoclickWithAccessibilityServiceTest,
                       ScrollableBoundsPlumbing) {
  const std::string kQuoteText =
      "'Whatever you choose to do, leave tracks. That means don't do it just "
      "for yourself. You will want to leave the world a little better for your "
      "having lived.'";

  LoadURLAndAutoclick(
      "data:text/html;charset=utf-8,"
      "<textarea id='test_textarea' class='scrollableField' rows='2'' "
      "cols='20'>" +
      kQuoteText + "</textarea>");
  gfx::Rect bounds =
      utils()->GetBoundsForNodeInRootByClassName("scrollableField");

  fake_service_->BindAnotherAutoclickClient();

  utils()->SetAutoclickEventTypeWithHover(generator(),
                                          AutoclickEventType::kScroll);

  fake_service_->set_autoclick_scrollable_bounds(bounds);
  base::RunLoop waiter;
  Shell::Get()->autoclick_controller()->SetScrollableBoundsCallbackForTesting(
      base::BindLambdaForTesting(
          [&waiter, &bounds](const gfx::Rect& scrollable_bounds) {
            if (scrollable_bounds == bounds) {
              waiter.Quit();
            }
          }));
  utils()->HoverOverHtmlElement(generator(), kQuoteText, "staticText");
  waiter.Run();
}

}  // namespace ash
