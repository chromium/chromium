// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SUGGESTION_CHIP_CONTAINER_VIEW_H_
#define ASH_APP_LIST_VIEWS_SUGGESTION_CHIP_CONTAINER_VIEW_H_

#include <vector>

#include "ash/app_list/views/search_result_container_view.h"
#include "ash/app_list/views/search_result_suggestion_chip_view.h"

namespace ash {

class ContentsView;

// A container that holds the suggestion chips.
class SuggestionChipContainerView : public SearchResultContainerView {
 public:
  explicit SuggestionChipContainerView(ContentsView* contents_view);

  SuggestionChipContainerView(const SuggestionChipContainerView&) = delete;
  SuggestionChipContainerView& operator=(const SuggestionChipContainerView&) =
      delete;

  ~SuggestionChipContainerView() override;

  // SearchResultContainerView:
  SearchResultSuggestionChipView* GetResultViewAt(size_t index) override;
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

  // Sets whether blur is disabled on suggestion chip views.
  void SetBlurDisabled(bool blur_disabled);

 private:
  // Enables or disables suggestion chips blur depending on the container state.
  void UpdateBlurState();

  ContentsView* contents_view_ = nullptr;  // Not owned
  views::BoxLayout* layout_manager_ = nullptr;  // Not owned

  // Whether tablet mode is active - tracked by OnTabletModeChanged().
  bool in_tablet_mode_ = false;

  // Whether suggestion chip blur has been explicitly disabled.
  bool blur_disabled_ = false;

  std::vector<SearchResultSuggestionChipView*> suggestion_chip_views_;  // Owned
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SUGGESTION_CHIP_CONTAINER_VIEW_H_
