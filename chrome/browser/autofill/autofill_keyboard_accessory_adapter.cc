// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_keyboard_accessory_adapter.h"

#include <numeric>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"

namespace autofill {

constexpr base::char16 kLabelSeparator = ' ';
constexpr size_t kMaxBulletCount = 8;

namespace {
base::string16 CreateLabel(const Suggestion& suggestion) {
  base::string16 password =
      suggestion.additional_label.substr(0, kMaxBulletCount);
  // The label contains the signon_realm or is empty. The additional_label can
  // never be empty since it must contain a password.
  if (suggestion.label.empty())
    return password;
  return suggestion.label + kLabelSeparator + password;
}

}  // namespace

AutofillKeyboardAccessoryAdapter::AutofillKeyboardAccessoryAdapter(
    base::WeakPtr<AutofillPopupController> controller)
    : controller_(controller) {}

AutofillKeyboardAccessoryAdapter::~AutofillKeyboardAccessoryAdapter() = default;

// AutofillPopupView implementation.

void AutofillKeyboardAccessoryAdapter::Show() {
  DCHECK(view_) << "Show called before a View was set!";
  OnSuggestionsChanged();
}

void AutofillKeyboardAccessoryAdapter::Hide() {
  DCHECK(view_) << "Hide called before a View was set!";
  view_->Hide();
}

void AutofillKeyboardAccessoryAdapter::OnSelectedRowChanged(
    base::Optional<int> previous_row_selection,
    base::Optional<int> current_row_selection) {}

void AutofillKeyboardAccessoryAdapter::OnSuggestionsChanged() {
  DCHECK(controller_) << "Call OnSuggestionsChanged only from its owner!";
  DCHECK(view_) << "OnSuggestionsChanged called before a View was set!";

  labels_.clear();
  front_element_ = base::nullopt;
  for (int i = 0; i < GetLineCount(); ++i) {
    const Suggestion& suggestion = controller_->GetSuggestionAt(i);
    if (suggestion.frontend_id != POPUP_ITEM_ID_CLEAR_FORM) {
      labels_.push_back(CreateLabel(suggestion));
      continue;
    }
    DCHECK(!front_element_.has_value()) << "Additional front item at: " << i;
    front_element_ = base::Optional<int>(i);
    // If there is a special popup item, just reuse the previously used label.
    labels_.push_back(controller_->GetSuggestionLabelAt(i));
  }

  view_->Show();
}

base::Optional<int32_t> AutofillKeyboardAccessoryAdapter::GetAxUniqueId() {
  NOTIMPLEMENTED() << "See https://crbug.com/985927";
  return base::nullopt;
}

// AutofillPopupController implementation.

void AutofillKeyboardAccessoryAdapter::AcceptSuggestion(int index) {
  if (controller_)
    controller_->AcceptSuggestion(OffsetIndexFor(index));
}

int AutofillKeyboardAccessoryAdapter::GetLineCount() const {
  return controller_ ? controller_->GetLineCount() : 0;
}

const autofill::Suggestion& AutofillKeyboardAccessoryAdapter::GetSuggestionAt(
    int row) const {
  DCHECK(controller_) << "Call GetSuggestionAt only from its owner!";
  return controller_->GetSuggestionAt(OffsetIndexFor(row));
}

const base::string16& AutofillKeyboardAccessoryAdapter::GetSuggestionValueAt(
    int row) const {
  DCHECK(controller_) << "Call GetSuggestionValueAt only from its owner!";
  return controller_->GetSuggestionValueAt(OffsetIndexFor(row));
}

const base::string16& AutofillKeyboardAccessoryAdapter::GetSuggestionLabelAt(
    int row) const {
  DCHECK(controller_) << "Call GetSuggestionLabelAt only from its owner!";
  DCHECK(static_cast<size_t>(row) < labels_.size());
  return labels_[OffsetIndexFor(row)];
}

PopupType AutofillKeyboardAccessoryAdapter::GetPopupType() const {
  DCHECK(controller_) << "Call GetPopupType only from its owner!";
  return controller_->GetPopupType();
}

bool AutofillKeyboardAccessoryAdapter::GetRemovalConfirmationText(
    int index,
    base::string16* title,
    base::string16* body) {
  return controller_ && controller_->GetRemovalConfirmationText(
                            OffsetIndexFor(index), title, body);
}

bool AutofillKeyboardAccessoryAdapter::RemoveSuggestion(int index) {
  DCHECK(view_) << "RemoveSuggestion called before a View was set!";
  base::string16 title, body;
  if (!GetRemovalConfirmationText(index, &title, &body))
    return false;

  view_->ConfirmDeletion(
      title, body,
      base::BindOnce(&AutofillKeyboardAccessoryAdapter::OnDeletionConfirmed,
                     weak_ptr_factory_.GetWeakPtr(), index));
  return true;
}

void AutofillKeyboardAccessoryAdapter::SetSelectedLine(
    base::Optional<int> selected_line) {
  if (!controller_)
    return;
  if (!selected_line.has_value()) {
    controller_->SetSelectedLine(base::nullopt);
    return;
  }
  controller_->SetSelectedLine(OffsetIndexFor(selected_line.value()));
}

base::Optional<int> AutofillKeyboardAccessoryAdapter::selected_line() const {
  if (!controller_ || !controller_->selected_line().has_value())
    return base::nullopt;
  for (int i = 0; i < GetLineCount(); ++i) {
    if (OffsetIndexFor(i) == controller_->selected_line().value()) {
      return i;
    }
  }
  return base::nullopt;
}

// AutofillPopupViewDelegate implementation

void AutofillKeyboardAccessoryAdapter::Hide(PopupHidingReason reason) {
  if (controller_)
    controller_->Hide(reason);
}

void AutofillKeyboardAccessoryAdapter::ViewDestroyed() {
  if (controller_)
    controller_->ViewDestroyed();

  view_.reset();

  // The controller has now deleted itself.
  controller_ = nullptr;
  delete this;  // Remove dangling weak reference.
}

void AutofillKeyboardAccessoryAdapter::SelectionCleared() {
  if (controller_)
    controller_->SelectionCleared();
}

gfx::NativeView AutofillKeyboardAccessoryAdapter::container_view() const {
  DCHECK(controller_) << "Call OnSuggestionsChanged only from its owner!";
  return controller_->container_view();
}

content::WebContents* AutofillKeyboardAccessoryAdapter::GetWebContents() const {
  return controller_->GetWebContents();
}

const gfx::RectF& AutofillKeyboardAccessoryAdapter::element_bounds() const {
  DCHECK(controller_) << "Call OnSuggestionsChanged only from its owner!";
  return controller_->element_bounds();
}

bool AutofillKeyboardAccessoryAdapter::IsRTL() const {
  return controller_ && controller_->IsRTL();
}

std::vector<Suggestion> AutofillKeyboardAccessoryAdapter::GetSuggestions()
    const {
  if (!controller_)
    return std::vector<Suggestion>();
  std::vector<Suggestion> suggestions = controller_->GetSuggestions();
  if (front_element_.has_value()) {
    std::rotate(suggestions.begin(),
                suggestions.begin() + front_element_.value(),
                suggestions.begin() + front_element_.value() + 1);
  }
  return suggestions;
}

void AutofillKeyboardAccessoryAdapter::OnDeletionConfirmed(int index) {
  if (controller_)
    controller_->RemoveSuggestion(OffsetIndexFor(index));
}

int AutofillKeyboardAccessoryAdapter::OffsetIndexFor(int element_index) const {
  if (!front_element_.has_value())
    return element_index;
  if (0 == element_index)
    return front_element_.value();
  return element_index - (element_index <= front_element_.value() ? 1 : 0);
}

}  // namespace autofill
