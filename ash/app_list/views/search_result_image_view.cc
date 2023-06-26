// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_image_view.h"

#include <algorithm>

#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/search_result_image_list_view.h"
#include "ash/app_list/views/search_result_image_view_delegate.h"
#include "ash/style/ash_color_id.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Sizing and spacing values for `result_image_`.
constexpr int kTopBottomMargin = 10;
constexpr int kLeftRightMargin = 15;
constexpr int kRoundedCornerRadius = 8;

class ImagePreviewView : public views::ImageButton {
 public:
  ImagePreviewView() = default;
  ImagePreviewView(const ImagePreviewView&) = delete;
  ImagePreviewView& operator=(const ImagePreviewView&) = delete;
  ~ImagePreviewView() override = default;

 private:
  // views::ImageView:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    if (GetImage(views::Button::STATE_NORMAL).isNull()) {
      return;
    }

    SkPath mask;
    mask.addRoundRect(gfx::RectToSkRect(GetContentsBounds()),
                      kRoundedCornerRadius, kRoundedCornerRadius);
    canvas->ClipPath(mask, true);
    views::ImageButton::PaintButtonContents(canvas);
  }
};

}  // namespace

SearchResultImageView::SearchResultImageView(
    SearchResultImageListView* list_view,
    std::string dummy_result_id)
    : list_view_(list_view) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  result_image_ = AddChildView(std::make_unique<ImagePreviewView>());
  result_image_->SetCanProcessEventsWithinSubtree(false);
  result_image_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kTopBottomMargin, kLeftRightMargin, kTopBottomMargin, kLeftRightMargin)));

  SetCallback(base::BindRepeating(&SearchResultImageView::OnImageViewPressed,
                                  base::Unretained(this)));

  set_context_menu_controller(SearchResultImageViewDelegate::Get());
  set_drag_controller(SearchResultImageViewDelegate::Get());
}

void SearchResultImageView::OnImageViewPressed(const ui::Event& event) {
  list_view_->SearchResultActivated(this, event.flags(),
                                    true /* by_button_press */);
}

void SearchResultImageView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kListBoxOption;
  node_data->SetName(result() ? result()->title() : u"");
  // TODO(crbug.com/1352636) update with internationalized accessible name if we
  // launch this feature.
}

void SearchResultImageView::OnGestureEvent(ui::GestureEvent* event) {
  SearchResultImageViewDelegate::Get()->HandleSearchResultImageViewGestureEvent(
      this, *event);
  SearchResultBaseView::OnGestureEvent(event);
}

void SearchResultImageView::OnMouseEvent(ui::MouseEvent* event) {
  SearchResultImageViewDelegate::Get()->HandleSearchResultImageViewMouseEvent(
      this, *event);
  SearchResultBaseView::OnMouseEvent(event);
}

void SearchResultImageView::OnResultChanged() {
  OnMetadataChanged();
  SchedulePaint();
}

void SearchResultImageView::OnMetadataChanged() {
  if (!result() || result()->icon().icon.IsEmpty()) {
    result_image_->SetImage(views::Button::STATE_NORMAL, gfx::ImageSkia());
    return;
  }

  const gfx::ImageSkia& image =
      result()->icon().icon.Rasterize(GetColorProvider());
  result_image_->SetImage(views::Button::STATE_NORMAL, image);
  result_image_->SetTooltipText(result()->title());
  result_image_->SetAccessibleName(result()->title());
}

SearchResultImageView::~SearchResultImageView() = default;

BEGIN_METADATA(SearchResultImageView, SearchResultBaseView)
END_METADATA

}  // namespace ash
