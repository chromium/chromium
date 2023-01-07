// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_UI_ASSISTIVE_ACCESSIBILITY_VIEW_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_UI_ASSISTIVE_ACCESSIBILITY_VIEW_H_

#include "chrome/browser/ash/input_method/ui/suggestion_accessibility_label.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/view_factory.h"

namespace ui {
namespace ime {

// This is en empty view box holding an accessibility label, through which we
// can make ChromeVox announcement incurred by assistive features.
class UI_CHROMEOS_EXPORT AssistiveAccessibilityView
    : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(AssistiveAccessibilityView);
  explicit AssistiveAccessibilityView(gfx::NativeView parent);
  AssistiveAccessibilityView(const AssistiveAccessibilityView&) = delete;
  AssistiveAccessibilityView& operator=(const AssistiveAccessibilityView&) =
      delete;
  ~AssistiveAccessibilityView() override;

  virtual void Announce(const std::u16string& message);

 protected:
  AssistiveAccessibilityView();

 private:
  SuggestionAccessibilityLabel* accessibility_label_ = nullptr;
};

BEGIN_VIEW_BUILDER(UI_CHROMEOS_EXPORT,
                   AssistiveAccessibilityView,
                   views::BubbleDialogDelegateView)
END_VIEW_BUILDER

}  // namespace ime
}  // namespace ui

DEFINE_VIEW_BUILDER(UI_CHROMEOS_EXPORT, ui::ime::AssistiveAccessibilityView)

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_UI_ASSISTIVE_ACCESSIBILITY_VIEW_H_
