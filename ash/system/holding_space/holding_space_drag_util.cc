// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_drag_util.h"

#include <memory>

#include "ash/bubble/bubble_utils.h"
#include "ash/drag_drop/drag_drop_util.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/typography.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "base/containers/adapters.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/shadow_util.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/drag_utils.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace holding_space_util {

namespace {

// Appearance.
constexpr int kDragImageItemViewCornerRadius = 8;
constexpr int kDragImageItemChipViewIconSize = 24;
constexpr auto kDragImageItemChipViewInsets = gfx::Insets::TLBR(8, 8, 8, 12);
constexpr gfx::Size kDragImageItemChipViewPreferredSize(160, 40);
constexpr int kDragImageItemChipViewSpacing = 8;
constexpr gfx::Size kDragImageItemScreenCaptureViewPreferredSize(104, 80);
constexpr auto kDragImageOverflowBadgeInsets = gfx::Insets::VH(0, 8);
constexpr gfx::Size kDragImageOverflowBadgeMinimumSize(24, 24);
constexpr int kDragImageViewChildOffset = 8;

// The maximum number of items to paint to the drag image. If more items exist
// they will be represented by an overflow badge.
constexpr size_t kDragImageViewMaxItemsToPaint = 2;

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

  std::vector<raw_ptr<views::View, VectorExperimental>>
  GetChildViewsInPaintOrder(const views::View* host) const override {
    // Paint `children` in reverse order so that earlier views paint at a higher
    // z-index than later views, like a deck of cards with the first `child`
    // stacked on top.
    std::vector<raw_ptr<views::View, VectorExperimental>> children;
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
  METADATA_HEADER(DragImageItemView, views::View)

 public:
  DragImageItemView(const DragImageItemView&) = delete;
  DragImageItemView& operator=(const DragImageItemView&) = delete;
  ~DragImageItemView() override = default;

 protected:
  explicit DragImageItemView(const ui::ColorProvider* color_provider)
      : color_provider_(color_provider) {}

  const ui::ColorProvider* color_provider() const { return color_provider_; }

  // views::View:
  gfx::Insets GetInsets() const final {
    // Add insets to accommodate the shadow so that the view's content will be
    // laid out within the appropriate shadow margins.
    return gfx::Insets(-gfx::ShadowValue::GetMargin(GetShadowDetails().values));
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    // NOTE: The contents bounds are shrunk by a single pixel to avoid
    // painting the background outside content bounds as might otherwise occur
    // due to pixel rounding. Failure to do so could result in paint artifacts.
    gfx::RectF bounds(GetContentsBounds());
    bounds.Inset(gfx::InsetsF(0.5f));

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(
        color_provider_->GetColor(drag_drop::kDragImageBackgroundColor));
    flags.setLooper(gfx::CreateShadowDrawLooper(GetShadowDetails().values));
    canvas->DrawRoundRect(bounds, kDragImageItemViewCornerRadius, flags);
  }

 private:
  const gfx::ShadowDetails& GetShadowDetails() const {
    return drag_drop::GetDragImageShadowDetails(kDragImageItemViewCornerRadius);
  }

  const raw_ptr<const ui::ColorProvider> color_provider_;
};

BEGIN_METADATA(DragImageItemView)
END_METADATA

// DragImageItemChipView -------------------------------------------------------

// A `DragImageItemView` which represents a single holding space `item` as a
// chip in the drag image for a collection of holding space item views.
class DragImageItemChipView : public DragImageItemView {
  METADATA_HEADER(DragImageItemChipView, DragImageItemView)

 public:
  DragImageItemChipView(const HoldingSpaceItem* item,
                        const ui::ColorProvider* color_provider)
      : DragImageItemView(color_provider) {
    InitLayout(item);
  }

