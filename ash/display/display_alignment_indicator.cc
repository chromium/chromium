// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_alignment_indicator.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Constants for indicator highlights.
constexpr SkColor kEdgeHighlightColor = gfx::kGoogleBlue600;
constexpr int kHighlightShadowElevation = 2;

// Thickness (and radius) of indicator highlight is dependent on resolution.
// If display has resolution smaller than 1440p, then its thickness is
// |kHighlightRadiusSub1440p|. Otherwise, use |kHighlightRadius1440p|.
constexpr int kHighlightRadiusSub1440p = 4;
constexpr int kHighlightRadius1440p = 6;
constexpr int kHighlightSizeChangeRes = 1440;

// Constants for pill theme.
// White with ~60% opacity.
constexpr SkColor kPillBackgroundColor = SkColorSetARGB(0x99, 0xFF, 0xFF, 0xFF);
constexpr SkColor kPillTextColor = gfx::kGoogleBlue600;
constexpr int kPillBackgroundBlur = 10;

// Constants for pill layout.
constexpr int kPillRadius = 12;
constexpr int kMaxPillWidth = 192;
constexpr int kPillHeight = 32;
constexpr int kTextMarginNormal = 24;
constexpr int kTextMarginElided = 20;
// Distance between the indicator highlight and the pill.
constexpr int kPillMargin = 20;

// Constants for arrow layout.
constexpr int kArrowSize = 28;
constexpr int kArrowHorizontalMargin = 12;
constexpr int kArrowVerticalMargin = (kPillHeight - kArrowSize) / 2;
constexpr int kArrowAllocatedWidth =
    kArrowHorizontalMargin + kArrowSize + kArrowHorizontalMargin;

enum class IndicatorPosition { kTop, kRight, kBottom, kLeft };

// Returns IndicatorPosition given the bounds of an indicator highlight along
// with its corresponding display.
IndicatorPosition GetIndicatorPosition(const display::Display& src_display,
                                       const gfx::Rect& indicator_bounds) {
  const gfx::Point midpoint = src_display.bounds().CenterPoint();

  // Horizontal shared edge (kTop or kBottom)
  if (indicator_bounds.width() > indicator_bounds.height()) {
    return (indicator_bounds.y() < midpoint.y()) ? IndicatorPosition::kTop
                                                 : IndicatorPosition::kBottom;
  }
  // Vertical shared edge (kLeft or kRight)
  return (indicator_bounds.x() < midpoint.x()) ? IndicatorPosition::kLeft
                                               : IndicatorPosition::kRight;
}

// Indicator thickness is dependent on display resolution.
int GetIndicatorThickness(const gfx::Size& display_size) {
  return std::min(display_size.width(), display_size.height()) >
                 kHighlightSizeChangeRes
             ? kHighlightRadius1440p
             : kHighlightRadiusSub1440p;
}

// Adjust the indicator bounds to the correct thickness depending on the
// resolution of |display|.
void AdjustIndicatorBounds(const display::Display& display,
                           gfx::Rect* out_indicator_bounds) {
  const int indicator_thickness = GetIndicatorThickness(display.size());

  // Apply the new thickness to the indicator.
  if (out_indicator_bounds->height() > out_indicator_bounds->width())
    out_indicator_bounds->set_width(indicator_thickness);
  else
    out_indicator_bounds->set_height(indicator_thickness);

  // Create enough space for the full indicator on the x and y axis.
  const gfx::Point display_bottom_right = display.bounds().bottom_right();
  if (out_indicator_bounds->x() == (display_bottom_right.x() - 1))
    out_indicator_bounds->set_x(display_bottom_right.x() - indicator_thickness);
  else if (out_indicator_bounds->y() == (display_bottom_right.y() - 1))
    out_indicator_bounds->set_y(display_bottom_right.y() - indicator_thickness);
}

// Returns the pill's origin based on |pill_size| and the indicator's
// |thickened_bounds|.
gfx::Point GetPillOrigin(const gfx::Size& pill_size,
                         IndicatorPosition src_position,
                         const gfx::Rect& thickened_bounds) {
  gfx::Point pill_origin;
  switch (src_position) {
    case IndicatorPosition::kLeft:
      pill_origin = thickened_bounds.right_center();
      pill_origin.Offset(kPillMargin, -1 * kPillHeight / 2);
      break;
    case IndicatorPosition::kRight:
      pill_origin = thickened_bounds.left_center();
      pill_origin.Offset(-1 * kPillMargin - pill_size.width(),
                         -1 * kPillHeight / 2);
      break;
    case IndicatorPosition::kTop:
      pill_origin = thickened_bounds.bottom_center();
      pill_origin.Offset(-1 * pill_size.width() / 2, kPillMargin);
      break;
    case IndicatorPosition::kBottom:
      pill_origin = thickened_bounds.top_center();
      pill_origin.Offset(-1 * pill_size.width() / 2,
                         -1 * kPillMargin - kPillHeight);
      break;
  }

  return pill_origin;
}

