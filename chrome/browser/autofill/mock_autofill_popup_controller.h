// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MOCK_AUTOFILL_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_MOCK_AUTOFILL_POPUP_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/test/scoped_default_font_description.h"

namespace autofill {

class MockAutofillPopupController
    : public AutofillPopupController,
      public base::SupportsWeakPtr<MockAutofillPopupController> {
 public:
  MockAutofillPopupController();
  ~MockAutofillPopupController() override;

  // AutofillPopupViewDelegate:
  MOCK_METHOD(void, Hide, (PopupHidingReason), (override));
  MOCK_METHOD(void, ViewDestroyed, (), (override));
  MOCK_METHOD(bool, HasSelection, (), (const override));
  MOCK_METHOD(gfx::Rect, popup_bounds, (), (const override));
  MOCK_METHOD(AutofillSuggestionTriggerSource,
              GetAutofillSuggestionTriggerSource,
              (),
              (const override));
  MOCK_METHOD(bool,
              ShouldIgnoreMouseObservedOutsideItemBoundsCheck,
              (),
              (const override));
  MOCK_METHOD(gfx::NativeView, container_view, (), (const override));
  MOCK_METHOD(content::WebContents*, GetWebContents, (), (const override));
  const gfx::RectF& element_bounds() const override { return element_bounds_; }
  void set_element_bounds(const gfx::RectF& bounds) {
    element_bounds_ = bounds;
  }
  MOCK_METHOD(base::i18n::TextDirection,
              GetElementTextDirection,
              (),
              (const override));

  // AutofillPopupController:
  MOCK_METHOD(void, OnSuggestionsChanged, (), (override));
  MOCK_METHOD(void, AcceptSuggestion, (int, base::TimeTicks), (override));
  MOCK_METHOD(void, PerformButtonActionForSuggestion, (int), (override));
  MOCK_METHOD(std::optional<AutofillClient::PopupScreenLocation>,
              GetPopupScreenLocation,
              (),
              (const override));
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

  MOCK_METHOD(bool,
              GetRemovalConfirmationText,
              (int, std::u16string*, std::u16string*),
              (override));
  MOCK_METHOD(bool, RemoveSuggestion, (int), (override));
  MOCK_METHOD(void, SelectSuggestion, (absl::optional<size_t>), (override));
  MOCK_METHOD(PopupType, GetPopupType, (), (const override));
  MOCK_METHOD(base::WeakPtr<AutofillPopupController>,
              OpenSubPopup,
              (const gfx::RectF& anchor_bounds,
               std::vector<Suggestion> suggestions,
               AutoselectFirstSuggestion autoselect_first_suggestion),
              (override));
  MOCK_METHOD(void, HideSubPopup, (), (override));
  void set_suggestions(const std::vector<PopupItemId>& ids) {
    suggestions_.clear();

    for (const auto& id : ids) {
      // Accessibility requires all focusable AutofillPopupItemView to have
      // ui::AXNodeData with non-empty names. We specify dummy values and labels
      // to satisfy this.
      suggestions_.emplace_back("dummy_value", "dummy_label", "", id);
    }
  }

  void set_suggestions(std::vector<Suggestion> suggestions) {
    suggestions_ = std::move(suggestions);
  }

  void InvalidateWeakPtrs() { weak_ptr_factory_.InvalidateWeakPtrs(); }

 private:
  std::vector<autofill::Suggestion> suggestions_;
  gfx::ScopedDefaultFontDescription default_font_desc_setter_;
  gfx::RectF element_bounds_ = {100, 100, 250, 50};

  base::WeakPtrFactory<MockAutofillPopupController> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_MOCK_AUTOFILL_POPUP_CONTROLLER_H_
