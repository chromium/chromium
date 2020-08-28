// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_bitmap_item_view.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

// The preferred height for the bitmap.
constexpr int kBitmapHeight = 64;

// The margins of the delete button.
constexpr gfx::Insets kDeleteButtonMargins =
    gfx::Insets(/*top=*/4, /*left=*/0, /*bottom=*/0, /*right=*/4);
}  // namespace

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// ClipboardHistoryBitmapItemView::BitmapContentsView

class ClipboardHistoryBitmapItemView::BitmapContentsView
    : public ClipboardHistoryBitmapItemView::ContentsView {
 public:
  explicit BitmapContentsView(ClipboardHistoryBitmapItemView* container)
      : ContentsView(container) {}
  BitmapContentsView(const BitmapContentsView& rhs) = delete;
  BitmapContentsView& operator=(const BitmapContentsView& rhs) = delete;
  ~BitmapContentsView() override = default;

  // ContentsView:
  DeleteButton* CreateDeleteButton() override {
    auto delete_button_container = std::make_unique<views::View>();
    auto* layout_manager = delete_button_container->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
    layout_manager->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kEnd);
    layout_manager->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);

    auto delete_button = std::make_unique<DeleteButton>(container_);
    delete_button->SetVisible(false);
    delete_button->SetProperty(views::kMarginsKey, kDeleteButtonMargins);
    DeleteButton* delete_button_ptr =
        delete_button_container->AddChildView(std::move(delete_button));
    AddChildView(std::move(delete_button_container));

    return delete_button_ptr;
  }
};

////////////////////////////////////////////////////////////////////////////////
// ClipboardHistoryBitmapItemView

ClipboardHistoryBitmapItemView::ClipboardHistoryBitmapItemView(
    const ClipboardHistoryItem& item,
    views::MenuItemView* container)
    : ClipboardHistoryItemView(container),
      original_image_(
          gfx::ImageSkia::CreateFrom1xBitmap(item.data().bitmap())) {}

ClipboardHistoryBitmapItemView::~ClipboardHistoryBitmapItemView() = default;

const char* ClipboardHistoryBitmapItemView::GetClassName() const {
  return "ClipboardHistoryBitmapItemView";
}

std::unique_ptr<ClipboardHistoryBitmapItemView::ContentsView>
ClipboardHistoryBitmapItemView::CreateContentsView() {
  auto contents_view = std::make_unique<BitmapContentsView>(this);
  contents_view->SetLayoutManager(std::make_unique<views::FillLayout>());

  auto image_view = std::make_unique<views::ImageView>();
  image_view->SetImage(original_image_);
  image_view->SetPreferredSize(gfx::Size(INT_MAX, kBitmapHeight));
  image_view_ = contents_view->AddChildView(std::move(image_view));

  contents_view->InstallDeleteButton();
  return contents_view;
}

void ClipboardHistoryBitmapItemView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  image_view_->SetImageSize(CalculateTargetImageSize());
}

gfx::Size ClipboardHistoryBitmapItemView::CalculateTargetImageSize() const {
  const gfx::Size image_size = image_view_->GetImage().size();
  const double width_ratio = image_size.width() / double(width());
  const double height_ratio = image_size.height() / double(height());

  if (width_ratio <= 1.f || height_ratio <= 1.f)
    return image_size;

  const double resize_ratio = std::fmin(width_ratio, height_ratio);
  return gfx::Size(image_size.width() / resize_ratio,
                   image_size.height() / resize_ratio);
}

}  // namespace ash
