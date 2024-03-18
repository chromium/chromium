// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_PINE_TEST_API_H_
#define ASH_WM_WINDOW_RESTORE_PINE_TEST_API_H_

#include <memory>

#include "ash/wm/window_restore/pine_contents_view.h"
#include "ash/wm/window_restore/pine_item_view.h"
#include "ash/wm/window_restore/pine_items_container_view.h"
#include "ash/wm/window_restore/pine_items_overflow_view.h"
#include "ash/wm/window_restore/pine_screenshot_icon_row_view.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class PillButton;
class SystemDialogDelegateView;

class PineContentsViewTestApi {
 public:
  explicit PineContentsViewTestApi(const PineContentsView* pine_contents_view);
  PineContentsViewTestApi(const PineContentsViewTestApi&) = delete;
  PineContentsViewTestApi& operator=(const PineContentsViewTestApi&) = delete;
  ~PineContentsViewTestApi();

  const PillButton* restore_button() const {
    return pine_contents_view_->restore_button_for_testing_;
  }
  const PillButton* cancel_button() const {
    return pine_contents_view_->cancel_button_for_testing_;
  }
  const PineItemsContainerView* items_container_view() const {
    return pine_contents_view_->items_container_view_;
  }
  const PineScreenshotIconRowView* screenshot_icon_row_view() const {
    return pine_contents_view_->screenshot_icon_row_view_;
  }
  const PineItemsOverflowView* overflow_view() const {
    return pine_contents_view_->items_container_view_
        ->overflow_view_for_testing_;
  }

 private:
  const raw_ptr<const PineContentsView> pine_contents_view_;
};

class PineItemViewTestApi {
 public:
  explicit PineItemViewTestApi(const PineItemView* pine_item_view);
  PineItemViewTestApi(const PineItemViewTestApi&) = delete;
  PineItemViewTestApi& operator=(const PineItemViewTestApi&) = delete;
  ~PineItemViewTestApi();

  const views::BoxLayoutView* favicon_container_view_for_testing() {
    return pine_item_view_->favicon_container_view_;
  }

 private:
  const raw_ptr<const PineItemView> pine_item_view_;
};

class PineItemsOverflowViewTestApi {
 public:
  explicit PineItemsOverflowViewTestApi(
      const PineItemsOverflowView* overflow_view);
  PineItemsOverflowViewTestApi(const PineItemsOverflowViewTestApi&) = delete;
  PineItemsOverflowViewTestApi& operator=(const PineItemsOverflowViewTestApi&) =
      delete;
  ~PineItemsOverflowViewTestApi();

  size_t image_views_count() const {
    return overflow_view_->image_view_map_.size();
  }
  size_t top_row_view_children_count() const {
    return overflow_view_->top_row_view_->children().size();
  }
  size_t bottom_row_view_children_count() const {
    return overflow_view_->bottom_row_view_->children().size();
  }

 private:
  const raw_ptr<const PineItemsOverflowView> overflow_view_;
};

class PineTestApi {
 public:
  explicit PineTestApi();
  PineTestApi(const PineTestApi&) = delete;
  PineTestApi& operator=(const PineTestApi&) = delete;
  ~PineTestApi();

  void SetPineContentsDataForTesting(
      std::unique_ptr<PineContentsData> pine_contents_data);

  SystemDialogDelegateView* GetOnboardingDialog();
};

class PineScreenshotIconRowViewTestApi {
 public:
  explicit PineScreenshotIconRowViewTestApi(
      const PineScreenshotIconRowView* icon_row_view);
  PineScreenshotIconRowViewTestApi(const PineScreenshotIconRowViewTestApi&) =
      delete;
  PineScreenshotIconRowViewTestApi& operator=(
      const PineScreenshotIconRowViewTestApi&) = delete;
  ~PineScreenshotIconRowViewTestApi();

  size_t image_views_count() const {
    return icon_row_view_->image_view_map_.size();
  }

 private:
  const raw_ptr<const PineScreenshotIconRowView> icon_row_view_;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_PINE_TEST_API_H_
