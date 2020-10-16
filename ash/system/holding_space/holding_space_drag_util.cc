// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_drag_util.h"

#include <memory>

#include "ash/public/cpp/holding_space/holding_space_color_provider.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "base/containers/adapters.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/shadow_util.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/drag_utils.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace holding_space_util {

namespace {

// Appearance.
constexpr int kDragImageItemViewCornerRadius = 8;
constexpr int kDragImageItemViewElevation = 2;
constexpr int kDragImageItemChipViewIconSize = 24;
constexpr gfx::Insets kDragImageItemChipViewInsets(0, 13);
constexpr gfx::Size kDragImageItemChipViewPreferredSize(160, 40);
constexpr int kDragImageItemChipViewSpacing = 13;
constexpr gfx::Size kDragImageItemScreenshotViewPreferredSize(104, 80);
constexpr int kDragImageViewChildOffset = 8;

// Helpers ---------------------------------------------------------------------

#if DCHECK_IS_ON()
// Asserts that there are no `ui::Layer`s in the specified `view` hierarchy.
void AssertNoLayers(const views::View* view) {
  DCHECK(!view->layer());
  for (const views::View* child : view->children())
    AssertNoLayers(child);
}
#endif  // DCHECK_IS_ON()

// Returns the holding space items associated with the specified `views`.
std::vector<const HoldingSpaceItem*> GetHoldingSpaceItems(
    const std::vector<const HoldingSpaceItemView*> views) {
  std::vector<const HoldingSpaceItem*> items;
  for (const HoldingSpaceItemView* view : views)
    items.push_back(view->item());
  return items;
}

// DragImageLayoutManager ------------------------------------------------------

// A `views::LayoutManager` which lays out its children atop each other with a
// specified `child_offset`. Note that children are painted in reverse order.
class DragImageLayoutManager : public views::LayoutManagerBase {
 public:
  explicit DragImageLayoutManager(int child_offset)
      : child_offset_(child_offset) {}

  DragImageLayoutManager(const DragImageLayoutManager&) = delete;
  DragImageLayoutManager& operator=(const DragImageLayoutManager&) = delete;
  ~DragImageLayoutManager() override = default;

 private:
  // views::LayoutManagerBase:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override {
    views::ProposedLayout proposed_layout;

    int left = 0, top = 0;
    for (views::View* child_view : host_view()->children()) {
      const gfx::Size child_preferred_size = child_view->GetPreferredSize();

      // Child layout.
      views::ChildLayout child_layout;
      child_layout.available_size = views::SizeBounds(child_preferred_size);
      child_layout.bounds = gfx::Rect({left, top}, child_preferred_size);
      child_layout.child_view = child_view;
      child_layout.visible = true;
      proposed_layout.child_layouts.push_back(std::move(child_layout));

      // Host size.
      if (proposed_layout.host_size.IsEmpty()) {
        proposed_layout.host_size = child_preferred_size;
      } else {
        int host_width = left + child_preferred_size.width();
        int host_height = top + child_preferred_size.height();
        proposed_layout.host_size.SetToMax(gfx::Size(host_width, host_height));
      }

      left += child_offset_;
      top += child_offset_;
    }

    return proposed_layout;
  }

  std::vector<views::View*> GetChildViewsInPaintOrder(
      const views::View* host) const override {
    // Paint `children` in reverse order so that earlier views paint at a higher
    // z-index than later views, like a deck of cards with the first `child`
    // stacked on top.
    std::vector<views::View*> children;
    for (views::View* child : base::Reversed(host->children()))
      children.push_back(child);
    return children;
  }

  const int child_offset_;
};

// DragImageItemView -----------------------------------------------------------

// An abstract `views::View` which represents a single holding space item in the
// drag image for a collection of holding space item views. The main purpose of
// this view is to implement the shadow which is intentionally done without use
// of `ui::Layer`s to accommodate painting to an `SkBitmap`.
class DragImageItemView : public views::View {
 public:
  DragImageItemView(const DragImageItemView&) = delete;
  DragImageItemView& operator=(const DragImageItemView&) = delete;
  ~DragImageItemView() override = default;

 protected:
  DragImageItemView() = default;

  // views::View:
  gfx::Insets GetInsets() const final {
    // Add insets to accommodate the shadow so that the view's content will be
    // laid out within the appropriate shadow margins.
    return gfx::Insets(-gfx::ShadowValue::GetMargin(GetShadowDetails().values));
  }

 private:
  // views::View:
  void OnPaintBackground(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SK_ColorWHITE);
    flags.setLooper(gfx::CreateShadowDrawLooper(GetShadowDetails().values));
    canvas->DrawRoundRect(GetContentsBounds(), kDragImageItemViewCornerRadius,
                          flags);
  }

  const gfx::ShadowDetails& GetShadowDetails() const {
    return gfx::ShadowDetails::Get(kDragImageItemViewElevation,
                                   kDragImageItemViewCornerRadius);
  }
};

// DragImageItemChipView -------------------------------------------------------

// TODO(crbug.com/1139113): Support theming.
// A `DragImageItemView` which represents a single holding space `item` as a
// chip in the drag image for a collection of holding space item views.
class DragImageItemChipView : public DragImageItemView {
 public:
  explicit DragImageItemChipView(const HoldingSpaceItem* item) {
    InitLayout(item);
  }

