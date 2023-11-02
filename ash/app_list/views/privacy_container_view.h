// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_PRIVACY_CONTAINER_VIEW_H_
#define ASH_APP_LIST_VIEWS_PRIVACY_CONTAINER_VIEW_H_

#include "ash/app_list/views/search_result_container_view.h"

namespace ash {
namespace test {
class PrivacyContainerViewTest;
}

class AppListViewDelegate;
class SearchResult;
class SuggestedContentInfoView;

// A container view for privacy info.
class ASH_EXPORT PrivacyContainerView : public SearchResultContainerView {
 public:
  explicit PrivacyContainerView(AppListViewDelegate* view_delegate);
  ~PrivacyContainerView() override;
  PrivacyContainerView(const PrivacyContainerView&) = delete;
  PrivacyContainerView& operator=(const PrivacyContainerView&) = delete;

  // SearchResultContainerView:
  SearchResultBaseView* GetResultViewAt(size_t index) override;

 private:
  friend class test::PrivacyContainerViewTest;

  // SearchResultContainerView:
  int DoUpdate() override;

  // Owned by views hierarchy.
  SuggestedContentInfoView* suggested_content_info_view_ = nullptr;

  // A skeleton result that contains an id.
  SearchResult result_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_PRIVACY_CONTAINER_VIEW_H_
