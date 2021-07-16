// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_CONTINUE_SECTION_VIEW_H_
#define ASH_APP_LIST_VIEWS_CONTINUE_SECTION_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

class AppListViewDelegate;

// The "Continue" section of the bubble launcher. This view wraps around
// suggestions with tasks to continue.
class ASH_EXPORT ContinueSectionView : public views::View {
 public:
  METADATA_HEADER(ContinueSectionView);

  explicit ContinueSectionView(AppListViewDelegate* view_delegate);
  ContinueSectionView(const ContinueSectionView&) = delete;
  ContinueSectionView& operator=(const ContinueSectionView&) = delete;
  ~ContinueSectionView() override;

 private:
  AppListViewDelegate* const view_delegate_;

  views::View* continue_suggestions_ = nullptr;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_CONTINUE_SECTION_VIEW_H_
