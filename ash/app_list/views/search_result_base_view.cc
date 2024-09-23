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
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

SearchResultBaseView::SearchResultBaseView() {
  SetGroup(kSearchResultViewGroup);
  SetInstallFocusRingOnFocus(false);

  // Result views are not expected to be focused - while the results UI is shown
  // the focus is kept within the `SearchBoxView`, which manages result
  // selection state in response to keyboard navigation keys, and forwards
  // all relevant key events (e.g. ENTER key for result activation) to search
  // result views as needed.
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  // Mark the result is a list item in the list of search results.
  // Also avoids an issue with the nested button case(append and remove
  // button are child button of SearchResultView), which is not supported by
  // ChromeVox. see details in crbug.com/924776.
  GetViewAccessibility().SetRole(ax::mojom::Role::kListBoxOption);
  UpdateAccessibleName();
  UpdateAccessibleDefaultAction();
}

SearchResultBaseView::~SearchResultBaseView() {
  if (result_) {
    result_->RemoveObserver(this);
  }
  result_ = nullptr;
}

bool SearchResultBaseView::SkipDefaultKeyEventProcessing(
    const ui::KeyEvent& event) {
  // Ensure accelerators take priority in the app list. This ensures, e.g., that
  // Ctrl+Space will switch input methods rather than activate the button.
  return false;
}

void SearchResultBaseView::SetVisible(bool visible) {
  views::Button::SetVisible(visible);
  UpdateAccessibleDefaultAction();
}

void SearchResultBaseView::SetSelected(bool selected,
                                       std::optional<bool> reverse_tab_order) {
  if (selected_ == selected) {
    return;
  }

  selected_ = selected;

  if (selected) {
    SelectInitialResultAction(reverse_tab_order.value_or(false));
  } else {
    ClearSelectedResultAction();
  }

  SchedulePaint();
}

bool SearchResultBaseView::SelectNextResultAction(bool reverse_tab_order) {
  if (!selected() || !actions_view_) {
    return false;
  }

  if (!actions_view_->SelectNextAction(reverse_tab_order)) {
    return false;
  }

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
  if (result_) {
    result_->AddObserver(this);
  }
  OnResultChanged();

  UpdateAccessibleName();
}

void SearchResultBaseView::OnResultDestroying() {
  ClearResult();
}

std::u16string SearchResultBaseView::ComputeAccessibleName() const {
  if (!result()) {
    return u"";
  }

  std::u16string accessible_name;
  if (!result()->accessible_name().empty()) {
    return result()->accessible_name();
  }

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
  // It is possible for the view to be visible but lack a result. When this
  // happens, `ComputeAccessibleName()` will return an empty string. Because
  // the focusable state is set in the constructor and not updated when the
  // result is removed, the accessibility paint checks will fail.
  const std::u16string name = ComputeAccessibleName();
  if (name.empty()) {
    GetViewAccessibility().SetName(
        name, ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  } else {
    GetViewAccessibility().SetName(name);
  }
}

void SearchResultBaseView::OnEnabledChanged() {
  views::Button::OnEnabledChanged();
  UpdateAccessibleDefaultAction();
}

void SearchResultBaseView::ClearResult() {
  if (result_) {
    result_->RemoveObserver(this);
  }
  SetSelected(false, std::nullopt);
  result_ = nullptr;
}

void SearchResultBaseView::SelectInitialResultAction(bool reverse_tab_order) {
  if (actions_view_) {
    actions_view_->SelectInitialAction(reverse_tab_order);
  }
}

void SearchResultBaseView::ClearSelectedResultAction() {
  if (actions_view_) {
    actions_view_->ClearSelectedAction();
  }
}

void SearchResultBaseView::UpdateAccessibleDefaultAction() {
  if (GetVisible()) {
    GetViewAccessibility().SetDefaultActionVerb(
        ax::mojom::DefaultActionVerb::kClick);
  } else {
    GetViewAccessibility().RemoveDefaultActionVerb();
  }
}

BEGIN_METADATA(SearchResultBaseView)
END_METADATA

}  // namespace ash
