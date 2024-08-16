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
#include "base/timer/timer.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"

namespace views {
class Textfield;
class ImageButton;
}

namespace ash {

class PickerKeyEventHandler;
class PickerPerformanceMetrics;
class PickerSearchBarTextfield;

// View for the Picker search field.
class ASH_EXPORT PickerSearchFieldView : public views::BoxLayoutView,
                                         public views::TextfieldController,
                                         public views::FocusChangeListener {
  METADATA_HEADER(PickerSearchFieldView, views::BoxLayoutView)

 public:
  using SearchCallback =
      base::RepeatingCallback<void(const std::u16string& query)>;
  using BackCallback = base::RepeatingClosure;

  // The delay before notifying the initial active descendant when the textfield
  // is focused. Same value as Launcher.
  static constexpr base::TimeDelta kNotifyInitialActiveDescendantA11yDelay =
      base::Milliseconds(1500);

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
  void OnPaint(gfx::Canvas* canvas) override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(View* focused_before, View* focused_now) override;
  void OnDidChangeFocus(View* focused_before, View* focused_now) override;

  // Should be called every time the contents of the text field changes, even
  // if the search callback should not be called.
  void ContentsChangedInternal(std::u16string_view new_contents);

  // Gets or sets the placeholder text to show when the textfield is empty.
  const std::u16string& GetPlaceholderText() const;
  void SetPlaceholderText(const std::u16string& new_placeholder_text);

  // Sets the active descendant of the underlying textfield to `view` for screen
  // readers. `view` may be null, in which case the active descendant is
  // cleared.
  void SetTextfieldActiveDescendant(views::View* view);

  // Gets the current search query text.
  std::u16string_view GetQueryText() const;
  // Sets the current search query text. Does not call the search callback.
  void SetQueryText(std::u16string text);

  // Sets whether the back button is visible.
  void SetBackButtonVisible(bool visible);

  void SetShouldShowFocusIndicator(bool should_show_focus_indicator);

  // Returns the view directly to the left / right of `view`, or nullptr if
  // there is no such view in the PickerSearchFieldView.
  views::View* GetViewLeftOf(views::View* view);
  views::View* GetViewRightOf(views::View* view);

  // Returns true if a left / right key event should move the cursor rather than
  // moving the currently pseudo focused view.
  bool LeftEventShouldMoveCursor(views::View* pseudo_focused_view);
  bool RightEventShouldMoveCursor(views::View* pseudo_focused_view);

  // Should be called when the search field or one of its child views gains
  // pseudo focus after a left / right key event.
  void OnGainedPseudoFocusFromLeftEvent(views::View* pseudo_focused_view);
  void OnGainedPseudoFocusFromRightEvent(views::View* pseudo_focused_view);

  PickerSearchBarTextfield* textfield() { return textfield_; }

  PickerSearchBarTextfield& textfield_for_testing() { return *textfield_; }
  views::ImageButton& back_button_for_testing() { return *back_button_; }
  views::ImageButton& clear_button_for_testing() { return *clear_button_; }

 private:
  void ClearButtonPressed();

  // Updates the textfield border when the clear button visibility changes.
  void UpdateTextfieldBorder();
  // Schedules a delayed announcement of the initial active descendant.
  void ScheduleNotifyInitialActiveDescendantForA11y();
  // Notifies the initial active descendant for the screen reader.
  void NotifyInitialActiveDescendantForA11y();

  // Gets the start and end indices of the current search query text, to use
  // when moving pseudo focus to and from the textfield. Note that the start and
  // end are swapped in RTL locales since we swapped left and right key events
  // when traversing the Picker UI in RTL.
  size_t GetQueryStartIndexForTraversal();
  size_t GetQueryEndIndexForTraversal();

  bool should_show_focus_indicator_ = false;

  SearchCallback search_callback_;
  raw_ptr<PickerKeyEventHandler> key_event_handler_ = nullptr;
  raw_ptr<PickerPerformanceMetrics> performance_metrics_ = nullptr;
  raw_ptr<PickerSearchBarTextfield> textfield_ = nullptr;
  raw_ptr<views::ImageButton> back_button_ = nullptr;
  raw_ptr<views::ImageButton> clear_button_ = nullptr;

  // Tracks pending active descendant change when the textfield is not focused.
  views::ViewTracker active_descendant_tracker_;
  // Delay the initial active descendant change notification for a query.
  base::OneShotTimer notify_initial_active_descendant_timer_;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, PickerSearchFieldView, views::BoxLayoutView)
VIEW_BUILDER_PROPERTY(std::u16string, PlaceholderText)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::PickerSearchFieldView)

#endif  // ASH_PICKER_VIEWS_PICKER_SEARCH_FIELD_VIEW_H_