views::Widget::InitParams CreateInitParams(int64_t display_id,
                                           const std::string& target_name) {
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);

  aura::Window* root =
      Shell::GetRootWindowControllerWithDisplayId(display_id)->GetRootWindow();

  params.parent = Shell::GetContainer(root, kShellWindowId_OverlayContainer);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.accept_events = false;
  params.name = target_name;

  return params;
}

}  // namespace

// -----------------------------------------------------------------------------
// IndicatorHighlightView:
// View for the indicator highlight that renders on a shared edge of a given
// display.
class IndicatorHighlightView : public views::View {
  METADATA_HEADER(IndicatorHighlightView, views::View)
 public:
  explicit IndicatorHighlightView(const display::Display& display)
      // Corner radius is the same as edge thickness.
      : corner_radius_(GetIndicatorThickness(display.size())) {
    SetPaintToLayer(ui::LAYER_TEXTURED);

    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetIsFastRoundedCorner(true);
    SetBackground(views::CreateSolidBackground(kEdgeHighlightColor));
  }

  IndicatorHighlightView(const IndicatorHighlightView&) = delete;
  IndicatorHighlightView& operator=(const IndicatorHighlightView&) = delete;
  ~IndicatorHighlightView() override = default;

  // Sets which corners should be rounded depending on the position of the
  // indicator edge.
  void SetPosition(IndicatorPosition position) {
    gfx::RoundedCornersF corners;

    switch (position) {
      case IndicatorPosition::kLeft:
        corners = {0, corner_radius_, corner_radius_, 0};
        break;
      case IndicatorPosition::kRight:
        corners = {corner_radius_, 0, 0, corner_radius_};
        break;
      case IndicatorPosition::kTop:
        corners = {0, 0, corner_radius_, corner_radius_};
        break;
      case IndicatorPosition::kBottom:
        corners = {corner_radius_, corner_radius_, 0, 0};
        break;
    }

    layer()->SetRoundedCornerRadius(corners);
  }

 private:
  // Radius for the rounded rectangle highlight. Determined by display
  // resolution.
  float corner_radius_;
};

BEGIN_METADATA(IndicatorHighlightView)
END_METADATA

// -----------------------------------------------------------------------------
// IndicatorPillView:
// View for the pill with an arrow pointing to an indicator highlight and name
// of the target display.
class IndicatorPillView : public views::View {
  METADATA_HEADER(IndicatorPillView, views::View)
 public:
  explicit IndicatorPillView(const std::u16string& text)
      :  // TODO(1070352): Replace current placeholder arrow in
         // IndicatorPillView
        icon_(AddChildView(std::make_unique<views::ImageView>())),
        text_label_(AddChildView(std::make_unique<views::Label>())),
        arrow_image_(
            CreateVectorIcon(kLockScreenArrowIcon, gfx::kGoogleBlue600)) {
    SetPaintToLayer(ui::LAYER_TEXTURED);

    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetIsFastRoundedCorner(true);
    layer()->SetBackgroundBlur(kPillBackgroundBlur);
    layer()->SetRoundedCornerRadius(gfx::RoundedCornersF{kPillRadius});

    SetBackground(
        views::CreateRoundedRectBackground(kPillBackgroundColor, kPillRadius));

    text_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    text_label_->SetEnabledColor(kPillTextColor);
    text_label_->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);
    text_label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
    text_label_->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_MIDDLE);

    text_label_->SetText(text);

    icon_->SetImage(arrow_image_);
  }

  IndicatorPillView(const IndicatorPillView&) = delete;
  IndicatorPillView& operator=(const IndicatorPillView&) = delete;
  ~IndicatorPillView() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    // Pill is laid out as:
    // ( | | text )
    // Has max width of kMaxPillWidth.

    const int text_width = text_label_->CalculatePreferredSize({}).width();
    const int width = kArrowAllocatedWidth + text_width + kTextMarginNormal;

    return gfx::Size(std::min(width, kMaxPillWidth), kPillHeight);
  }

  // views::View:
  void Layout(PassKey) override {
    icon_->SetImageSize(gfx::Size(kArrowSize, kArrowSize));

    // IndicatorPosition::kRight is a special case for layout as it is the only
    // time where the arrow is on the right of the text instead of the usual
    // left.
    const int local_width = GetLocalBounds().width();
    int icon_x = position_ == IndicatorPosition::kRight
                     ? local_width - kArrowHorizontalMargin - kArrowSize
                     : kArrowHorizontalMargin;

    icon_->SetBoundsRect(
        gfx::Rect(icon_x, kArrowVerticalMargin, kArrowSize, kArrowSize));

    // If width of the pill is equal or greater than the max pill width, then
    // text is elided and thus side margin must be reduced.
    const int side_margin = CalculatePreferredSize({}).width() >= kMaxPillWidth
                                ? kTextMarginElided
                                : kTextMarginNormal;

    const int text_label_width =
        local_width - kArrowAllocatedWidth - side_margin;

    const int text_label_x = position_ == IndicatorPosition::kRight
                                 ? side_margin
                                 : kArrowAllocatedWidth;

    text_label_->SetBoundsRect(
        gfx::Rect(text_label_x, 0, text_label_width, kPillHeight));
  }

  // Rotates the arrow depending on indicator highlight's position on-screen.
  void SetPosition(IndicatorPosition position) {
    if (position_ == position)
      return;

    position_ = position;

    switch (position) {
      case IndicatorPosition::kLeft:
        icon_->SetImage(gfx::ImageSkiaOperations::CreateRotatedImage(
            arrow_image_, SkBitmapOperations::ROTATION_180_CW));
        return;
      case IndicatorPosition::kRight:
        // |arrow_image_| points to right by default; no rotation required.
        icon_->SetImage(arrow_image_);
        return;
      case IndicatorPosition::kTop:
        icon_->SetImage(gfx::ImageSkiaOperations::CreateRotatedImage(
            arrow_image_, SkBitmapOperations::ROTATION_270_CW));
        return;
      case IndicatorPosition::kBottom:
        icon_->SetImage(gfx::ImageSkiaOperations::CreateRotatedImage(
            arrow_image_, SkBitmapOperations::ROTATION_90_CW));
        return;
    }
  }

 private:
  // Possibly rotated image of an arrow based on |vector_icon_|.
  raw_ptr<views::ImageView> icon_ = nullptr;  // NOT OWNED
  // Label containing name of target display in the pill.
  raw_ptr<views::Label> text_label_ = nullptr;  // NOT OWNED
  gfx::ImageSkia arrow_image_;
  // The side of the display indicator is postioned on. Used to determine arrow
  // direction and placement.
  IndicatorPosition position_ = IndicatorPosition::kRight;
};

