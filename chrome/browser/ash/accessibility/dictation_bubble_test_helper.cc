// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/dictation_bubble_test_helper.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/shell.h"
#include "ash/system/accessibility/dictation_bubble_controller.h"
#include "ash/system/accessibility/dictation_bubble_view.h"

namespace ash {

DictationBubbleTestHelper::DictationBubbleTestHelper() {
  // Ensure the bubble UI is initialized.
  GetController()->MaybeInitialize();
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
  DictationBubbleController* controller =
      Shell::Get()
          ->accessibility_controller()
          ->GetDictationBubbleControllerForTest();
  DCHECK(controller != nullptr);
  return controller;
}

}  // namespace ash
