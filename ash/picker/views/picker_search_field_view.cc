// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_field_view.h"

#include <string>
#include <string_view>

#include "ash/ash_element_identifiers.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr auto kSearchFieldBorderInsets = gfx::Insets::VH(0, 16);
constexpr auto kSearchFieldVerticalPadding = gfx::Insets::VH(6, 0);

}  // namespace

PickerSearchFieldView::PickerSearchFieldView(
    SearchCallback search_callback,
    PickerSessionMetrics* session_metrics)
    : search_callback_(std::move(search_callback)),
      session_metrics_(session_metrics) {
  views::Builder<PickerSearchFieldView>(this)
      .SetUseDefaultFillLayout(true)
      .SetProperty(views::kMarginsKey, kSearchFieldVerticalPadding)
      .AddChild(
          views::Builder<views::Textfield>()
              .CopyAddressTo(&textfield_)
              .SetProperty(views::kElementIdentifierKey,
                           kPickerSearchFieldTextfieldElementId)
              .SetController(this)
              .SetBorder(views::CreateEmptyBorder(kSearchFieldBorderInsets))
              .SetBackgroundColor(SK_ColorTRANSPARENT)
              .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
                  TypographyToken::kCrosBody2))
              .SetPlaceholderText(l10n_util::GetStringUTF16(
                  IDS_PICKER_ZERO_STATE_SEARCH_FIELD_PLACEHOLDER_TEXT))
              // TODO(b/309706053): Replace this once the strings are finalized.
              .SetAccessibleName(u"placeholder"))
      .BuildChildren();
}

PickerSearchFieldView::~PickerSearchFieldView() = default;

void PickerSearchFieldView::RequestFocus() {
  textfield_->RequestFocus();
}

void PickerSearchFieldView::AddedToWidget() {
  GetFocusManager()->AddFocusChangeListener(this);
}

void PickerSearchFieldView::RemovedFromWidget() {
  GetFocusManager()->RemoveFocusChangeListener(this);
}

void PickerSearchFieldView::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  session_metrics_->MarkContentsChanged();

  search_callback_.Run(new_contents);
}

void PickerSearchFieldView::OnWillChangeFocus(View* focused_before,
                                              View* focused_now) {}

void PickerSearchFieldView::OnDidChangeFocus(View* focused_before,
                                             View* focused_now) {
  if (focused_now == textfield_) {
    session_metrics_->MarkInputFocus();
  }
}

void PickerSearchFieldView::SetPlaceholderText(
    std::u16string_view new_placeholder_text) {
  textfield_->SetPlaceholderText(std::u16string(new_placeholder_text));
}

BEGIN_METADATA(PickerSearchFieldView)
END_METADATA

}  // namespace ash
