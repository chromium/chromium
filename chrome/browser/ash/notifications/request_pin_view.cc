// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/request_pin_view.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/notifications/passphrase_textfield.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/security_token_pin/error_generator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Default width of the text field.
constexpr int kDefaultTextWidth = 200;

}  // namespace

RequestPinView::RequestPinView(
    const std::string& extension_name,
    security_token_pin::CodeType code_type,
    int attempts_left,
    const PinEnteredCallback& pin_entered_callback,
    ViewDestructionCallback view_destruction_callback)
    : pin_entered_callback_(pin_entered_callback),
      view_destruction_callback_(std::move(view_destruction_callback)) {
  Init();
  SetExtensionName(extension_name);
  const bool accept_input = (attempts_left != 0);
  SetDialogParameters(code_type, security_token_pin::ErrorLabel::kNone,
                      attempts_left, accept_input);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::REQUEST_PIN);

  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
}

RequestPinView::~RequestPinView() {
  std::move(view_destruction_callback_).Run();
}

void RequestPinView::ContentsChanged(views::Textfield* sender,
                                     const std::u16string& new_contents) {
  DialogModelChanged();
}

bool RequestPinView::Accept() {
  if (!textfield_->GetEnabled())
    return true;
  DCHECK(!textfield_->GetText().empty());
  DCHECK(!locked_);

  error_label_->SetVisible(true);
  error_label_->SetText(
      l10n_util::GetStringUTF16(IDS_REQUEST_PIN_DIALOG_PROCESSING));
  error_label_->SetTooltipText(error_label_->GetText());
  error_label_->SetEnabledColor(SK_ColorGRAY);
  error_label_->SizeToPreferredSize();
  // The |textfield_| and OK button become disabled, but the user still can
  // close the dialog.
  SetAcceptInput(false);
  pin_entered_callback_.Run(base::UTF16ToUTF8(textfield_->GetText()));
  locked_ = true;
  DialogModelChanged();

  return false;
}

bool RequestPinView::IsDialogButtonEnabled(ui::DialogButton button) const {
  switch (button) {
    case ui::DialogButton::DIALOG_BUTTON_CANCEL:
      return true;
    case ui::DialogButton::DIALOG_BUTTON_OK:
      if (locked_)
        return false;
      // Not locked but the |textfield_| is not enabled. It's just a
      // notification to the user and [OK] button can be used to close the
      // dialog.
      if (!textfield_->GetEnabled())
        return true;
      return textfield_->GetText().size() > 0;
    case ui::DialogButton::DIALOG_BUTTON_NONE:
      return true;
  }

  NOTREACHED();
  return true;
}

views::View* RequestPinView::GetInitiallyFocusedView() {
  return textfield_;
}

std::u16string RequestPinView::GetWindowTitle() const {
  return window_title_;
}

void RequestPinView::SetDialogParameters(
    security_token_pin::CodeType code_type,
    security_token_pin::ErrorLabel error_label,
    int attempts_left,
    bool accept_input) {
  locked_ = false;
  SetErrorMessage(error_label, attempts_left, accept_input);
  SetAcceptInput(accept_input);

  switch (code_type) {
    case security_token_pin::CodeType::kPin:
      code_type_ = l10n_util::GetStringUTF16(IDS_REQUEST_PIN_DIALOG_PIN);
      break;
    case security_token_pin::CodeType::kPuk:
      code_type_ = l10n_util::GetStringUTF16(IDS_REQUEST_PIN_DIALOG_PUK);
      break;
  }

  UpdateHeaderText();
}

void RequestPinView::SetExtensionName(const std::string& extension_name) {
  window_title_ = base::ASCIIToUTF16(extension_name);
  UpdateHeaderText();
}

void RequestPinView::UpdateHeaderText() {
  int label_text_id = IDS_REQUEST_PIN_DIALOG_HEADER;
  std::u16string label_text =
      l10n_util::GetStringFUTF16(label_text_id, window_title_, code_type_);
  header_label_->SetText(label_text);
  header_label_->SizeToPreferredSize();
}

void RequestPinView::Init() {
  const views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetBorder(views::CreateEmptyBorder(
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT)));

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  int column_view_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_view_set_id);

  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  layout->StartRow(0, column_view_set_id);

  // Information label.
  int label_text_id = IDS_REQUEST_PIN_DIALOG_HEADER;
  std::u16string label_text = l10n_util::GetStringUTF16(label_text_id);
  auto header_label = std::make_unique<views::Label>(label_text);
  header_label->SetEnabled(true);
  header_label_ = layout->AddView(std::move(header_label));

  const int related_vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  layout->AddPaddingRow(0, related_vertical_spacing);

  column_view_set_id++;
  column_set = layout->AddColumnSet(column_view_set_id);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 100,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  // Textfield to enter the PIN/PUK.
  layout->StartRow(0, column_view_set_id);
  auto textfield = std::make_unique<PassphraseTextfield>();
  textfield->set_controller(this);
  textfield->SetEnabled(true);
  textfield->SetAssociatedLabel(header_label_);
  textfield_ =
      layout->AddView(std::move(textfield), 1, 1, views::GridLayout::LEADING,
                      views::GridLayout::FILL, kDefaultTextWidth, 0);

  layout->AddPaddingRow(0, related_vertical_spacing);

  column_view_set_id++;
  column_set = layout->AddColumnSet(column_view_set_id);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  // Error label.
  layout->StartRow(0, column_view_set_id);
  auto error_label = std::make_unique<views::Label>();
  error_label->SetVisible(false);
  error_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  error_label_ = layout->AddView(std::move(error_label));
}

void RequestPinView::SetAcceptInput(bool accept_input) {
  if (accept_input) {
    textfield_->SetEnabled(true);
    textfield_->SetBackgroundColor(SK_ColorWHITE);
    textfield_->RequestFocus();
  } else {
    textfield_->SetEnabled(false);
    textfield_->SetBackgroundColor(SK_ColorGRAY);
  }
}

void RequestPinView::SetErrorMessage(security_token_pin::ErrorLabel error_label,
                                     int attempts_left,
                                     bool accept_input) {
  if (error_label == security_token_pin::ErrorLabel::kNone &&
      attempts_left < 0) {
    error_label_->SetVisible(false);
    textfield_->SetInvalid(false);
    return;
  }

  std::u16string error_message = security_token_pin::GenerateErrorMessage(
      error_label, attempts_left, accept_input);

  error_label_->SetVisible(true);
  error_label_->SetText(error_message);
  error_label_->SetTooltipText(error_message);
  error_label_->SetEnabledColor(gfx::kGoogleRed600);
  error_label_->SizeToPreferredSize();
  textfield_->SetInvalid(true);
}

BEGIN_METADATA(RequestPinView, views::DialogDelegateView)
END_METADATA

}  // namespace ash
