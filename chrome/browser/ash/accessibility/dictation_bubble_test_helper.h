// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_DICTATION_BUBBLE_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_DICTATION_BUBBLE_TEST_HELPER_H_

#include <string>
#include <vector>

#include "ash/system/accessibility/dictation_bubble_controller.h"
#include "base/functional/callback_forward.h"

namespace ash {

class DictationBubbleController;
enum class DictationBubbleIconType;

class DictationBubbleTestHelper : public DictationBubbleController::Observer {
 public:
  DictationBubbleTestHelper();
  ~DictationBubbleTestHelper() override;
  DictationBubbleTestHelper(const DictationBubbleTestHelper&) = delete;
  DictationBubbleTestHelper& operator=(const DictationBubbleTestHelper&) =
      delete;

  // Returns true if the Dictation bubble UI is currently visible.
  bool IsVisible();
  // Returns the currently visible icon in the Dictation bubble UI top row.
  DictationBubbleIconType GetVisibleIcon();
  // Returns the current text of the Dictation bubble UI top row.
  std::u16string GetText();
  // Returns true if the currently visible hints match `expected`.
  bool HasVisibleHints(const std::vector<std::u16string>& expected);

  // Waiter functions.
  void WaitForVisibility(bool visible);
  void WaitForVisibleIcon(DictationBubbleIconType icon);
  void WaitForVisibleText(const std::u16string& text);
  void WaitForVisibleHints(const std::vector<std::u16string>& hints);

  // DictationBubbleController::Observer
  void OnBubbleUpdated() override;

 private:
  // Returns true if the standby view is visible in the top row.
  bool IsStandbyViewVisible();
  // Returns true if the macro succeeded image is visible in the top row.
  bool IsMacroSucceededImageVisible();
  // Returns true if the macro failed image is visible in the top row.
  bool IsMacroFailedImageVisible();
  // Returns the currently visible hints in the hint view.
  std::vector<std::u16string> GetVisibleHints();
  // Returns controller for the DicatationBubbleView.
  DictationBubbleController* GetController();

  // Expected properties.
  bool expected_visible_;
  DictationBubbleIconType expected_icon_;
  std::u16string expected_text_;
  std::vector<std::u16string> expected_hints_;

  // Functions that run once the expected property is shown in the bubble UI.
  base::OnceClosure visible_closure_;
  base::OnceClosure icon_closure_;
  base::OnceClosure text_closure_;
  base::OnceClosure hints_closure_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_DICTATION_BUBBLE_TEST_HELPER_H_
