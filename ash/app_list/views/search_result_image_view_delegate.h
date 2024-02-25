// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_IMAGE_VIEW_DELEGATE_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_IMAGE_VIEW_DELEGATE_H_

#include "ash/ash_export.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/drag_controller.h"

namespace ash {
class SearchResultImageView;

// A delegate for `SearchResultImageView` which implements drag and drop.
class ASH_EXPORT SearchResultImageViewDelegate : public views::DragController {
 public:
  SearchResultImageViewDelegate();
  SearchResultImageViewDelegate(const SearchResultImageViewDelegate&) = delete;
  SearchResultImageViewDelegate& operator=(
      const SearchResultImageViewDelegate&) = delete;
  ~SearchResultImageViewDelegate() override;

 private:
  // views::DragController:
  bool CanStartDragForView(views::View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& current_pt) override;
  int GetDragOperationsForView(views::View* sender,
                               const gfx::Point& press_pt) override;
  void WriteDragDataForView(views::View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_IMAGE_VIEW_DELEGATE_H_
