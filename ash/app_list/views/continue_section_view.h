// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_CONTINUE_SECTION_VIEW_H_
#define ASH_APP_LIST_VIEWS_CONTINUE_SECTION_VIEW_H_

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/views/continue_task_container_view.h"
#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

class AppListViewDelegate;
class ContinueTaskContainerView;
class ContinueTaskView;

// The "Continue" section of the bubble launcher. This view wraps around
// suggestions with tasks to continue.
class ASH_EXPORT ContinueSectionView : public views::View,
                                       public AppListModelProvider::Observer {
 public:
  METADATA_HEADER(ContinueSectionView);

  ContinueSectionView(AppListViewDelegate* view_delegate,
                      int columns,
                      bool tablet_mode);
  ContinueSectionView(const ContinueSectionView&) = delete;
  ContinueSectionView& operator=(const ContinueSectionView&) = delete;
  ~ContinueSectionView() override;

  // AppListModelProvider::Observer:
  void OnActiveAppListModelsChanged(AppListModel* model,
                                    SearchModel* search_model) override;

  // Called when the |suggestion_container_| finishes updating the tasks.
  void OnSearchResultContainerResultsChanged();

  // Schedule an update to the |suggestion_container_| tasks.
  void UpdateSuggestionTasks();

  size_t GetTasksSuggestionsCount() const;

  void DisableFocusForShowingActiveFolder(bool disabled);

  ContinueTaskView* GetTaskViewAtForTesting(size_t index) const;

 private:
  bool tablet_mode_ = false;

  ContinueTaskContainerView* suggestions_container_ = nullptr;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_CONTINUE_SECTION_VIEW_H_