BEGIN_METADATA(IndicatorPillView)
END_METADATA

// -----------------------------------------------------------------------------
// DisplayAlignmentIndicator:

// static
std::unique_ptr<DisplayAlignmentIndicator> DisplayAlignmentIndicator::Create(
    const display::Display& src_display,
    const gfx::Rect& bounds) {
  // Using `new` to access a non-public constructor.
  return base::WrapUnique(
      new DisplayAlignmentIndicator(src_display, bounds, ""));
}

// static
std::unique_ptr<DisplayAlignmentIndicator>
DisplayAlignmentIndicator::CreateWithPill(const display::Display& src_display,
                                          const gfx::Rect& bounds,
                                          const std::string& target_name) {
  // Using `new` to access a non-public constructor.
  return base::WrapUnique(
      new DisplayAlignmentIndicator(src_display, bounds, target_name));
}

DisplayAlignmentIndicator::DisplayAlignmentIndicator(
    const display::Display& src_display,
    const gfx::Rect& bounds,
    const std::string& target_name)
    : display_id_(src_display.id()) {
  gfx::Rect thickened_bounds = bounds;
  AdjustIndicatorBounds(src_display, &thickened_bounds);

  views::Widget::InitParams indicator_widget_params =
      CreateInitParams(src_display.id(), "IndicatorHighlight");
  indicator_widget_params.shadow_elevation = kHighlightShadowElevation;

  indicator_widget_.Init(std::move(indicator_widget_params));
  indicator_widget_.SetVisibilityChangedAnimationsEnabled(false);
  indicator_view_ = indicator_widget_.SetContentsView(
      std::make_unique<IndicatorHighlightView>(src_display));
  indicator_widget_.SetBounds(thickened_bounds);

  const IndicatorPosition indicator_position =
      GetIndicatorPosition(src_display, thickened_bounds);
  indicator_view_->SetPosition(indicator_position);

  // Only create IndicatorPillView when |target_name| is specified.
  if (!target_name.empty()) {
    pill_widget_ = std::make_unique<views::Widget>();
    pill_widget_->Init(CreateInitParams(src_display.id(), "IndicatorPill"));
    pill_widget_->SetVisibilityChangedAnimationsEnabled(false);
    pill_view_ = pill_widget_->SetContentsView(
        std::make_unique<IndicatorPillView>(base::UTF8ToUTF16(target_name)));
    pill_view_->SetPosition(indicator_position);

    gfx::Size pill_size = pill_view_->GetPreferredSize();
    gfx::Rect pill_bounds = gfx::Rect(
        GetPillOrigin(pill_size, indicator_position, thickened_bounds),
        pill_size);
    pill_widget_->SetBounds(pill_bounds);
  }

  Show();
}

DisplayAlignmentIndicator::~DisplayAlignmentIndicator() = default;

void DisplayAlignmentIndicator::Show() {
  indicator_widget_.Show();

  if (pill_widget_)
    pill_widget_->Show();
}

void DisplayAlignmentIndicator::Hide() {
  indicator_widget_.Hide();

  if (pill_widget_)
    pill_widget_->Hide();
}

void DisplayAlignmentIndicator::Update(const display::Display& display,
                                       gfx::Rect bounds) {
  DCHECK(!pill_widget_);

  AdjustIndicatorBounds(display, &bounds);
  const IndicatorPosition src_direction = GetIndicatorPosition(display, bounds);
  indicator_view_->SetPosition(src_direction);
  indicator_widget_.SetBounds(bounds);
}

}  // namespace ash
