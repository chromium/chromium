// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_toast_style.h"

#include "ash/public/cpp/ash_typography.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/system/toast/toast_overlay.h"
#include "ash/wm/work_area_insets.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// These values are in DIP.
constexpr int kToastCornerRounding = 16;
constexpr int kToastHeight = 32;
constexpr int kToastHorizontalSpacing = 16;
constexpr int kToastMaximumWidth = 512;
constexpr int kToastMinimumWidth = 288;

// The label inside SystemToastStyle, which allows two lines at maximum.
class SystemToastInnerLabel : public views::Label {
 public:
  METADATA_HEADER(SystemToastInnerLabel);

  explicit SystemToastInnerLabel(const std::u16string& label)
      : views::Label(label, CONTEXT_TOAST_OVERLAY) {
    SetHorizontalAlignment(gfx::ALIGN_LEFT);
    SetAutoColorReadabilityEnabled(false);
    SetMultiLine(true);
    SetMaxLines(2);
    SetSubpixelRenderingEnabled(false);

    const int vertical_spacing =
        std::max((kToastHeight - GetPreferredSize().height()) / 2, 0);
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets(vertical_spacing, kToastHorizontalSpacing)));
  }

  SystemToastInnerLabel(const SystemToastInnerLabel&) = delete;
  SystemToastInnerLabel& operator=(const SystemToastInnerLabel&) = delete;
  ~SystemToastInnerLabel() override = default;

 private:
  // views::Label:
  void OnThemeChanged() override {
    views::Label::OnThemeChanged();
    SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
  }
};

BEGIN_METADATA(SystemToastInnerLabel, views::Label)
END_METADATA

}  // namespace

SystemToastStyle::SystemToastStyle(
    base::RepeatingClosure dismiss_callback,
    const std::u16string& text,
    const absl::optional<std::u16string>& dismiss_text,
    const bool is_managed) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(kToastCornerRounding));
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  SetBackground(
      views::CreateSolidBackground(AshColorProvider::Get()->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kTransparent80)));

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  int icon_width = 0;
  if (is_managed) {
    managed_icon_ = AddChildView(std::make_unique<views::ImageView>());
    managed_icon_->SetBorder(views::CreateEmptyBorder(
        gfx::Insets(kToastHorizontalSpacing, kToastHorizontalSpacing,
                    kToastHorizontalSpacing, 0)));
    icon_width =
        managed_icon_->GetPreferredSize().width() + kToastHorizontalSpacing;
  }

  label_ = AddChildView(std::make_unique<SystemToastInnerLabel>(text));
  label_->SetMaximumWidth(GetMaximumSize().width() - icon_width);
  layout->SetFlexForView(label_, 1);

  if (!dismiss_text.has_value())
    return;

  button_ = AddChildView(std::make_unique<PillButton>(
      std::move(dismiss_callback),
      dismiss_text.value().empty()
          ? l10n_util::GetStringUTF16(IDS_ASH_TOAST_DISMISS_BUTTON)
          : dismiss_text.value(),
      PillButton::Type::kIconlessAccentFloating,
      /*icon=*/nullptr));
  button_->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);

  label_->SetMaximumWidth(
      GetMaximumSize().width() - button_->GetPreferredSize().width() -
      icon_width - kToastHorizontalSpacing * 2 - kToastHorizontalSpacing * 2);
}

SystemToastStyle::~SystemToastStyle() = default;

void SystemToastStyle::SetText(const std::u16string& text) {
  label_->SetText(text);
}

gfx::Size SystemToastStyle::GetMaximumSize() const {
  return gfx::Size(kToastMaximumWidth, WorkAreaInsets::ForWindow(
                                           Shell::GetRootWindowForNewWindows())
                                               ->user_work_area_bounds()
                                               .height() -
                                           ToastOverlay::kOffset * 2);
}

gfx::Size SystemToastStyle::GetMinimumSize() const {
  return gfx::Size(kToastMinimumWidth, kToastHeight);
}

void SystemToastStyle::OnThemeChanged() {
  views::View::OnThemeChanged();
  if (!managed_icon_)
    return;

  managed_icon_->SetImage(gfx::CreateVectorIcon(
      kSystemMenuBusinessIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));
}

BEGIN_METADATA(SystemToastStyle, views::View)
END_METADATA

}  // namespace ash
