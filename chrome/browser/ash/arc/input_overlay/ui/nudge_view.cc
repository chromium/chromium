// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/nudge_view.h"

#include <algorithm>
#include <memory>
#include <type_traits>

#include "ash/components/arc/compat_mode/style/arc_color_provider.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/ui/nudge_view.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace arc::input_overlay {

namespace {

// About UI specs.
constexpr int kCornerRadius = 18;
constexpr int kLineHeight = 20;
constexpr int kDotRadius = 8;
constexpr int kDotSizeWithMargin = 36;
constexpr int kVerticalInset = 8;
constexpr int kHorizontalInset = 16;
constexpr int kLeftScreenMargin = 24;
constexpr int kSpaceIconLabel = 12;
constexpr int kIconSize = 20;

class NudgeDotBackground : public views::Background {
 public:
  explicit NudgeDotBackground(SkColor color) { SetNativeControlColor(color); }
  ~NudgeDotBackground() override = default;

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(get_color());
    canvas->DrawCircle(
        gfx::Point(kDotSizeWithMargin / 2, kDotSizeWithMargin / 2), kDotRadius,
        flags);
  }
};

class DotIndicator : public views::View {
  METADATA_HEADER(DotIndicator, views::View)

 public:
  DotIndicator() {
    SetBackground(std::make_unique<NudgeDotBackground>(
        cros_styles::ResolveColor(cros_styles::ColorName::kNudgeBackgroundColor,
                                  IsDarkModeEnabled())));
  }
  ~DotIndicator() override = default;

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(kDotSizeWithMargin, kDotSizeWithMargin);
  }
};

BEGIN_METADATA(DotIndicator)
END_METADATA

class ContentView : public views::LabelButton {
  METADATA_HEADER(ContentView, views::LabelButton)

 public:
  explicit ContentView(int available_width)
      : available_width_(available_width) {
    SetText(
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_SETTINGS_NUDGE_ALPHAV2));
    GetViewAccessibility().SetName(base::UTF8ToUTF16(GetClassName()));
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets::VH(kVerticalInset, kHorizontalInset)));
    SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            kTipIcon,
            cros_styles::ResolveColor(cros_styles::ColorName::kNudgeIconColor,
                                      IsDarkModeEnabled()),
            kIconSize));
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    SetImageLabelSpacing(kSpaceIconLabel);
    SetTextColor(
        views::Button::STATE_NORMAL,
        cros_styles::ResolveColor(cros_styles::ColorName::kNudgeLabelColor,
                                  IsDarkModeEnabled()));
    SetTextColor(
        views::Button::STATE_HOVERED,
        cros_styles::ResolveColor(cros_styles::ColorName::kNudgeLabelColor,
                                  IsDarkModeEnabled()));
    label()->SetFontList(views::Label::GetDefaultFontList().Derive(
        1, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
    label()->SetMultiLine(true);
    label()->SetLineHeight(kLineHeight);

    auto background_color = cros_styles::ResolveColor(
        cros_styles::ColorName::kNudgeBackgroundColor, IsDarkModeEnabled());
    SetBackground(
        views::CreateRoundedRectBackground(background_color, kCornerRadius));
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    constexpr int extra_width =
        2 * kHorizontalInset + kSpaceIconLabel + kIconSize;
    const views::SizeBound label_available_width =
        std::max<views::SizeBound>(0, available_size.width() - extra_width);

    const int label_width =
        label()
            ->GetPreferredSize(views::SizeBounds(label_available_width, {}))
            .width();
    return views::LabelButton::CalculatePreferredSize(views::SizeBounds(
        std::min(label_width + extra_width, available_width_), {}));
  }

  ~ContentView() override = default;

 private:
  int available_width_ = 0;
};

BEGIN_METADATA(ContentView)
END_METADATA

}  // namespace

// static
NudgeView* NudgeView::Show(views::View* parent, views::View* menu_entry) {
  auto* nudge =
      parent->AddChildView(std::make_unique<NudgeView>(parent, menu_entry));
  nudge->Init();
  return nudge;
}

NudgeView::NudgeView(views::View* parent, views::View* menu_entry)
    : parent_(parent), menu_entry_(menu_entry) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

NudgeView::~NudgeView() = default;

gfx::Size NudgeView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  DCHECK_EQ(2u, children().size());
  int width =
      std::min(children()[0]->GetPreferredSize().width() + kDotSizeWithMargin,
               std::max(0, menu_entry_->origin().x() - kLeftScreenMargin));
  return gfx::Size(width,
                   GetLayoutManager()->GetPreferredHeightForWidth(this, width));
}

void NudgeView::Init() {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // Content view is added first.
  auto* content_view = AddChildView(std::make_unique<ContentView>(
      menu_entry_->origin().x() - kLeftScreenMargin - kDotSizeWithMargin));
  content_view->SizeToPreferredSize();

  auto* dot = AddChildView(std::make_unique<DotIndicator>());
  dot->SizeToPreferredSize();
  dot->SetProperty(views::kMarginsKey,
                   gfx::Insets::TLBR(content_view->height() / 2, 0, 0, 0));

  SizeToPreferredSize();
  int x = std::max(0, menu_entry_->origin().x() - width());
  int y = std::max(0, menu_entry_->origin().y() +
                          (menu_entry_->height() - kDotSizeWithMargin) / 2 -
                          content_view->height() / 2);
  SetPosition(gfx::Point(x, y));
}

BEGIN_METADATA(NudgeView)
END_METADATA

}  // namespace arc::input_overlay
