// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/nudge.h"

#include <memory>

#include "ash/bubble/bubble_utils.h"
#include "ash/style/typography.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/nudge_view.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace arc::input_overlay {

namespace {

constexpr int kMaxWidth = 296;
constexpr int kHeight = 36;
constexpr int kHorizontalBorder = 16;
constexpr int kVerticalBorder = 8;
constexpr int kOverflowCornerRadius = 16;

constexpr int kDotContainerSize = 16;
constexpr int kDotDiameter = 8;
constexpr int kDotLeftMargin = 32;

constexpr int kIconSize = 20;
constexpr int kIconTextSpace = 12;

class Dot : public views::View {
 public:
  explicit Dot(ui::ColorId color_id) : color_id_(color_id) {}

  Dot(const Dot&) = delete;
  Dot& operator=(const Dot&) = delete;
  ~Dot() override = default;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(GetColorProvider()->GetColor(color_id_));
    canvas->DrawCircle(gfx::Point(kDotContainerSize / 2, kDotContainerSize / 2),
                       kDotDiameter / 2, flags);
  }

  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kDotContainerSize, kDotContainerSize);
  }

 private:
  const ui::ColorId color_id_;
};

}  // namespace

Nudge::Nudge(DisplayOverlayController* controller,
             views::View* anchor_view,
             const std::u16string& text)
    : controller_(controller), anchor_view_(anchor_view) {
  SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  auto* dot = AddChildView(std::make_unique<Dot>(cros_tokens::kCrosSysPrimary));
  dot->SetProperty(views::kMarginsKey,
                   gfx::Insets::TLBR(0, kDotLeftMargin, 0, 0));
  dot->SizeToPreferredSize();

  auto* content_container = AddChildView(std::make_unique<views::View>());
  content_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      /*inside_border_insets=*/
      gfx::Insets::VH(kVerticalBorder, kHorizontalBorder),
      /*between_child_spacing=*/kIconTextSpace));
  content_container->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          kTipIcon, cros_tokens::kCrosSysOnPrimary, kIconSize)));

  auto* label = content_container->AddChildView(ash::bubble_utils::CreateLabel(
      ash::TypographyToken::kCrosBody1, text, cros_tokens::kCrosSysOnPrimary));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  int max_text_width =
      kMaxWidth - kIconSize - 2 * kHorizontalBorder - kIconTextSpace;
  label->SetMaximumWidth(max_text_width);
  label->SetLineHeight(kHeight - 2 * kVerticalBorder);
  content_container->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysPrimary,
      (label->GetRequiredLines() > 1u ? kOverflowCornerRadius : kHeight / 2)));

  auto* anchor_widget = anchor_view_->GetWidget();
  DCHECK(anchor_widget);
  anchor_widget->AddObserver(this);
}

Nudge::~Nudge() {
  auto* anchor_widget = anchor_view_->GetWidget();
  DCHECK(anchor_widget);
  anchor_widget->RemoveObserver(this);
}

void Nudge::OnWidgetClosing(views::Widget* widget) {
  controller_->RemoveNudgeWidget(widget);
}

void Nudge::OnWidgetBoundsChanged(views::Widget* widget,
                                  const gfx::Rect& new_bounds) {
  UpdateBounds();
}

void Nudge::VisibilityChanged(views::View* starting_from, bool is_visible) {
  if (is_visible) {
    UpdateBounds();
  }
}

void Nudge::UpdateBounds() {
  auto* widget = GetWidget();
  DCHECK(widget);

  auto anchor_point = anchor_view_->GetBoundsInScreen().bottom_left();
  anchor_point.Offset(anchor_view_->bounds().width() / 2, 0);
  anchor_point.Offset(-kDotLeftMargin - kDotContainerSize / 2, 0);
  widget->SetBounds(gfx::Rect(anchor_point, GetPreferredSize()));
  widget->StackAtTop();
}

}  // namespace arc::input_overlay
