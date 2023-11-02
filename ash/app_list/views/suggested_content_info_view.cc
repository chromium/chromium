// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/suggested_content_info_view.h"

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/privacy_container_view.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "url/gurl.h"

namespace ash {

SuggestedContentInfoView::SuggestedContentInfoView(
    AppListViewDelegate* view_delegate,
    PrivacyContainerView* container)
    : PrivacyInfoView(IDS_APP_LIST_SUGGESTED_CONTENT_INFO,
                      IDS_APP_LIST_MANAGE_SETTINGS),
      view_delegate_(view_delegate),
      container_(container) {}

SuggestedContentInfoView::~SuggestedContentInfoView() = default;

void SuggestedContentInfoView::CloseButtonPressed() {
  view_delegate_->MarkSuggestedContentInfoDismissed();
  container_->Update();
}

void SuggestedContentInfoView::LinkClicked() {
  view_delegate_->MarkSuggestedContentInfoDismissed();
  constexpr char url[] = "chrome://os-settings/osPrivacy";
  NewWindowDelegate::GetInstance()->OpenUrl(
      GURL(url), NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

}  // namespace ash