 private:
  void InitLayout(const HoldingSpaceItem* item) {
    // NOTE: We enlarge `preferred_size` to accommodate the view's shadow.
    gfx::Size preferred_size(kDragImageItemChipViewPreferredSize);
    preferred_size.Enlarge(GetInsets().width(), GetInsets().height());
    SetPreferredSize(preferred_size);

    // Layout.
    views::BoxLayout* layout =
        SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            kDragImageItemChipViewInsets, kDragImageItemChipViewSpacing));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);

    // Icon.
    auto* icon = AddChildView(std::make_unique<RoundedImageView>(
        /*radius=*/kDragImageItemChipViewIconSize / 2,
        RoundedImageView::Alignment::kCenter));
    icon->SetPreferredSize(gfx::Size(kDragImageItemChipViewIconSize,
                                     kDragImageItemChipViewIconSize));
    icon->SetImage(item->image().image_skia(), icon->GetPreferredSize());

    // Label.
    auto* label = AddChildView(std::make_unique<views::Label>(item->text()));
    label->SetElideBehavior(gfx::ElideBehavior::ELIDE_MIDDLE);
    label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    layout->SetFlexForView(label, 1);

    TrayPopupItemStyle(TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL,
                       /*use_unified_theme=*/false)
        .SetupLabel(label);
  }
};

// DragImageItemScreenshotView -------------------------------------------------

// A `DragImageItemView` which represents a single holding space screenshot
// `item` in the drag image for a collection of holding space item views.
class DragImageItemScreenshotView : public DragImageItemView {
 public:
  explicit DragImageItemScreenshotView(const HoldingSpaceItem* item) {
    DCHECK_EQ(item->type(), HoldingSpaceItem::Type::kScreenshot);
    InitLayout(item);
  }

 private:
  void InitLayout(const HoldingSpaceItem* item) {
    // NOTE: We enlarge `preferred_size` to accommodate the view's shadow.
    gfx::Size preferred_size(kDragImageItemScreenshotViewPreferredSize);
    preferred_size.Enlarge(GetInsets().width(), GetInsets().height());
    SetPreferredSize(preferred_size);

    // Layout.
    SetLayoutManager(std::make_unique<views::FillLayout>());

    // Image.
    auto* image = AddChildView(std::make_unique<RoundedImageView>(
        kDragImageItemViewCornerRadius, RoundedImageView::Alignment::kCenter));
    image->SetPreferredSize(kDragImageItemScreenshotViewPreferredSize);
    image->SetImage(item->image().image_skia(), image->GetPreferredSize());
  }
};

// DragImageView ---------------------------------------------------------------

// A `views::View` for use as a drag image for a collection of holding space
// item `views`. This view expects to be painted to an `SkBitmap`.
class DragImageView : public views::View {
 public:
  explicit DragImageView(const std::vector<const HoldingSpaceItem*>& items) {
    InitLayout(items);
  }

  DragImageView(const DragImageView&) = delete;
  DragImageView& operator=(const DragImageView&) = delete;
  ~DragImageView() override = default;

  // Paints this view to a `gfx::ImageSkia`.
  gfx::ImageSkia PaintToImageSkia(float scale, bool is_pixel_canvas) {
#if DCHECK_IS_ON()
    // NOTE: This method will *not* paint `ui::Layer`s, so it is expected that
    // all views in this view hierarchy *not* paint to layers.
    AssertNoLayers(this);
#endif  // DCHECK_IS_ON()
    SkBitmap bitmap;
    Paint(views::PaintInfo::CreateRootPaintInfo(
        ui::CanvasPainter(&bitmap, size(), scale,
                          /*clear_color=*/SK_ColorTRANSPARENT, is_pixel_canvas)
            .context(),
        size()));
    return gfx::ImageSkia(gfx::ImageSkiaRep(bitmap, scale));
  }

 private:
  void InitLayout(const std::vector<const HoldingSpaceItem*>& items) {
    SetLayoutManager(
        std::make_unique<DragImageLayoutManager>(kDragImageViewChildOffset));

    const bool contains_only_screenshots = std::all_of(
        items.begin(), items.end(), [](const HoldingSpaceItem* item) {
          return item->type() == HoldingSpaceItem::Type::kScreenshot;
        });

    // TODO(crbug.com/1139113): Limit number of items and add overflow badge.
    for (const HoldingSpaceItem* item : items) {
      if (contains_only_screenshots)
        AddChildView(std::make_unique<DragImageItemScreenshotView>(item));
      else
        AddChildView(std::make_unique<DragImageItemChipView>(item));
    }
  }
};

}  // namespace

// Utilities -------------------------------------------------------------------

gfx::ImageSkia CreateDragImage(
    const std::vector<const HoldingSpaceItemView*>& views) {
  if (views.empty())
    return gfx::ImageSkia();

  const views::Widget* widget = views[0]->GetWidget();
  const float scale = views::ScaleFactorForDragFromWidget(widget);
  const bool is_pixel_canvas = widget->GetCompositor()->is_pixel_canvas();

  DragImageView drag_image_view(GetHoldingSpaceItems(views));
  drag_image_view.SetSize(drag_image_view.GetPreferredSize());
  return drag_image_view.PaintToImageSkia(scale, is_pixel_canvas);
}

}  // namespace holding_space_util
}  // namespace ash
