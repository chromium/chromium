// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_DICTATION_BUBBLE_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_DICTATION_BUBBLE_VIEW_H_

#include <optional>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {
class TopRowView;
}  // namespace

enum class DictationBubbleHintType;
enum class DictationBubbleIconType;

class DictationHintView;

// View for the Dictation bubble.
class ASH_EXPORT DictationBubbleView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(DictationBubbleView, views::BubbleDialogDelegateView)

 public:
  DictationBubbleView();
  DictationBubbleView(const DictationBubbleView&) = delete;
  DictationBubbleView& operator=(const DictationBubbleView&) = delete;
  ~DictationBubbleView() override;

  // Updates the visibility of all child views, displays the icon/animation
  // specified by `icon`, and updates text content and size of this view.
  void Update(DictationBubbleIconType icon,
              const std::optional<std::u16string>& text,
              const std::optional<std::vector<DictationBubbleHintType>>& hints);

  // views::BubbleDialogDelegateView:
  void Init() override;
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;

  std::u16string GetTextForTesting();
  bool IsStandbyViewVisibleForTesting();
  bool IsMacroSucceededImageVisibleForTesting();
  bool IsMacroFailedImageVisibleForTesting();
  SkColor GetLabelBackgroundColorForTesting();
  SkColor GetLabelTextColorForTesting();
  std::vector<std::u16string> GetVisibleHintsForTesting();

 private:
  friend class DictationBubbleControllerTest;

  views::View* GetTopRowView();

  raw_ptr<TopRowView> top_row_view_ = nullptr;
  raw_ptr<DictationHintView> hint_view_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   DictationBubbleView,
                   views::BubbleDialogDelegateView)
END_VIEW_BUILDER

// View responsible for showing hints for Dictation commands.
// **important**: Chromevox expects this class to have a specific name to
// compute when to announce hints differently. Don't change it!
class ASH_EXPORT DictationHintView : public views::View {
  METADATA_HEADER(DictationHintView, views::View)

 public:
  DictationHintView();
  DictationHintView(const DictationHintView&) = delete;
  DictationHintView& operator=(const DictationHintView&) = delete;
  ~DictationHintView() override;

  // Updates the text content and visibility of all labels in this view.
  void Update(const std::optional<std::vector<DictationBubbleHintType>>& hints);

 private:
  friend class DictationBubbleView;

  static const size_t kMaxLabelHints = 5;

  // Labels containing hints for users of Dictation. A max of five hints can be
  // shown at any given time.
  std::vector<raw_ptr<views::Label>> labels_{kMaxLabelHints, nullptr};
};

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::DictationBubbleView)

#endif  // ASH_SYSTEM_ACCESSIBILITY_DICTATION_BUBBLE_VIEW_H_
