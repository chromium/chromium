// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_image_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/pulsing_block_view.h"
#include "ash/app_list/views/search_result_image_list_view.h"
#include "ash/app_list/views/search_result_image_view_delegate.h"
#include "ash/style/ash_color_id.h"
#include "cc/paint/display_item_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Sizing and spacing values for `result_image_`.
constexpr int kRoundedCornerRadius = 16;

// Focus ring constants
constexpr int kSpaceBetweenFocusRingAndImage = 2;
constexpr int kFocusRingCornerRadius =
    kRoundedCornerRadius + kSpaceBetweenFocusRingAndImage;
constexpr gfx::Insets kFocusRingInsets =
    gfx::Insets(-kSpaceBetweenFocusRingAndImage -
                views::FocusRing::kDefaultHaloThickness / 2);

// Dragged image constants
constexpr double kDraggedImageOpacity = 0.6;

class ImagePreviewView : public views::ImageButton {
 public:
  ImagePreviewView() { SetInstallFocusRingOnFocus(false); }
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
    views::FocusRing::Get(parent())->SchedulePaint();
  }
};

}  // namespace

SearchResultImageView::SearchResultImageView(
    int index,
    SearchResultImageListView* list_view)
    : index_(index), list_view_(list_view) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  result_image_ = AddChildView(std::make_unique<ImagePreviewView>());
  result_image_->SetCanProcessEventsWithinSubtree(false);
  result_image_->GetViewAccessibility().OverrideIsIgnored(true);

  views::FocusRing::Install(this);
  views::FocusRing* const focus_ring = views::FocusRing::Get(this);
  const bool is_jelly_enabled = chromeos::features::IsJellyEnabled();
  focus_ring->SetColorId(is_jelly_enabled ? static_cast<ui::ColorId>(
                                                cros_tokens::kCrosSysFocusRing)
                                          : ui::kColorAshFocusRing);
  focus_ring->SetHasFocusPredicate(base::BindRepeating([](const View* view) {
    const auto* v = views::AsViewClass<SearchResultBaseView>(view);
    CHECK(v);
    return v->selected();
  }));
  views::InstallRoundRectHighlightPathGenerator(this, kFocusRingInsets,
                                                kFocusRingCornerRadius);

  SetCallback(base::BindRepeating(&SearchResultImageView::OnImageViewPressed,
                                  base::Unretained(this)));

  set_context_menu_controller(SearchResultImageViewDelegate::Get());
  set_drag_controller(SearchResultImageViewDelegate::Get());
}

void SearchResultImageView::OnImageViewPressed(const ui::Event& event) {
  list_view_->SearchResultActivated(this, event.flags(),
                                    true /* by_button_press */);
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

gfx::Size SearchResultImageView::CalculatePreferredSize() const {
  // Keep the ratio of the width and height be 3:2.
  return gfx::Size(preferred_width_, 2 * preferred_width_ / 3);
}

void SearchResultImageView::OnResultChanged() {
  OnMetadataChanged();
  SchedulePaint();
}

void SearchResultImageView::ConfigureLayoutForAvailableWidth(int width) {
  if (preferred_width_ == width) {
    return;
  }
  preferred_width_ = width;
  PreferredSizeChanged();
}

void SearchResultImageView::CreatePulsingBlockView() {
  pulsing_block_view_ = AddChildView(std::make_unique<PulsingBlockView>(
      size(), base::Milliseconds(index_ * 200), kRoundedCornerRadius));
  pulsing_block_view_->SetCanProcessEventsWithinSubtree(false);
  pulsing_block_view_->GetViewAccessibility().OverrideIsIgnored(true);
}

gfx::ImageSkia SearchResultImageView::CreateDragImage() {
  const ui::Compositor* compositor = GetWidget()->GetCompositor();
  const float scale = compositor->device_scale_factor();
  const gfx::Rect paint_bounds(gfx::ScaleToCeiledSize(size(), scale));
  const bool is_pixel_canvas = compositor->is_pixel_canvas();
  SkBitmap bitmap;
  bitmap.allocN32Pixels(paint_bounds.width(), paint_bounds.height());
  bitmap.eraseColor(SK_ColorTRANSPARENT);

  SkCanvas canvas(bitmap);
  auto list = base::MakeRefCounted<cc::DisplayItemList>();
  ui::PaintContext context(list.get(), scale, paint_bounds, is_pixel_canvas);

  result_image_->Paint(
      views::PaintInfo::CreateRootPaintInfo(context, paint_bounds.size()));
  list->Finalize();
  list->Raster(&canvas, nullptr);

  gfx::ImageSkia dragged_image =
      gfx::ImageSkia::CreateFromBitmap(bitmap, scale);
  return gfx::ImageSkiaOperations::CreateTransparentImage(dragged_image,
                                                          kDraggedImageOpacity);
}

void SearchResultImageView::OnMetadataChanged() {
  UpdateAccessibleName();
  // By default, the description will be set to the tooltip text, but the title
  // is already announced in the accessible name.
  SetAccessibleDescription(
      u"", ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);

  if (!result() || result()->icon().icon.IsEmpty()) {
    result_image_->SetVisible(false);
    if (result() && !pulsing_block_view_) {
      CreatePulsingBlockView();
    }

    return;
  }

  result_image_->SetVisible(true);
  if (pulsing_block_view_) {
    RemoveChildViewT(pulsing_block_view_.get());
    pulsing_block_view_ = nullptr;
  }

  gfx::ImageSkia image = result()->icon().icon.Rasterize(GetColorProvider());
  if (!GetContentsBounds().IsEmpty()) {
    image = gfx::ImageSkiaOperations::CreateResizedImage(
        image, skia::ImageOperations::RESIZE_BEST, GetContentsBounds().size());
  }

  result_image_->SetImage(views::Button::STATE_NORMAL, image);
  SetTooltipText(result()->title());
}

SearchResultImageView::~SearchResultImageView() = default;

BEGIN_METADATA(SearchResultImageView, SearchResultBaseView)
END_METADATA

}  // namespace ash
