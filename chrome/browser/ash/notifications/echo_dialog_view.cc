// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/echo_dialog_view.h"

#include <stddef.h>

#include "base/no_destructor.h"
#include "chrome/browser/ash/notifications/echo_dialog_listener.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/font.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

EchoDialogView::ShowCallback& GetShowCallbackForTesting() {
  static base::NoDestructor<EchoDialogView::ShowCallback> show_callback;
  return *show_callback;
}

}  // namespace

EchoDialogView::EchoDialogView(EchoDialogListener* listener,
                               const EchoDialogView::Params& params) {
  auto* learn_more_button = DialogDelegate::SetExtraView(
      views::CreateVectorImageButtonWithNativeTheme(
          base::BindRepeating(&EchoDialogListener::OnMoreInfoLinkClicked,
                              base::Unretained(listener)),
          vector_icons::kHelpOutlineIcon));
  learn_more_button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_CHROMEOS_ACC_LEARN_MORE));

  if (params.echo_enabled) {
    DialogDelegate::SetButtons(
        static_cast<int>(ui::mojom::DialogButton::kOk) |
        static_cast<int>(ui::mojom::DialogButton::kCancel));
    DialogDelegate::SetButtonLabel(
        ui::mojom::DialogButton::kOk,
        l10n_util::GetStringUTF16(IDS_OFFERS_CONSENT_INFOBAR_ENABLE_BUTTON));
    DialogDelegate::SetButtonLabel(
        ui::mojom::DialogButton::kCancel,
        l10n_util::GetStringUTF16(IDS_OFFERS_CONSENT_INFOBAR_DISABLE_BUTTON));
    InitForEnabledEcho(params.service_name, params.origin);
  } else {
    DialogDelegate::SetButtons(
        static_cast<int>(ui::mojom::DialogButton::kCancel));
    DialogDelegate::SetButtonLabel(
        ui::mojom::DialogButton::kCancel,
        l10n_util::GetStringUTF16(IDS_ECHO_CONSENT_DISMISS_BUTTON));
    InitForDisabledEcho();
  }

  DialogDelegate::SetAcceptCallback(base::BindOnce(
      &EchoDialogListener::OnAccept, base::Unretained(listener)));
  DialogDelegate::SetCancelCallback(base::BindOnce(
      &EchoDialogListener::OnCancel, base::Unretained(listener)));

  DialogDelegate::SetShowTitle(false);
  DialogDelegate::SetShowCloseButton(false);

  DialogDelegate::SetModalType(ui::mojom::ModalType::kWindow);
  DialogDelegate::set_fixed_width(
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
}

EchoDialogView::~EchoDialogView() = default;

void EchoDialogView::Show(gfx::NativeWindow parent) {
  views::DialogDelegate::CreateDialogWidget(this, parent, parent);
  GetWidget()->SetSize(GetWidget()->GetRootView()->GetPreferredSize());
  GetWidget()->Show();

  if (GetShowCallbackForTesting()) {
    std::move(GetShowCallbackForTesting()).Run(this);
  }
}

void EchoDialogView::AddShowCallbackForTesting(ShowCallback callback) {
  GetShowCallbackForTesting() = std::move(callback);
}

void EchoDialogView::InitForEnabledEcho(const std::u16string& service_name,
                                        const std::u16string& origin) {
  size_t offset;
  std::u16string text = l10n_util::GetStringFUTF16(IDS_ECHO_CONSENT_DIALOG_TEXT,
                                                   service_name, &offset);

  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(text);

  views::StyledLabel::RangeStyleInfo service_name_style;
  gfx::FontList font_list = label->GetFontList();
  service_name_style.custom_font =
      font_list.DeriveWithStyle(gfx::Font::UNDERLINE);
  service_name_style.tooltip = origin;
  label->AddStyleRange(gfx::Range(offset, offset + service_name.length()),
                       service_name_style);

  SetBorderAndLabel(std::move(label), font_list);
}

void EchoDialogView::InitForDisabledEcho() {
  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ECHO_DISABLED_CONSENT_DIALOG_TEXT));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // grab the font list before std::move(label) or it'll be nullptr
  gfx::FontList font_list = label->font_list();
  SetBorderAndLabel(std::move(label), font_list);
}

void EchoDialogView::SetBorderAndLabel(std::unique_ptr<views::View> label,
                                       const gfx::FontList& label_font_list) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Without a title, top padding isn't correctly calculated.  This adds the
  // text's internal leading to the top padding.  See
  // FontList::DeriveWithHeightUpperBound() for font padding details.
  int top_inset_padding =
      label_font_list.GetBaseline() - label_font_list.GetCapHeight();

  gfx::Insets insets =
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kText);
  insets += gfx::Insets::TLBR(top_inset_padding, 0, 0, 0);
  SetBorder(views::CreateEmptyBorder(insets));

  AddChildView(std::move(label));
}

BEGIN_METADATA(EchoDialogView)
END_METADATA

}  // namespace ash
