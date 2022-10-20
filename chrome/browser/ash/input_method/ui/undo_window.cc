// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/ui/undo_window.h"

#include "ash/public/cpp/style/color_provider.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/input_method/ui/border_factory.h"
#include "chrome/browser/ash/input_method/ui/colors.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/wm/core/window_animations.h"

#include <iostream>
namespace ui {
namespace ime {

namespace {
constexpr int kHeight = 28;
constexpr int kPadding = 0;
constexpr int kIconSize = 16;
// TODO(crbug/1099044): Update and use cros_colors.json5
constexpr cros_styles::ColorName kButtonHighlightColor =
    cros_styles::ColorName::kRippleColor;

}  // namespace

UndoWindow::UndoWindow(gfx::NativeView parent, AssistiveDelegate* delegate)
    : delegate_(delegate) {
  DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
  SetCanActivate(false);
  DCHECK(parent);
  set_parent_window(parent);
  set_margins(gfx::Insets(kPadding));
  SetArrow(views::BubbleBorder::Arrow::BOTTOM_LEFT);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  std::u16string undo_button_text =
      l10n_util::GetStringUTF16(IDS_SUGGESTION_AUTOCORRECT_UNDO_TEXT);

  undo_button_ = AddChildView(std::make_unique<views::LabelButton>(
      base::BindRepeating(&UndoWindow::UndoButtonPressed,
                          base::Unretained(this)),
      undo_button_text));
  undo_button_->SetText(undo_button_text);
  undo_button_->SetImageLabelSpacing(
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  undo_button_->SetBackground(nullptr);
  undo_button_->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  undo_button_->SetMaxSize(
      gfx::Size(std::numeric_limits<int>::max(), kHeight - 2 * kPadding));
}

void UndoWindow::OnThemeChanged() {
  // Without the scoped light mode, default for ash color provider is dark mode,
  // which is bad.
  ash::ScopedLightModeAsDefault scoped_light_mode_as_default;
  undo_button_->SetImage(
      views::Button::ButtonState::STATE_NORMAL,
      gfx::CreateVectorIcon(
          kAutocorrectUndoIcon, kIconSize,
          ash::ColorProvider::Get()->GetContentLayerColor(
              ash::ColorProvider::ContentLayerType::kIconColorPrimary)));
  undo_button_->SetEnabledTextColors(
      ash::ColorProvider::Get()->GetContentLayerColor(
          ash::ColorProvider::ContentLayerType::kTextColorSecondary));
  BubbleDialogDelegateView::OnThemeChanged();
}

UndoWindow::~UndoWindow() = default;

views::Widget* UndoWindow::InitWidget() {
  views::Widget* widget = BubbleDialogDelegateView::CreateBubble(this);

  wm::SetWindowVisibilityAnimationTransition(widget->GetNativeView(),
                                             wm::ANIMATE_NONE);

  GetBubbleFrameView()->SetBubbleBorder(
      GetBorderForWindow(WindowBorderType::Undo));
  GetBubbleFrameView()->OnThemeChanged();
  return widget;
}

void UndoWindow::Hide() {
  GetWidget()->Close();
}

void UndoWindow::Show() {
  GetWidget()->Show();
}

void UndoWindow::SetBounds(const gfx::Rect& word_bounds) {
  SetAnchorRect(word_bounds);
}

void UndoWindow::SetButtonHighlighted(const AssistiveWindowButton& button,
                                      bool highlighted) {
  if (button.id != ButtonId::kUndo)
    return;

  bool currently_hightlighted = undo_button_->background() != nullptr;
  if (highlighted == currently_hightlighted)
    return;

  undo_button_->SetBackground(
      highlighted ? views::CreateSolidBackground(
                        ResolveSemanticColor(kButtonHighlightColor))
                  : nullptr);
}

views::Button* UndoWindow::GetUndoButtonForTesting() {
  return undo_button_;
}

void UndoWindow::UndoButtonPressed() {
  const AssistiveWindowButton button = {
      .id = ButtonId::kUndo,
      .window_type = ash::ime::AssistiveWindowType::kUndoWindow};
  SetButtonHighlighted(button, true);
  delegate_->AssistiveWindowButtonClicked(button);
}

BEGIN_METADATA(UndoWindow, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace ime
}  // namespace ui