 private:
  void InitLayout(const HoldingSpaceItem* item) {
    // NOTE: Enlarge `preferred_size` to accommodate the view's shadow.
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

    // NOTE: The view's background is white when the dark/light mode feature is
    // disabled. Otherwise, the view's background depends on theming.
    icon->SetImage(item->image().GetImageSkia(
        icon->GetPreferredSize(),
        /*dark_background=*/DarkLightModeControllerImpl::Get()
            ->IsDarkModeEnabled()));

    // Label.
    auto* label = AddChildView(bubble_utils::CreateLabel(
        TypographyToken::kCrosBody2, item->GetText()));
    // Label created via `bubble_utils::CreateLabel()` has an enabled color id,
    // which is resolved when the label is added to the views hierarchy. But
    // `this` is never added to widget, enabled color id will never be resolved.
    // Thus we need to manually resolve it and set the color as the enabled
    // color for the label.
    if (auto enabled_color_id = label->GetEnabledColorId()) {
      label->SetEnabledColor(color_provider()->GetColor(*enabled_color_id));
    }

    label->SetElideBehavior(gfx::ElideBehavior::ELIDE_MIDDLE);
    label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    layout->SetFlexForView(label, 1);
  }
};

BEGIN_METADATA(DragImageItemChipView)
END_METADATA

// DragImageItemScreenCaptureView ----------------------------------------------

// A `DragImageItemView` which represents a single holding space screen capture
// `item` in the drag image for a collection of holding space item views.
class DragImageItemScreenCaptureView : public DragImageItemView {
  METADATA_HEADER(DragImageItemScreenCaptureView, DragImageItemView)

 public:
  DragImageItemScreenCaptureView(const HoldingSpaceItem* item,
                                 const ui::ColorProvider* color_provider)
      : DragImageItemView(color_provider) {
    DCHECK(HoldingSpaceItem::IsScreenCaptureType(item->type()));
    InitLayout(item);
  }

 private:
  void InitLayout(const HoldingSpaceItem* item) {
    // NOTE: Enlarge `preferred_size` to accommodate the view's shadow.
    gfx::Size preferred_size(kDragImageItemScreenCaptureViewPreferredSize);
    preferred_size.Enlarge(GetInsets().width(), GetInsets().height());
    SetPreferredSize(preferred_size);

    // Layout.
    SetLayoutManager(std::make_unique<views::FillLayout>());

    // Image.
    auto* image = AddChildView(std::make_unique<RoundedImageView>(
        kDragImageItemViewCornerRadius, RoundedImageView::Alignment::kCenter));
    image->SetPreferredSize(kDragImageItemScreenCaptureViewPreferredSize);

    // NOTE: The view's background is white when the dark/light mode feature is
    // disabled. Otherwise, the view's background depends on theming.
    image->SetImage(item->image().GetImageSkia(
        image->GetPreferredSize(),
        /*dark_background=*/DarkLightModeControllerImpl::Get()
            ->IsDarkModeEnabled()));
  }
};

BEGIN_METADATA(DragImageItemScreenCaptureView)
END_METADATA

// DragImageOverflowBadge ------------------------------------------------------

// A `views::View` which indicates the number of items being dragged in the
// drag image for a collection of holding space items. This view is only created
// if the number of dragged items is > `kDragImageViewMaxItemsToPaint`.
class DragImageOverflowBadge : public views::View {
  METADATA_HEADER(DragImageOverflowBadge, views::View)

 public:
  DragImageOverflowBadge(size_t count, const ui::ColorProvider* color_provider)
      : color_provider_(color_provider) {
    DCHECK_GT(count, kDragImageViewMaxItemsToPaint);
    InitLayout(count);
  }

  DragImageOverflowBadge(const DragImageOverflowBadge&) = delete;
  DragImageOverflowBadge& operator=(const DragImageOverflowBadge&) = delete;
  ~DragImageOverflowBadge() override = default;

 private:
  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    gfx::Size preferred_size =
        views::View::CalculatePreferredSize(available_size);
    preferred_size.SetToMax(kDragImageOverflowBadgeMinimumSize);
    return preferred_size;
  }

