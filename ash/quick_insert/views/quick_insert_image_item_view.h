// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_IMAGE_ITEM_VIEW_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_IMAGE_ITEM_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/quick_insert/model/quick_insert_action_type.h"
#include "ash/quick_insert/views/quick_insert_item_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class ImageView;
}  // namespace views

namespace ash {

// Quick Insert item which contains just an image.
class ASH_EXPORT QuickInsertImageItemView : public QuickInsertItemView {
  METADATA_HEADER(QuickInsertImageItemView, QuickInsertItemView)

 public:
  QuickInsertImageItemView(std::unique_ptr<views::ImageView> image,
                           std::u16string accessible_name,
                           SelectItemCallback select_item_callback);
  QuickInsertImageItemView(const QuickInsertImageItemView&) = delete;
  QuickInsertImageItemView& operator=(const QuickInsertImageItemView&) = delete;
  ~QuickInsertImageItemView() override;

  void SetAction(QuickInsertActionType action);

  // Resizes the contained image to `width`, with the height scaled to retain
  // the same aspect ratio.
  void FitToWidth(int width);

  views::ImageView* image_view_for_testing() const { return image_view_; }

 private:
  raw_ptr<views::ImageView> image_view_ = nullptr;
  std::u16string accessible_name_;
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_IMAGE_ITEM_VIEW_H_
