// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_CONTINUE_TASK_VIEW_H_
#define ASH_APP_LIST_VIEWS_CONTINUE_TASK_VIEW_H_

#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/model/search/search_result_observer.h"
#include "ash/ash_export.h"
#include "ui/views/controls/button/button.h"

namespace views {
class ImageView;
class Label;
}

namespace ash {
class AppListViewDelegate;

// A view with a suggested task for the "Continue" section.
class ASH_EXPORT ContinueTaskView : public views::Button,
                                    public SearchResultObserver {
 public:
  METADATA_HEADER(ContinueTaskView);

  explicit ContinueTaskView(AppListViewDelegate* view_delegate);
  ContinueTaskView(const ContinueTaskView&) = delete;
  ContinueTaskView& operator=(const ContinueTaskView&) = delete;
  ~ContinueTaskView() override;

  // views::View:
  void OnThemeChanged() override;

  // SearchResultObserver:
  void OnResultDestroying() override;
  void OnMetadataChanged() override;

  void SetResult(SearchResult* result);

  void set_index_in_container(size_t index) { index_in_container_ = index; }
  SearchResult* result() const { return result_; }

 private:
  void SetIcon(const gfx::ImageSkia& icon);
  gfx::Size GetIconSize() const;
  void UpdateResult();

  void OnButtonPressed(const ui::Event& event);

  // The index of this view within a |SearchResultContainerView| that holds it.
  absl::optional<int> index_in_container_;

  AppListViewDelegate* const view_delegate_;
  views::Label* title_ = nullptr;
  views::Label* subtitle_ = nullptr;
  views::ImageView* icon_ = nullptr;
  SearchResult* result_ = nullptr;  // Owned by SearchModel::SearchResults.

  base::ScopedObservation<SearchResult, SearchResultObserver>
      search_result_observation_{this};

  base::WeakPtrFactory<ContinueTaskView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_CONTINUE_TASK_VIEW_H_