  void InitLayout(size_t count) {
    // Background.
    // NOTE: `this` is never added to a widget, so background color must be
    // explicitly resolved with the `color_provider_`.
    SetBackground(views::CreateRoundedRectBackground(
        color_provider_->GetColor(ui::kColorAshFocusRing),
        /*radius=*/kDragImageOverflowBadgeMinimumSize.height() / 2));

    // Layout.
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        kDragImageOverflowBadgeInsets));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);

    // Label.
    // NOTE: `this` is never added to a widget, so enabled color must be
    // explicitly resolved with the `color_provider_`.
    auto* label =
        AddChildView(bubble_utils::CreateLabel(TypographyToken::kCrosButton1));
    label->SetEnabledColor(
        color_provider_->GetColor(kColorAshDragImageOverflowBadgeTextColor));
    label->SetText(base::UTF8ToUTF16(base::NumberToString(count)));
  }

  const raw_ptr<const ui::ColorProvider> color_provider_;
};

BEGIN_METADATA(DragImageOverflowBadge)
END_METADATA

// DragImageView ---------------------------------------------------------------

// A `views::View` for use as a drag image for a collection of holding space
// item `views`. This view expects to be painted to an `SkBitmap`.
class DragImageView : public views::View {
  METADATA_HEADER(DragImageView, views::View)

 public:
  DragImageView(const std::vector<const HoldingSpaceItem*>& items,
                const ui::ColorProvider* color_provider)
      : color_provider_(color_provider) {
    InitLayout(items);
  }

  DragImageView(const DragImageView&) = delete;
  DragImageView& operator=(const DragImageView&) = delete;
  ~DragImageView() override = default;

  // Paints this view to a `gfx::ImageSkia` for use as a drag image.
  gfx::ImageSkia GetDragImage(float scale, bool is_pixel_canvas) {
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
    return gfx::ImageSkia::CreateFromBitmap(bitmap, scale);
  }

  // Returns the drag offset to use when rendering this view as a drag image.
  // This offset will position the cursor directly over the top left hand corner
  // of the first dragged item (or flipped for RTL).
  gfx::Vector2d GetDragOffset() const {
    DCHECK(first_drag_image_item_view_);
    const gfx::Rect contents_bounds =
        first_drag_image_item_view_->GetContentsBounds();

    // Use the contents origin of the first dragged item instead of its local
    // bounds origin to exclude the region reserved for its shadow margins.
    gfx::Point contents_origin = contents_bounds.origin();
    views::View::ConvertPointToTarget(first_drag_image_item_view_->parent(),
                                      /*target=*/this, &contents_origin);

    gfx::Vector2d drag_offset = contents_origin.OffsetFromOrigin();

    // In RTL, its necessary to offset by the contents width of the first
    // dragged item so that the cursor is positioned over its top right hand
    // corner. Again, contents width is used instead of local bounds width to
    // exclude shadow margins.
    if (base::i18n::IsRTL())
      drag_offset += gfx::Vector2d(contents_bounds.width(), 0);

    return drag_offset;
  }

 private:
  // views::View:
  gfx::Insets GetInsets() const override {
    if (!drag_image_overflow_badge_)
      return gfx::Insets();
    // When the number of dragged items is > `kDragImageViewMaxItemsToPaint`,
    // add insets in which to layout `drag_image_overflow_badge_`. Note that
    // because the badge is centered at the top right hand corner of the
    // `first_drag_image_item_view_`, half of the badge will be positioned
    // within contents bounds so only half of the badge's preferred `size` needs
    // to be added as insets.
    gfx::Size size = drag_image_overflow_badge_->GetPreferredSize();
    return gfx::Insets::TLBR(size.height() / 2, 0, 0, size.width() / 2);
  }

