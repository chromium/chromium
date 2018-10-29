// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/echo_dialog_view.h"

#include <stddef.h>

#include "chrome/browser/chromeos/ui/echo_dialog_listener.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace chromeos {

EchoDialogView::EchoDialogView(EchoDialogListener* listener)
    : listener_(listener),
      learn_more_button_(nullptr),
      ok_button_label_id_(0),
      cancel_button_label_id_(0) {
  chrome::RecordDialogCreation(chrome::DialogIdentifier::ECHO);
}

EchoDialogView::~EchoDialogView() = default;

void EchoDialogView::InitForEnabledEcho(const base::string16& service_name,
                                        const base::string16& origin) {
  ok_button_label_id_ = IDS_OFFERS_CONSENT_INFOBAR_ENABLE_BUTTON;
  cancel_button_label_id_ = IDS_OFFERS_CONSENT_INFOBAR_DISABLE_BUTTON;

  size_t offset;
  base::string16 text = l10n_util::GetStringFUTF16(IDS_ECHO_CONSENT_DIALOG_TEXT,
                                                   service_name, &offset);

  views::StyledLabel* label = new views::StyledLabel(text, nullptr);

  views::StyledLabel::RangeStyleInfo service_name_style;
  gfx::FontList font_list = label->GetDefaultFontList();
  service_name_style.custom_font =
      font_list.DeriveWithStyle(gfx::Font::UNDERLINE);
  service_name_style.tooltip = origin;
  label->AddStyleRange(gfx::Range(offset, offset + service_name.length()),
                       service_name_style);

  SetBorderAndLabel(label, font_list);
}

void EchoDialogView::InitForDisabledEcho() {
  ok_button_label_id_ = 0;
  cancel_button_label_id_ = IDS_ECHO_CONSENT_DISMISS_BUTTON;

  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_ECHO_DISABLED_CONSENT_DIALOG_TEXT));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  SetBorderAndLabel(label, label->font_list());
}

void EchoDialogView::Show(gfx::NativeWindow parent) {
  DCHECK(cancel_button_label_id_);

  views::DialogDelegate::CreateDialogWidget(this, parent, parent);
  GetWidget()->SetSize(GetWidget()->GetRootView()->GetPreferredSize());
  GetWidget()->Show();
}

views::View* EchoDialogView::CreateExtraView() {
  learn_more_button_ = views::CreateVectorImageButton(this);
  views::SetImageFromVectorIcon(learn_more_button_,
                                vector_icons::kHelpOutlineIcon);
  learn_more_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_CHROMEOS_ACC_LEARN_MORE));
  learn_more_button_->SetFocusForPlatform();
  return learn_more_button_;
}

int EchoDialogView::GetDialogButtons() const {
  int buttons = ui::DIALOG_BUTTON_NONE;
  if (ok_button_label_id_)
    buttons |= ui::DIALOG_BUTTON_OK;
  if (cancel_button_label_id_)
    buttons |= ui::DIALOG_BUTTON_CANCEL;
  return buttons;
}

base::string16 EchoDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(ok_button_label_id_);
  if (button == ui::DIALOG_BUTTON_CANCEL)
    return l10n_util::GetStringUTF16(cancel_button_label_id_);
  return base::string16();
}

bool EchoDialogView::Cancel() {
  if (listener_) {
    listener_->OnCancel();
    listener_ = nullptr;
  }
  return true;
}

bool EchoDialogView::Accept() {
  if (listener_) {
    listener_->OnAccept();
    listener_ = nullptr;
  }
  return true;
}

ui::ModalType EchoDialogView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

bool EchoDialogView::ShouldShowWindowTitle() const {
  return false;
}

bool EchoDialogView::ShouldShowCloseButton() const {
  return false;
}

void EchoDialogView::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  if (!listener_ || sender != learn_more_button_)
    return;
  listener_->OnMoreInfoLinkClicked();
}

gfx::Size EchoDialogView::CalculatePreferredSize() const {
  const int default_width = views::LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  return gfx::Size(
      default_width,
      GetLayoutManager()->GetPreferredHeightForWidth(this, default_width));
}

void EchoDialogView::SetBorderAndLabel(views::View* label,
                                       const gfx::FontList& label_font_list) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Without a title, top padding isn't correctly calculated.  This adds the
  // text's internal leading to the top padding.  See
  // FontList::DeriveWithHeightUpperBound() for font padding details.
  int top_inset_padding =
      label_font_list.GetBaseline() - label_font_list.GetCapHeight();

  gfx::Insets insets =
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(views::TEXT,
                                                                  views::TEXT);
  insets += gfx::Insets(top_inset_padding, 0, 0, 0);
  SetBorder(views::CreateEmptyBorder(insets));

  AddChildView(label);
}

}  // namespace chromeos
