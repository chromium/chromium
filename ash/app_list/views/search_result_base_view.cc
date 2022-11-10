// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_base_view.h"

#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/search_result_actions_view.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

SearchResultBaseView::SearchResultBaseView() {
  SetGroup(kSearchResultViewGroup);
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetInstallFocusRingOnFocus(false);
}

SearchResultBaseView::~SearchResultBaseView() {
  if (result_)
    result_->RemoveObserver(this);
  result_ = nullptr;
}

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
                                       absl::optional<bool> reverse_tab_order) {
  if (selected_ == selected)
    return;

  selected_ = selected;

  if (selected) {
    SelectInitialResultAction(reverse_tab_order.value_or(false));
  } else {
    ClearSelectedResultAction();
  }

  SchedulePaint();
}

bool SearchResultBaseView::SelectNextResultAction(bool reverse_tab_order) {
  if (!selected() || !actions_view_)
    return false;

  if (!actions_view_->SelectNextAction(reverse_tab_order))
    return false;

  SchedulePaint();
  return true;
}

views::View* SearchResultBaseView::GetSelectedView() {
  if (actions_view_ && actions_view_->HasSelectedAction())
    return actions_view_->GetSelectedView();
  return this;
}

void SearchResultBaseView::SetResult(SearchResult* result) {
  ClearResult();
  result_ = result;
  if (result_)
    result_->AddObserver(this);
  OnResultChanged();
}

void SearchResultBaseView::OnResultDestroying() {
  ClearResult();
}

std::u16string SearchResultBaseView::ComputeAccessibleName() const {
  if (!result())
    return u"";

  std::u16string accessible_name;
  if (!result()->accessible_name().empty())
    return result()->accessible_name();

  std::u16string title = result()->title();
  if (result()->result_type() == AppListSearchResultType::kPlayStoreApp ||
      result()->result_type() == AppListSearchResultType::kInstantApp) {
    accessible_name = l10n_util::GetStringFUTF16(
        IDS_APP_ACCESSIBILITY_ARC_APP_ANNOUNCEMENT, title);
  } else if (result()->result_type() ==
             AppListSearchResultType::kPlayStoreReinstallApp) {
    accessible_name = l10n_util::GetStringFUTF16(
        IDS_APP_ACCESSIBILITY_APP_RECOMMENDATION_ARC, title);
  } else if (result()->result_type() ==
             AppListSearchResultType::kInstalledApp) {
    accessible_name = l10n_util::GetStringFUTF16(
        IDS_APP_ACCESSIBILITY_INSTALLED_APP_ANNOUNCEMENT, title);
  } else if (result()->result_type() == AppListSearchResultType::kInternalApp) {
    accessible_name = l10n_util::GetStringFUTF16(
        IDS_APP_ACCESSIBILITY_INTERNAL_APP_ANNOUNCEMENT, title);
  } else if (!result()->details().empty()) {
    accessible_name = base::JoinString({title, result()->details()}, u", ");
  } else {
    accessible_name = title;
  }

  if (result()->rating() && result()->rating() >= 0) {
    accessible_name = l10n_util::GetStringFUTF16(
        IDS_APP_ACCESSIBILITY_APP_WITH_STAR_RATING_ARC, accessible_name,
        base::FormatDouble(result()->rating(), 1));
  }
  return accessible_name;
}

void SearchResultBaseView::UpdateAccessibleName() {
  SetAccessibleName(ComputeAccessibleName());
}

void SearchResultBaseView::ClearResult() {
  if (result_)
    result_->RemoveObserver(this);
  SetSelected(false, absl::nullopt);
  result_ = nullptr;
}

void SearchResultBaseView::SelectInitialResultAction(bool reverse_tab_order) {
  if (actions_view_)
    actions_view_->SelectInitialAction(reverse_tab_order);
}

void SearchResultBaseView::ClearSelectedResultAction() {
  if (actions_view_)
    actions_view_->ClearSelectedAction();
}

}  // namespace ash
