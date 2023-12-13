// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/dictation_bubble_test_helper.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/shell.h"
#include "ash/system/accessibility/dictation_bubble_view.h"
#include "base/run_loop.h"

namespace ash {

DictationBubbleTestHelper::DictationBubbleTestHelper() {
  // Ensure the bubble UI is initialized.
  GetController()->MaybeInitialize();
  GetController()->AddObserver(this);
}

DictationBubbleTestHelper::~DictationBubbleTestHelper() {
  auto* controller = GetController();
  if (controller) {
    controller->RemoveObserver(this);
  }
}

bool DictationBubbleTestHelper::IsVisible() {
  return GetController()->widget_->IsVisible();
}

DictationBubbleIconType DictationBubbleTestHelper::GetVisibleIcon() {
  DCHECK_GE(1, IsStandbyViewVisible() + IsMacroSucceededImageVisible() +
                   IsMacroFailedImageVisible())
      << "No more than one icon should be visible!";
  if (IsStandbyViewVisible())
    return DictationBubbleIconType::kStandby;
  if (IsMacroSucceededImageVisible())
    return DictationBubbleIconType::kMacroSuccess;
  if (IsMacroFailedImageVisible())
    return DictationBubbleIconType::kMacroFail;
  return DictationBubbleIconType::kHidden;
}

std::u16string DictationBubbleTestHelper::GetText() {
  return GetController()->dictation_bubble_view_->GetTextForTesting();
}

bool DictationBubbleTestHelper::HasVisibleHints(
    const std::vector<std::u16string>& expected) {
  std::vector<std::u16string> actual = GetVisibleHints();
  if (expected.size() != actual.size())
    return false;

  for (size_t i = 0; i < expected.size(); ++i) {
    if (expected[i] != actual[i])
      return false;
  }

  return true;
}

bool DictationBubbleTestHelper::IsStandbyViewVisible() {
  return GetController()
      ->dictation_bubble_view_->IsStandbyViewVisibleForTesting();
}

bool DictationBubbleTestHelper::IsMacroSucceededImageVisible() {
  return GetController()
      ->dictation_bubble_view_->IsMacroSucceededImageVisibleForTesting();
}

bool DictationBubbleTestHelper::IsMacroFailedImageVisible() {
  return GetController()
      ->dictation_bubble_view_->IsMacroFailedImageVisibleForTesting();
}

std::vector<std::u16string> DictationBubbleTestHelper::GetVisibleHints() {
  return GetController()->dictation_bubble_view_->GetVisibleHintsForTesting();
}

DictationBubbleController* DictationBubbleTestHelper::GetController() {
  if (!Shell::HasInstance()) {
    return nullptr;
  }

  return Shell::Get()
      ->accessibility_controller()
      ->GetDictationBubbleControllerForTest();
}

void DictationBubbleTestHelper::WaitForVisibility(bool visible) {
  if (IsVisible() == visible) {
    return;
  }

  expected_visible_ = visible;
  base::RunLoop loop;
  visible_closure_ = loop.QuitClosure();
  loop.Run();
}

void DictationBubbleTestHelper::WaitForVisibleIcon(
    DictationBubbleIconType icon) {
  if (GetVisibleIcon() == icon) {
    return;
  }

  expected_icon_ = icon;
  base::RunLoop loop;
  icon_closure_ = loop.QuitClosure();
  loop.Run();
}

void DictationBubbleTestHelper::WaitForVisibleText(const std::u16string& text) {
  if (GetText() == text) {
    return;
  }

  expected_text_ = text;
  base::RunLoop loop;
  text_closure_ = loop.QuitClosure();
  loop.Run();
}

void DictationBubbleTestHelper::WaitForVisibleHints(
    const std::vector<std::u16string>& hints) {
  if (HasVisibleHints(hints)) {
    return;
  }

  expected_hints_ = hints;
  base::RunLoop loop;
  hints_closure_ = loop.QuitClosure();
  loop.Run();
}

void DictationBubbleTestHelper::OnBubbleUpdated() {
  if (!visible_closure_.is_null() && IsVisible() == expected_visible_) {
    std::move(visible_closure_).Run();
  }
  if (!icon_closure_.is_null() && GetVisibleIcon() == expected_icon_) {
    std::move(icon_closure_).Run();
  }
  if (!text_closure_.is_null() && GetText() == expected_text_) {
    std::move(text_closure_).Run();
  }
  if (!hints_closure_.is_null() && HasVisibleHints(expected_hints_)) {
    std::move(hints_closure_).Run();
  }
}

}  // namespace ash
