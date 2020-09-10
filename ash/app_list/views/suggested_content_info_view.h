// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SUGGESTED_CONTENT_INFO_VIEW_H_
#define ASH_APP_LIST_VIEWS_SUGGESTED_CONTENT_INFO_VIEW_H_

#include "ash/app_list/views/privacy_info_view.h"
#include "ui/views/controls/button/button.h"

namespace ash {

class AppListViewDelegate;
class PrivacyContainerView;

// View representing Suggested Content privacy info in the Launcher.
class SuggestedContentInfoView : public PrivacyInfoView {
 public:
  SuggestedContentInfoView(AppListViewDelegate* view_delegate,
                           PrivacyContainerView* container);
  SuggestedContentInfoView(const SuggestedContentInfoView&) = delete;
  SuggestedContentInfoView& operator=(const SuggestedContentInfoView&) = delete;
  ~SuggestedContentInfoView() override;

  // PrivacyInfoView:
  void CloseButtonPressed() override;
  void LinkClicked() override;

 private:
  AppListViewDelegate* const view_delegate_;
  PrivacyContainerView* const container_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SUGGESTED_CONTENT_INFO_VIEW_H_
