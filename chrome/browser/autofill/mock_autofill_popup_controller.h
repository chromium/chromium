// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MOCK_AUTOFILL_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_MOCK_AUTOFILL_POPUP_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/test/scoped_default_font_description.h"

namespace autofill {

class MockAutofillPopupController
    : public AutofillPopupController,
      public base::SupportsWeakPtr<MockAutofillPopupController> {
 public:
  MockAutofillPopupController();
  ~MockAutofillPopupController() override;

  // AutofillPopupViewDelegate
  MOCK_METHOD1(Hide, void(PopupHidingReason reason));
  MOCK_METHOD0(ViewDestroyed, void());
  MOCK_METHOD1(SetSelectionAtPoint, void(const gfx::Point& point));
  MOCK_METHOD0(AcceptSelectedLine, bool());
  MOCK_METHOD0(SelectionCleared, void());
  MOCK_CONST_METHOD0(HasSelection, bool());
  MOCK_CONST_METHOD0(popup_bounds, gfx::Rect());
  MOCK_CONST_METHOD0(container_view, gfx::NativeView());
  MOCK_CONST_METHOD0(GetWebContents, content::WebContents*());
  const gfx::RectF& element_bounds() const override {
    static const gfx::RectF bounds(100, 100, 250, 50);
    return bounds;
  }
  MOCK_CONST_METHOD0(IsRTL, bool());

  // AutofillPopupController
  MOCK_METHOD0(OnSuggestionsChanged, void());
  MOCK_METHOD1(AcceptSuggestion, void(int index));
  std::vector<Suggestion> GetSuggestions() const override {
    return suggestions_;
  }

  int GetLineCount() const override { return suggestions_.size(); }

  const autofill::Suggestion& GetSuggestionAt(int row) const override {
    return suggestions_[row];
  }

  std::u16string GetSuggestionMainTextAt(int row) const override {
    return suggestions_[row].main_text.value;
  }

  std::u16string GetSuggestionMinorTextAt(int row) const override {
    return std::u16string();
  }

  std::vector<std::vector<Suggestion::Text>> GetSuggestionLabelsAt(
      int row) const override {
    return suggestions_[row].labels;
  }

  base::WeakPtr<MockAutofillPopupController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  MOCK_METHOD3(GetRemovalConfirmationText,
               bool(int index, std::u16string* title, std::u16string* body));
  MOCK_METHOD1(RemoveSuggestion, bool(int index));
  MOCK_METHOD1(SetSelectedLine, void(absl::optional<int> selected_line));
  MOCK_CONST_METHOD0(selected_line, absl::optional<int>());
  MOCK_CONST_METHOD0(GetPopupType, PopupType());

  void set_suggestions(const std::vector<int>& ids) {
    for (const auto& id : ids) {
      // Accessibility requires all focusable AutofillPopupItemView to have
      // ui::AXNodeData with non-empty names. We specify dummy values and labels
      // to satisfy this.
      suggestions_.emplace_back("dummy_value", "dummy_label", "", id);
    }
  }

  void set_suggestions(const std::vector<Suggestion>& suggestions) {
    suggestions_ = suggestions;
  }

 private:
  std::vector<autofill::Suggestion> suggestions_;
  gfx::ScopedDefaultFontDescription default_font_desc_setter_;

  base::WeakPtrFactory<MockAutofillPopupController> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_MOCK_AUTOFILL_POPUP_CONTROLLER_H_
