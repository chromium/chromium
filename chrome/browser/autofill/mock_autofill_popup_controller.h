// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MOCK_AUTOFILL_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_MOCK_AUTOFILL_POPUP_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_layout_model.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

class MockAutofillPopupController
    : public AutofillPopupController,
      public base::SupportsWeakPtr<MockAutofillPopupController> {
 public:
  MockAutofillPopupController();
  ~MockAutofillPopupController();

  // AutofillPopupViewDelegate
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD0(ViewDestroyed, void());
  MOCK_METHOD1(SetSelectionAtPoint, void(const gfx::Point& point));
  MOCK_METHOD0(AcceptSelectedLine, bool());
  MOCK_METHOD0(SelectionCleared, void());
  MOCK_CONST_METHOD0(HasSelection, bool());
  MOCK_CONST_METHOD0(popup_bounds, gfx::Rect());
  MOCK_CONST_METHOD0(container_view, gfx::NativeView());
  const gfx::RectF& element_bounds() const override {
    static base::NoDestructor<gfx::RectF> bounds({100, 100, 250, 50});
    return *bounds;
  }
  MOCK_CONST_METHOD0(IsRTL, bool());
  const std::vector<autofill::Suggestion> GetSuggestions() override {
    return suggestions_;
  }
#if !defined(OS_ANDROID)
  MOCK_METHOD1(GetElidedValueWidthForRow, int(int row));
  MOCK_METHOD1(GetElidedLabelWidthForRow, int(int row));
#endif

  // AutofillPopupController
  MOCK_METHOD0(OnSuggestionsChanged, void());
  MOCK_METHOD1(AcceptSuggestion, void(int index));

  int GetLineCount() const override { return suggestions_.size(); }

  const autofill::Suggestion& GetSuggestionAt(int row) const override {
    return suggestions_[row];
  }

  const base::string16& GetElidedValueAt(int i) const override {
    return suggestions_[i].value;
  }

  const base::string16& GetElidedLabelAt(int row) const override {
    return suggestions_[row].label;
  }

  base::WeakPtr<MockAutofillPopupController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  MOCK_METHOD3(GetRemovalConfirmationText,
               bool(int index, base::string16* title, base::string16* body));
  MOCK_METHOD1(RemoveSuggestion, bool(int index));
  MOCK_METHOD1(SetSelectedLine, void(base::Optional<int> selected_line));
  MOCK_CONST_METHOD0(selected_line, base::Optional<int>());
  const autofill::AutofillPopupLayoutModel& layout_model() const override {
    return *layout_model_;
  }

  void set_suggestions(const std::vector<int>& ids) {
    for (const auto& id : ids)
      suggestions_.push_back(autofill::Suggestion("", "", "", id));
  }

  void set_suggestions(const std::vector<Suggestion>& suggestions) {
    suggestions_ = suggestions;
  }

 private:
  std::unique_ptr<autofill::AutofillPopupLayoutModel> layout_model_;
  std::vector<autofill::Suggestion> suggestions_;

  base::WeakPtrFactory<MockAutofillPopupController> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_MOCK_AUTOFILL_POPUP_CONTROLLER_H_
