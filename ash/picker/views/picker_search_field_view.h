// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SEARCH_FIELD_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SEARCH_FIELD_VIEW_H_

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class Textfield;
}

namespace ash {

// View for the Picker search field.
class ASH_EXPORT PickerSearchFieldView : public views::View,
                                         public views::TextfieldController {
 public:
  METADATA_HEADER(PickerSearchFieldView);

  using SearchCallback =
      base::RepeatingCallback<void(const std::u16string& query)>;

  // `search_callback` is called synchronously whenever the contents of the
  // search field changes. It is also called synchronously with the empty string
  // when this view is constructed.
  explicit PickerSearchFieldView(SearchCallback search_callback);
  PickerSearchFieldView(const PickerSearchFieldView&) = delete;
  PickerSearchFieldView& operator=(const PickerSearchFieldView&) = delete;
  ~PickerSearchFieldView() override;

  // views::View:
  void RequestFocus() override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

  // Set the placeholder text to show when the textfield is empty.
  void SetPlaceholderText(base::StringPiece16 new_placeholder_text);

  const views::Textfield& textfield_for_testing() const { return *textfield_; }

 private:
  SearchCallback search_callback_;
  raw_ptr<views::Textfield> textfield_ = nullptr;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SEARCH_FIELD_VIEW_H_
