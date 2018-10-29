// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SUGGESTION_CHIP_CONTAINER_VIEW_H_
#define ASH_APP_LIST_VIEWS_SUGGESTION_CHIP_CONTAINER_VIEW_H_

#include <vector>

#include "ash/app_list/views/search_result_container_view.h"
#include "base/macros.h"

namespace app_list {

class AppListViewDelegate;
class ContentsView;
class SearchResultSuggestionChipView;

// A container that holds the suggestion chips.
class SuggestionChipContainerView : public SearchResultContainerView {
 public:
  explicit SuggestionChipContainerView(ContentsView* contents_view);
  ~SuggestionChipContainerView() override;

  // SearchResultContainerView:
  int DoUpdate() override;
  const char* GetClassName() const override;

  // views::View:
  void Layout() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // Suggestion chips become unfocusable if |disabled| is true. This is used to
  // trap focus within the folder when it is opened.
  void DisableFocusForShowingActiveFolder(bool disabled);

  // Called when tablet mode starts and ends.
  void OnTabletModeChanged(bool started);

 private:
  // Returns true if update and layout should be ignored.
  bool IgnoreUpdateAndLayout() const;

  ContentsView* contents_view_ = nullptr;  // Not owned
  AppListViewDelegate* view_delegate_ = nullptr;

  std::vector<SearchResultSuggestionChipView*> suggestion_chip_views_;  // Owned

  DISALLOW_COPY_AND_ASSIGN(SuggestionChipContainerView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_SUGGESTION_CHIP_CONTAINER_VIEW_H_
