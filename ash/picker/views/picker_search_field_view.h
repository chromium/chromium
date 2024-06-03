// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SEARCH_FIELD_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SEARCH_FIELD_VIEW_H_

#include <string_view>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace views {
class Textfield;
class ImageButton;
}

namespace ash {

class PickerKeyEventHandler;
class PickerPerformanceMetrics;

// View for the Picker search field.
class ASH_EXPORT PickerSearchFieldView : public views::FlexLayoutView,
                                         public views::TextfieldController,
                                         public views::FocusChangeListener {
  METADATA_HEADER(PickerSearchFieldView, views::FlexLayoutView)

 public:
  using SearchCallback =
      base::RepeatingCallback<void(const std::u16string& query)>;
  using BackCallback = base::RepeatingClosure;

  // `search_callback` is called asynchronously whenever the contents of the
  // search field changes (with debouncing logic to avoid unnecessary calls).
  // `key_event_handler` and `performance_metrics` must live as long as this
  // class. `delay` is the time to wait before calling `search_callback` for
  // debouncing.
  //
  // `back_callback` is called when clicking on the back button.
  explicit PickerSearchFieldView(SearchCallback search_callback,
                                 BackCallback back_callback,
                                 PickerKeyEventHandler* key_event_handler,
                                 PickerPerformanceMetrics* performance_metrics);
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
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(View* focused_before, View* focused_now) override;
  void OnDidChangeFocus(View* focused_before, View* focused_now) override;

  // Gets or sets the placeholder text to show when the textfield is empty.
  const std::u16string& GetPlaceholderText() const;
  void SetPlaceholderText(const std::u16string& new_placeholder_text);

  // Sets the active descendant of the underlying textfield to `view` for screen
  // readers. `view` may be null, in which case the active descendant is
  // cleared.
  void SetTextfieldActiveDescendant(views::View* view);

  // Gets or sets the current search query text.
  std::u16string_view GetQueryText() const;
  void SetQueryText(std::u16string text);

  // Sets whether the back button is visible.
  void SetBackButtonVisible(bool visible);

  const views::Textfield& textfield_for_testing() const { return *textfield_; }
  views::ImageButton& back_button_for_testing() { return *back_button_; }
  views::ImageButton& clear_button_for_testing() { return *clear_button_; }

 private:
  void ClearButtonPressed();

  // Updates the textfield border when the clear button visibility changes.
  void UpdateTextfieldBorder();

  SearchCallback search_callback_;
  raw_ptr<PickerKeyEventHandler> key_event_handler_ = nullptr;
  raw_ptr<PickerPerformanceMetrics> performance_metrics_ = nullptr;
  raw_ptr<views::Textfield> textfield_ = nullptr;
  raw_ptr<views::ImageButton> back_button_ = nullptr;
  raw_ptr<views::ImageButton> clear_button_ = nullptr;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, PickerSearchFieldView, views::FlexLayoutView)
VIEW_BUILDER_PROPERTY(std::u16string, PlaceholderText)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::PickerSearchFieldView)

#endif  // ASH_PICKER_VIEWS_PICKER_SEARCH_FIELD_VIEW_H_
