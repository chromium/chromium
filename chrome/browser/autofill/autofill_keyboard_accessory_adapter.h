// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_ADAPTER_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_ADAPTER_H_

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"

namespace autofill {

// This adapter allows the AutofillPopupController to treat the keyboard
// accessory like any other implementation of AutofillPopupView.
// From the controller's perspective, this behaves like a real AutofillPopupView
// and for the view, it behaves like the real AutofillPopupController.
class AutofillKeyboardAccessoryAdapter : public AutofillPopupView,
                                         public AutofillPopupController {
 public:
  AutofillKeyboardAccessoryAdapter(
      base::WeakPtr<AutofillPopupController> controller,
      unsigned int animation_duration_millis,
      bool should_limit_label_width);
  ~AutofillKeyboardAccessoryAdapter() override;

  // Interface describing the minimal capabilities for the native view.
  class AccessoryView {
   public:
    virtual ~AccessoryView() = default;

    // Initializes the Java-side of this bridge.
    virtual void Initialize(unsigned int animation_duration_millis,
                            bool should_limit_label_width) = 0;

    // Requests to dismiss this view.
    virtual void Hide() = 0;

    // Requests to show this view with the data provided by the controller.
    virtual void Show() = 0;

    // Ask to confirm a deletion. Triggers the callback upon confirmation.
    virtual void ConfirmDeletion(const base::string16& confirmation_title,
                                 const base::string16& confirmation_body,
                                 base::OnceClosure confirm_deletion) = 0;
  };

  void SetAccessoryView(std::unique_ptr<AccessoryView> view) {
    view_ = std::move(view);
  }

 private:
  // AutofillPopupView implementation.
  void Show() override;
  void Hide() override;
  void OnSelectedRowChanged(base::Optional<int> previous_row_selection,
                            base::Optional<int> current_row_selection) override;
  void OnSuggestionsChanged() override;
  base::Optional<int32_t> GetAxUniqueId() override;

  // AutofillPopupController implementation.
  // Hidden: void OnSuggestionsChanged() override;
  void AcceptSuggestion(int index) override;
  int GetLineCount() const override;
  const autofill::Suggestion& GetSuggestionAt(int row) const override;
  const base::string16& GetElidedValueAt(int row) const override;
  const base::string16& GetElidedLabelAt(int row) const override;
  bool GetRemovalConfirmationText(int index,
                                  base::string16* title,
                                  base::string16* body) override;
  bool RemoveSuggestion(int index) override;
  void SetSelectedLine(base::Optional<int> selected_line) override;
  base::Optional<int> selected_line() const override;
  const AutofillPopupLayoutModel& layout_model() const override;

  // AutofillPopupViewDelegate implementation
  // Hidden: void Hide() override;
  void ViewDestroyed() override;
  void SetSelectionAtPoint(const gfx::Point& point) override;
  bool AcceptSelectedLine() override;
  void SelectionCleared() override;
  bool HasSelection() const override;
  gfx::Rect popup_bounds() const override;
  gfx::NativeView container_view() const override;
  const gfx::RectF& element_bounds() const override;
  bool IsRTL() const override;
  const std::vector<autofill::Suggestion> GetSuggestions() override;

  void OnDeletionConfirmed(int index);

  // Indices might be offset because a special item is moved to the front. This
  // method returns the index used by the keyboard accessory (may be offset).
  // |element_index| is the position of an element as returned by |controller_|.
  int OffsetIndexFor(int element_index) const;

  base::WeakPtr<AutofillPopupController> controller_;
  std::unique_ptr<AutofillKeyboardAccessoryAdapter::AccessoryView> view_;

  // The labels to be used for the input chips.
  std::vector<base::string16> labels_;

  // If 0, don't animate suggestion view.
  const unsigned int animation_duration_millis_;

  // If true, limits label width to 1/2 device's width.
  const bool should_limit_label_width_;

  // Position that the front element has in the suggestion list returned by
  // controller_. It is used to determine the offset suggestions.
  base::Optional<int> front_element_;

  base::WeakPtrFactory<AutofillKeyboardAccessoryAdapter> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(AutofillKeyboardAccessoryAdapter);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_ADAPTER_H_