  void Layout(PassKey) override {
    LayoutSuperclass<views::View>(this);

    if (!drag_image_overflow_badge_)
      return;

    DCHECK(first_drag_image_item_view_);

    // Manually position `drag_image_overflow_badge_` to be centered at the top
    // right hand corner of the `first_drag_image_item_view_`.
    const gfx::Size badge_size = drag_image_overflow_badge_->GetPreferredSize();
    const gfx::Point badge_origin =
        first_drag_image_item_view_->GetContentsBounds().top_right() -
        gfx::Vector2d(badge_size.width() / 2, 0);
    drag_image_overflow_badge_->SetBoundsRect(
        gfx::Rect(badge_origin, badge_size));
  }

  void InitLayout(const std::vector<const HoldingSpaceItem*>& items) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddDragImageItemViews(items);
    AddDragImageOverflowBadge(items.size());
  }

  void AddDragImageItemViews(
      const std::vector<const HoldingSpaceItem*>& items) {
    auto* container = AddChildView(std::make_unique<views::View>());
    container->SetLayoutManager(
        std::make_unique<DragImageLayoutManager>(kDragImageViewChildOffset));

    const bool contains_only_screen_captures =
        base::ranges::all_of(items, [](const HoldingSpaceItem* item) {
          return HoldingSpaceItem::IsScreenCaptureType(item->type());
        });

    // Show at most `kDragImageViewMaxItemsToPaint` items in the drag image. If
    // more items exist, `drag_image_overflow_badge_` will be added to indicate
    // the total number of dragged items.
    const size_t count = std::min(items.size(), kDragImageViewMaxItemsToPaint);
    for (size_t i = 0; i < count; ++i) {
      if (contains_only_screen_captures) {
        container->AddChildView(
            std::make_unique<DragImageItemScreenCaptureView>(items[i],
                                                             color_provider_));
      } else {
        container->AddChildView(
            std::make_unique<DragImageItemChipView>(items[i], color_provider_));
      }
    }

    // Cache the first `DragImageItemView` so `drag_image_overflow_badge_` can
    // be relatively positioned if `kDragImageViewMaxItemsToPaint` is met.
    DCHECK(!container->children().empty());
    first_drag_image_item_view_ = container->children()[0].get();
  }

  void AddDragImageOverflowBadge(size_t count) {
    if (count <= kDragImageViewMaxItemsToPaint)
      return;

    drag_image_overflow_badge_ = AddChildView(
        std::make_unique<DragImageOverflowBadge>(count, color_provider_));

    // `drag_image_overflow_badge_` is manually positioned relative to the
    // `first_drag_image_item_view_`.
    drag_image_overflow_badge_->SetProperty(views::kViewIgnoredByLayoutKey,
                                            true);
  }

  const raw_ptr<const ui::ColorProvider> color_provider_;
  raw_ptr<views::View> first_drag_image_item_view_ = nullptr;
  raw_ptr<views::View> drag_image_overflow_badge_ = nullptr;
};

BEGIN_METADATA(DragImageView)
END_METADATA

}  // namespace

// Utilities -------------------------------------------------------------------

void CreateDragImage(const std::vector<const HoldingSpaceItemView*>& views,
                     gfx::ImageSkia* drag_image,
                     gfx::Vector2d* drag_offset,
                     const ui::ColorProvider* color_provider) {
  if (views.empty()) {
    *drag_image = gfx::ImageSkia();
    *drag_offset = gfx::Vector2d();
    return;
  }

  const views::Widget* widget = views[0]->GetWidget();
  const float scale = views::ScaleFactorForDragFromWidget(widget);
  const bool is_pixel_canvas = widget->GetCompositor()->is_pixel_canvas();

  DragImageView drag_image_view(GetHoldingSpaceItems(views), color_provider);
  drag_image_view.SetSize(drag_image_view.GetPreferredSize());

  *drag_image = drag_image_view.GetDragImage(scale, is_pixel_canvas);
  *drag_offset = drag_image_view.GetDragOffset();
}

}  // namespace holding_space_util
}  // namespace ash
