// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_field_view.h"

#include <string>

#include "ash/style/typography.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view_class_properties.h"

namespace ash {

PickerSearchFieldView::PickerSearchFieldView(SearchCallback search_callback)
    : search_callback_(std::move(search_callback)) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  textfield_ = AddChildView(std::make_unique<views::Textfield>());
  textfield_->set_controller(this);
  textfield_->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
      TypographyToken::kCrosBody2));
  // TODO(b/309706053): Replace this once the strings are finalized.
  textfield_->SetAccessibleName(u"placeholder");

  search_callback_.Run(u"");
}

PickerSearchFieldView::~PickerSearchFieldView() = default;

void PickerSearchFieldView::RequestFocus() {
  textfield_->RequestFocus();
}

void PickerSearchFieldView::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  search_callback_.Run(new_contents);
}

void PickerSearchFieldView::SetPlaceholderText(
    base::StringPiece16 new_placeholder_text) {
  textfield_->SetPlaceholderText(std::u16string(new_placeholder_text));
}

BEGIN_METADATA(PickerSearchFieldView, views::View)
END_METADATA

}  // namespace ash
