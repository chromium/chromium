// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/resize_confirmation_dialog_view.h"

#include <memory>

#include "ash/components/arc/compat_mode/overlay_dialog.h"
#include "ash/components/arc/compat_mode/style/arc_color_provider.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/checkbox_group.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/scoped_observation.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace arc {

void ResizeConfirmationDialogView::TestApi::SelectDoNotAskCheckbox() {
  view_->do_not_ask_checkbox_jelly_->SetSelected(true);
}

ResizeConfirmationDialogView::ResizeConfirmationDialogView(
    views::Widget* parent,
    ResizeConfirmationCallback callback)
    : callback_(std::move(callback)) {
  // Setup delegate.
  SetArrow(views::BubbleBorder::Arrow::FLOAT);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_parent_window(parent->GetNativeWindow());
  set_title_margins(gfx::Insets());
  SetTitle(
      l10n_util::GetStringUTF16(IDS_ASH_ARC_APP_COMPAT_RESIZE_CONFIRM_TITLE));
  SetShowTitle(false);
  set_margins(gfx::Insets());
  SetAnchorView(parent->GetContentsView());
  SetAccessibleWindowRole(ax::mojom::Role::kDialog);
  set_adjust_if_offscreen(false);
  set_close_on_deactivate(true);

  // Setup view.
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->SetOrientation(views::BoxLayout::Orientation::kVertical);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_inside_border_insets(gfx::Insets::TLBR(32, 32, 28, 32));
  layout->set_between_child_spacing(16);

  const raw_ptr<views::Label> title = AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_ASH_ARC_APP_COMPAT_RESIZE_CONFIRM_TITLE))
          .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
          .SetMultiLine(true)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetAllowCharacterBreak(true)
          .SetFontList(
              views::TypographyProvider::Get()
                  .GetFont(views::style::TextContext::CONTEXT_DIALOG_TITLE,
                           views::style::TextStyle::STYLE_PRIMARY)
                  .DeriveWithWeight(gfx::Font::Weight::MEDIUM))
          .Build());
  ash::TypographyProvider::Get()->StyleLabel(
      ash::TypographyToken::kCrosDisplay7, *title);
  title->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);

  AddChildView(MakeContentsView());
  AddChildView(MakeButtonsView());
}

ResizeConfirmationDialogView::~ResizeConfirmationDialogView() = default;

gfx::Size ResizeConfirmationDialogView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  int width = provider->GetDistanceMetric(
      views::DistanceMetric::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);

  const int kHorizontalMarginDp = 36;
  const auto* widget = GetWidget();
  if (widget && widget->parent()) {
    width =
        std::min(width, widget->parent()->GetWindowBoundsInScreen().width() -
                            kHorizontalMarginDp * 2);
  }
  return gfx::Size(width,
                   GetLayoutManager()->GetPreferredHeightForWidth(this, width));
}

void ResizeConfirmationDialogView::AddedToWidget() {
  const int kCornerRadius = 20;
  auto* const frame = GetBubbleFrameView();
  if (frame) {
    frame->SetCornerRadius(kCornerRadius);
  }

  widget_observation_.Observe(GetWidget());
}

void ResizeConfirmationDialogView::OnThemeChanged() {
  views::BubbleDialogDelegateView::OnThemeChanged();
}

void ResizeConfirmationDialogView::OnWidgetClosing(views::Widget* widget) {
  if (!callback_) {
    return;
  }
  std::move(callback_).Run(/*accept=*/false, /*do_not_ask_again=*/false);
}

std::unique_ptr<views::View> ResizeConfirmationDialogView::MakeContentsView() {
  auto contents_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetBetweenChildSpacing(16)
          .Build();

  const raw_ptr<views::Label> body = contents_view->AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_ASH_ARC_APP_COMPAT_RESIZE_CONFIRM_BODY))
          .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
          .SetTextStyle(views::style::STYLE_SECONDARY)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetMultiLine(true)
          .Build());
  ash::TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosBody1,
                                             *body);
  body->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);

  const raw_ptr<ash::CheckboxGroup> checkbox_group =
      contents_view->AddChildView(std::make_unique<ash::CheckboxGroup>(
          bounds().width() - 32 * 2, gfx::Insets::TLBR(0, 0, 8, 0), 0,
          gfx::Insets(), ash::Checkbox::kImageLabelSpacingDP));
  do_not_ask_checkbox_jelly_ = checkbox_group->AddButton(
      ash::OptionButtonBase::PressedCallback(),
      l10n_util::GetStringUTF16(
          IDS_ASH_ARC_APP_COMPAT_RESIZE_CONFIRM_DONT_ASK_ME));
  do_not_ask_checkbox_jelly_->SetLabelStyle(ash::TypographyToken::kCrosButton2);
  do_not_ask_checkbox_jelly_->SetLabelColorId(cros_tokens::kCrosSysOnSurface);
  return contents_view;
}

std::unique_ptr<views::View> ResizeConfirmationDialogView::MakeButtonsView() {
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  auto builder =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd)
          .SetBetweenChildSpacing(provider->GetDistanceMetric(
              views::DistanceMetric::DISTANCE_RELATED_BUTTON_HORIZONTAL));

  builder.AddChildren(
      views::Builder<ash::PillButton>()  // Cancel button.
          .CopyAddressTo(&cancel_button_)
          .SetCallback(base::BindRepeating(
              &ResizeConfirmationDialogView::OnButtonClicked,
              base::Unretained(this), false,
              views::Widget::ClosedReason::kCancelButtonClicked))
          .SetText(l10n_util::GetStringUTF16(IDS_APP_CANCEL))
          .SetIsDefault(false)
          .SetPillButtonType(ash::PillButton::kSecondaryLargeWithoutIcon),
      views::Builder<ash::PillButton>()  // Accept button.
          .CopyAddressTo(&accept_button_)
          .SetCallback(base::BindRepeating(
              &ResizeConfirmationDialogView::OnButtonClicked,
              base::Unretained(this), true,
              views::Widget::ClosedReason::kCancelButtonClicked))
          .SetText(l10n_util::GetStringUTF16(
              IDS_ASH_ARC_APP_COMPAT_RESIZE_CONFIRM_ACCEPT))
          .SetIsDefault(true)
          .SetPillButtonType(ash::PillButton::kPrimaryLargeWithoutIcon));
  return std::move(builder).Build();
}

void ResizeConfirmationDialogView::OnButtonClicked(
    bool accept,
    views::Widget::ClosedReason close_reason) {
  if (!callback_)
    return;
  std::move(callback_).Run(accept, do_not_ask_checkbox_jelly_->selected());

  auto* const widget = GetWidget();
  if (widget) {
    widget->CloseWithReason(close_reason);
  }
}

void ResizeConfirmationDialogView::Show(views::Widget* parent,
                                        ResizeConfirmationCallback callback) {
  auto remove_overlay = base::BindOnce(
      &OverlayDialog::CloseIfAny, base::Unretained(parent->GetNativeWindow()));

  auto dialog_view = std::make_unique<ResizeConfirmationDialogView>(
      parent, std::move(callback).Then(std::move(remove_overlay)));

  OverlayDialog::Show(
      parent->GetNativeWindow(),
      base::BindOnce(&ResizeConfirmationDialogView::OnButtonClicked,
                     base::Unretained(dialog_view.get()), /*accept=*/false,
                     views::Widget::ClosedReason::kUnspecified),
      /*dialog_view=*/nullptr);

  views::BubbleDialogDelegateView::CreateBubble(std::move(dialog_view))->Show();
}

BEGIN_METADATA(ResizeConfirmationDialogView)
END_METADATA

}  // namespace arc
