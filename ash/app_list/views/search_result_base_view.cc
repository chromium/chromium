// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_base_view.h"

#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/search_result_actions_view.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace ash {

SearchResultBaseView::SearchResultBaseView() : Button(this) {
  SetInstallFocusRingOnFocus(false);
}

SearchResultBaseView::~SearchResultBaseView() = default;

bool SearchResultBaseView::SkipDefaultKeyEventProcessing(
    const ui::KeyEvent& event) {
  // Ensure accelerators take priority in the app list. This ensures, e.g., that
  // Ctrl+Space will switch input methods rather than activate the button.
  return false;
}

const char* SearchResultBaseView::GetClassName() const {
  return "SearchResultBaseView";
}

void SearchResultBaseView::SetSelected(bool selected,
                                       base::Optional<bool> reverse_tab_order) {
  if (selected_ == selected)
    return;

  selected_ = selected;

  if (app_list_features::IsSearchBoxSelectionEnabled()) {
    if (selected) {
      SelectInitialResultAction(reverse_tab_order.value_or(false));
    } else {
      ClearSelectedResultAction();
    }
  }

  SchedulePaint();
}

bool SearchResultBaseView::SelectNextResultAction(bool reverse_tab_order) {
  DCHECK(app_list_features::IsSearchBoxSelectionEnabled());

  if (!selected() || !actions_view_)
    return false;

  if (!actions_view_->SelectNextAction(reverse_tab_order))
    return false;

  SchedulePaint();
  return true;
}

void SearchResultBaseView::SetResult(SearchResult* result) {
  OnResultChanging(result);
  ClearResult();
  result_ = result;
  if (result_)
    result_->AddObserver(this);
  OnResultChanged();
}

void SearchResultBaseView::OnResultDestroying() {
  // Uses |SetResult| to ensure that the |OnResultChanging()| and
  // |OnResultChanged()| logic gets run.
  SetResult(nullptr);
}

base::string16 SearchResultBaseView::ComputeAccessibleName() const {
  if (!result())
    return base::string16();

  base::string16 accessible_name = result()->title();
  if (!result()->title().empty() && !result()->details().empty())
    accessible_name += base::ASCIIToUTF16(", ");
  accessible_name += result()->details();

  return accessible_name;
}

void SearchResultBaseView::UpdateAccessibleName() {
  SetAccessibleName(ComputeAccessibleName());
}

void SearchResultBaseView::ClearResult() {
  if (result_)
    result_->RemoveObserver(this);
  result_ = nullptr;
}

void SearchResultBaseView::SelectInitialResultAction(bool reverse_tab_order) {
  DCHECK(app_list_features::IsSearchBoxSelectionEnabled());

  if (actions_view_ && actions_view_->SelectInitialAction(reverse_tab_order))
    return;

  NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
}

void SearchResultBaseView::ClearSelectedResultAction() {
  DCHECK(app_list_features::IsSearchBoxSelectionEnabled());

  if (actions_view_)
    actions_view_->ClearSelectedAction();
}

}  // namespace ash
