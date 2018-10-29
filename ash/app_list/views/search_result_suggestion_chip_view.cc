// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_suggestion_chip_view.h"

#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace app_list {

namespace {

// Records an app being launched.
void LogAppLaunch(int index_in_suggestion_chip_container) {
  DCHECK_GE(index_in_suggestion_chip_container, 0);
  base::UmaHistogramSparse("Apps.AppListSuggestedChipLaunched",
                           index_in_suggestion_chip_container);

  UMA_HISTOGRAM_BOOLEAN(kAppListAppLaunchedFullscreen,
                        true /* suggested app */);

  base::RecordAction(base::UserMetricsAction("AppList_OpenSuggestedApp"));
}

}  // namespace

SearchResultSuggestionChipView::SearchResultSuggestionChipView(
    AppListViewDelegate* view_delegate)
    : view_delegate_(view_delegate), weak_ptr_factory_(this) {
  suggestion_chip_view_ = new SuggestionChipView(
      app_list::SuggestionChipView::Params(), /* listener */ this);
  AddChildView(suggestion_chip_view_);
}

SearchResultSuggestionChipView::~SearchResultSuggestionChipView() {
  SetSearchResult(nullptr);
}

void SearchResultSuggestionChipView::SetSearchResult(SearchResult* item) {
  if (item == item_)
    return;

  // Replace old item with new item.
  if (item_)
    item_->RemoveObserver(this);
  item_ = item;
  if (item_)
    item_->AddObserver(this);

  SetVisible(!!item_);
  UpdateSuggestionChipView();
}

void SearchResultSuggestionChipView::SetIndexInSuggestionChipContainer(
    size_t index) {
  index_in_suggestion_chip_container_ = index;
}

void SearchResultSuggestionChipView::OnMetadataChanged() {
  UpdateSuggestionChipView();
}

void SearchResultSuggestionChipView::OnResultDestroying() {
  SetSearchResult(nullptr);
}

void SearchResultSuggestionChipView::ButtonPressed(views::Button* sender,
                                                   const ui::Event& event) {
  DCHECK(item_);
  LogAppLaunch(index_in_suggestion_chip_container_);
  RecordSearchResultOpenSource(item_, view_delegate_->GetModel(),
                               view_delegate_->GetSearchModel());
  view_delegate_->OpenSearchResult(item_->id(), event.flags());
}

void SearchResultSuggestionChipView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty() || !item_)
    return;

  suggestion_chip_view_->SetBoundsRect(rect);
}

const char* SearchResultSuggestionChipView::GetClassName() const {
  return "SearchResultSuggestionChipView";
}

gfx::Size SearchResultSuggestionChipView::CalculatePreferredSize() const {
  if (!suggestion_chip_view_)
    return gfx::Size();

  return suggestion_chip_view_->GetPreferredSize();
}

void SearchResultSuggestionChipView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kGenericContainer;
}

void SearchResultSuggestionChipView::UpdateSuggestionChipView() {
  if (!item_) {
    suggestion_chip_view_->SetIcon(gfx::ImageSkia());
    suggestion_chip_view_->SetText(base::string16());
    suggestion_chip_view_->SetAccessibleName(base::string16());
    return;
  }

  suggestion_chip_view_->SetIcon(item_->chip_icon());
  suggestion_chip_view_->SetText(item_->title());

  base::string16 accessible_name = item_->title();
  if (item_->id() == app_list::kInternalAppIdContinueReading) {
    accessible_name = l10n_util::GetStringFUTF16(
        IDS_APP_LIST_CONTINUE_READING_ACCESSIBILE_NAME, accessible_name);
  }
  suggestion_chip_view_->SetAccessibleName(accessible_name);
}

}  // namespace app_list
