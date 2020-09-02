// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/privacy_container_view.h"

#include <memory>
#include <utility>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/assistant/assistant_privacy_info_view.h"
#include "ash/app_list/views/suggested_content_info_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

PrivacyContainerView::PrivacyContainerView(AppListViewDelegate* view_delegate)
    : SearchResultContainerView(view_delegate) {
  // Create both notices even though only one is shown at a time. This is
  // because one notice could be dismissed, and after that we should show the
  // other notice.
  assistant_privacy_info_view_ = AddChildView(
      std::make_unique<AssistantPrivacyInfoView>(view_delegate, this));
  // |ShouldShowSuggestedContentInfo()| cannot change from false to true in the
  // middle of a session.
  if (view_delegate->ShouldShowSuggestedContentInfo()) {
    suggested_content_info_view_ = AddChildView(
        std::make_unique<SuggestedContentInfoView>(view_delegate, this));
  }

  auto metadata = std::make_unique<SearchResultMetadata>();
  metadata->id = "PrivacyInfoResult";
  metadata->result_type = AppListSearchResultType::kInternalPrivacyInfo;
  result_.SetMetadata(std::move(metadata));

  // This container simply wraps around PrivacyInfoView.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(0, 0, 0, 0), 0));
}

PrivacyContainerView::~PrivacyContainerView() = default;

SearchResultBaseView* PrivacyContainerView::GetResultViewAt(size_t index) {
  if (index != 0) {
    // There is only one result.
    return nullptr;
  }
  if (assistant_privacy_info_view_ &&
      assistant_privacy_info_view_->GetVisible()) {
    return assistant_privacy_info_view_;
  }
  if (suggested_content_info_view_ &&
      suggested_content_info_view_->GetVisible()) {
    return suggested_content_info_view_;
  }
  return nullptr;
}

int PrivacyContainerView::DoUpdate() {
  const bool should_show_assistant =
      view_delegate()->ShouldShowAssistantPrivacyInfo();

  if (assistant_privacy_info_view_) {
    const bool has_result = assistant_privacy_info_view_->result();
    if (has_result != should_show_assistant) {
      assistant_privacy_info_view_->SetResult(should_show_assistant ? &result_
                                                                    : nullptr);
    }
    assistant_privacy_info_view_->SetVisible(should_show_assistant);
  }

  const bool should_show_suggested_content =
      view_delegate()->ShouldShowSuggestedContentInfo() &&
      !should_show_assistant;

  if (suggested_content_info_view_) {
    const bool has_result = suggested_content_info_view_->result();
    if (has_result != should_show_suggested_content) {
      suggested_content_info_view_->SetResult(
          should_show_suggested_content ? &result_ : nullptr);
    }
    suggested_content_info_view_->SetVisible(should_show_suggested_content);
  }

  // If visible, set the maximum score so that the privacy notice is always at
  // the top of the results list.
  const bool should_show_container =
      should_show_assistant || should_show_suggested_content;
  set_container_score(should_show_container
                          ? AppListConfig::instance().privacy_container_score()
                          : -1);
  return should_show_container ? 1 : 0;
}

}  // namespace ash
