// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SEARCH_FIELD_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SEARCH_FIELD_VIEW_H_

#include <string_view>

#include "ash/ash_export.h"
#include "ash/picker/metrics/picker_session_metrics.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace views {
class Textfield;
}

namespace ash {

// View for the Picker search field.
class ASH_EXPORT PickerSearchFieldView : public views::View,
                                         public views::TextfieldController,
                                         public views::FocusChangeListener {
  METADATA_HEADER(PickerSearchFieldView, views::View)

 public:
  using SearchCallback =
      base::RepeatingCallback<void(const std::u16string& query)>;

  // `search_callback` is called asynchronously whenever the contents of the
  // search field changes (with debouncing logic to avoid unnecessary calls).
  // `session_metrics` must live as long as this class.
  // `delay` is the time to wait before calling `search_callback` for
  // debouncing.
  explicit PickerSearchFieldView(SearchCallback search_callback,
                                 PickerSessionMetrics* session_metrics);
  PickerSearchFieldView(const PickerSearchFieldView&) = delete;
  PickerSearchFieldView& operator=(const PickerSearchFieldView&) = delete;
  ~PickerSearchFieldView() override;

  // views::View:
  void RequestFocus() override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(View* focused_before, View* focused_now) override;
  void OnDidChangeFocus(View* focused_before, View* focused_now) override;

  // Set the placeholder text to show when the textfield is empty.
  void SetPlaceholderText(std::u16string_view new_placeholder_text);

  const views::Textfield& textfield_for_testing() const { return *textfield_; }

 private:
  SearchCallback search_callback_;
  raw_ptr<PickerSessionMetrics> session_metrics_ = nullptr;
  raw_ptr<views::Textfield> textfield_ = nullptr;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, PickerSearchFieldView, views::View)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::PickerSearchFieldView)

#endif  // ASH_PICKER_VIEWS_PICKER_SEARCH_FIELD_VIEW_H_
