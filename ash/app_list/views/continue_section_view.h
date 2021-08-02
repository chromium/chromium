// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_CONTINUE_SECTION_VIEW_H_
#define ASH_APP_LIST_VIEWS_CONTINUE_SECTION_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

class AppListViewDelegate;
class ContinueTaskView;

// The "Continue" section of the bubble launcher. This view wraps around
// suggestions with tasks to continue.
class ASH_EXPORT ContinueSectionView : public views::View {
 public:
  METADATA_HEADER(ContinueSectionView);

  ContinueSectionView(AppListViewDelegate* view_delegate, int columns);
  ContinueSectionView(const ContinueSectionView&) = delete;
  ContinueSectionView& operator=(const ContinueSectionView&) = delete;
  ~ContinueSectionView() override;

  size_t GetTasksSuggestionsCount() const;
  ContinueTaskView* GetTaskViewAtForTesting(size_t index) const;

 private:
  AppListViewDelegate* const view_delegate_;

  const int columns_;
  views::View* suggestions_container_ = nullptr;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_CONTINUE_SECTION_VIEW_H_
