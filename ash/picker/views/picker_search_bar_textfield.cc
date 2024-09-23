// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_bar_textfield.h"

#include "ash/picker/views/picker_search_field_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

PickerSearchBarTextfield::PickerSearchBarTextfield(
    PickerSearchFieldView* search_field_view)
    : search_field_view_(search_field_view) {}

PickerSearchBarTextfield::~PickerSearchBarTextfield() = default;

void PickerSearchBarTextfield::SetShouldShowFocusIndicator(
    bool should_show_focus_indicator) {
  search_field_view_->SetShouldShowFocusIndicator(should_show_focus_indicator);
}

BEGIN_METADATA(PickerSearchBarTextfield)
END_METADATA

}  // namespace ash
