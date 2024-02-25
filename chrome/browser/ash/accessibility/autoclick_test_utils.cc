// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/autoclick_test_utils.h"

#include "ash/accessibility/autoclick/autoclick_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/shell.h"
#include "ash/system/accessibility/autoclick_menu_bubble_controller.h"
#include "ash/system/accessibility/autoclick_menu_view.h"
#include "base/check_op.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/automation_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "ui/events/test/event_generator.h"

namespace {
const int kDefaultDelay = 5;
}  // namespace

namespace ash {

AutoclickTestUtils::AutoclickTestUtils(Profile* profile) {
  CHECK_EQ(false, AccessibilityManager::Get()->IsAutoclickEnabled());
  profile_ = profile;
  console_observer_ = std::make_unique<ExtensionConsoleErrorObserver>(
      profile_, extension_misc::kAccessibilityCommonExtensionId);

  SetAutoclickDelayMs(kDefaultDelay);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_->GetPrefs());
  pref_change_registrar_->Add(
      prefs::kAccessibilityAutoclickEventType,
      base::BindRepeating(&AutoclickTestUtils::OnEventTypePrefChanged,
                          GetWeakPtr()));

  automation_utils_ = std::make_unique<AutomationTestUtils>(
      extension_misc::kAccessibilityCommonExtensionId);
}

AutoclickTestUtils::~AutoclickTestUtils() {
  pref_change_registrar_.reset();
}

void AutoclickTestUtils::LoadAutoclick(bool install_automation_utils) {
  extensions::ExtensionHostTestHelper host_helper(
      profile_, extension_misc::kAccessibilityCommonExtensionId);
  AccessibilityManager::Get()->EnableAutoclick(true);
  Shell::Get()
      ->autoclick_controller()
      ->GetMenuBubbleControllerForTesting()
      ->SetAnimateForTesting(false);
  host_helper.WaitForHostCompletedFirstLoad();
  WaitForAutoclickReady();
  if (install_automation_utils) {
    automation_utils_->SetUpTestSupport();
  }
}

void AutoclickTestUtils::SetAutoclickDelayMs(int ms) {
  profile_->GetPrefs()->SetInteger(prefs::kAccessibilityAutoclickDelayMs, ms);
  profile_->GetPrefs()->CommitPendingWrite();
}

void AutoclickTestUtils::SetAutoclickEventTypeWithHover(
    ui::test::EventGenerator* generator,
    AutoclickEventType type) {
  // Check if we already have the right type selected.
  if (profile_->GetPrefs()->GetInteger(
          prefs::kAccessibilityAutoclickEventType) == static_cast<int>(type)) {
    return;
  }

  // Change the Autoclick delay to a value we know will work for this method
  // (in case it was set to a very large value before this method was called).
  int old_delay =
      profile_->GetPrefs()->GetInteger(prefs::kAccessibilityAutoclickDelayMs);
  SetAutoclickDelayMs(kDefaultDelay);

  // Find the menu button.
  AutoclickMenuView::ButtonId button_id;
  switch (type) {
    case AutoclickEventType::kLeftClick:
      button_id = AutoclickMenuView::ButtonId::kLeftClick;
      break;
    case AutoclickEventType::kRightClick:
      button_id = AutoclickMenuView::ButtonId::kRightClick;
      break;
    case AutoclickEventType::kDoubleClick:
      button_id = AutoclickMenuView::ButtonId::kDoubleClick;
      break;
    case AutoclickEventType::kDragAndDrop:
      button_id = AutoclickMenuView::ButtonId::kDragAndDrop;
      break;
    case AutoclickEventType::kScroll:
      button_id = AutoclickMenuView::ButtonId::kScroll;
      break;
    case AutoclickEventType::kNoAction:
      button_id = AutoclickMenuView::ButtonId::kPause;
      break;
  }
  AutoclickMenuView* menu_view = Shell::Get()
                                     ->autoclick_controller()
                                     ->GetMenuBubbleControllerForTesting()
                                     ->menu_view_;
  CHECK_NE(nullptr, menu_view);
  auto* button_view = menu_view->GetViewByID(static_cast<int>(button_id));
  CHECK_NE(nullptr, button_view);

  // Hover over it.
  const gfx::Rect bounds = button_view->GetBoundsInScreen();
  generator->MoveMouseTo(bounds.CenterPoint());

  // Wait for the pref change, indicating the button was pressed.
  base::RunLoop runner;
  pref_change_waiter_ = runner.QuitClosure();
  runner.Run();

  // Restore the delay to its previous value.
  SetAutoclickDelayMs(old_delay);
}

void AutoclickTestUtils::WaitForPageLoad(const std::string& url) {
  automation_utils_->WaitForPageLoad(url);
}

void AutoclickTestUtils::WaitForTextSelectionChangedEvent() {
  automation_utils_->WaitForTextSelectionChangedEvent();
}

void AutoclickTestUtils::HoverOverHtmlElement(
    ui::test::EventGenerator* generator,
    const std::string& name,
    const std::string& role) {
  const gfx::Rect bounds = automation_utils_->GetNodeBoundsInRoot(name, role);
  generator->MoveMouseTo(bounds.CenterPoint());
}

void AutoclickTestUtils::ObserveFocusRings() {
  // Create a callback for the focus ring observer.
  AccessibilityManager::Get()->SetFocusRingObserverForTest(base::BindRepeating(
      &AutoclickTestUtils::OnFocusRingChanged, GetWeakPtr()));
}

void AutoclickTestUtils::WaitForFocusRingChanged() {
  loop_runner_ = std::make_unique<base::RunLoop>();
  loop_runner_->Run();
}

gfx::Rect AutoclickTestUtils::GetNodeBoundsInRoot(const std::string& name,
                                                  const std::string& role) {
  return automation_utils_->GetNodeBoundsInRoot(name, role);
}

gfx::Rect AutoclickTestUtils::GetBoundsForNodeInRootByClassName(
    const std::string& class_name) {
  return automation_utils_->GetBoundsForNodeInRootByClassName(class_name);
}

void AutoclickTestUtils::WaitForAutoclickReady() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string script = base::StringPrintf(R"JS(
    (async function() {
      window.accessibilityCommon.setFeatureLoadCallbackForTest('autoclick',
          () => {
            chrome.test.sendScriptResult('ready');
          });
    })();
  )JS");
  base::Value result =
      extensions::browsertest_util::ExecuteScriptInBackgroundPage(
          profile_, extension_misc::kAccessibilityCommonExtensionId, script);
  ASSERT_EQ("ready", result);
}

void AutoclickTestUtils::OnEventTypePrefChanged() {
  if (pref_change_waiter_) {
    std::move(pref_change_waiter_).Run();
  }
}

void AutoclickTestUtils::OnFocusRingChanged() {
  if (loop_runner_ && loop_runner_->running()) {
    loop_runner_->Quit();
  }
}

base::WeakPtr<AutoclickTestUtils> AutoclickTestUtils::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
