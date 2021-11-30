// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/icon_button.h"

#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {
namespace {

constexpr int kSmallButtonSize = 32;
constexpr int kMediumButtonSize = 36;
constexpr int kLargeButtonSize = 48;

// Icon size of the IconButton. Though the button has different sizes, the icon
// inside will be kept the same size.
constexpr int kIconSize = 20;

int GetButtonSizeOnType(IconButton::Type type) {
  switch (type) {
    case IconButton::Type::kSmall:
    case IconButton::Type::kSmallFloating:
      return kSmallButtonSize;
    case IconButton::Type::kMedium:
    case IconButton::Type::kMediumFloating:
      return kMediumButtonSize;
    case IconButton::Type::kLarge:
    case IconButton::Type::kLargeFloating:
      return kLargeButtonSize;
  }
}

bool IsFloatingIconButton(IconButton::Type type) {
  return type == IconButton::Type::kSmallFloating ||
         type == IconButton::Type::kMediumFloating ||
         type == IconButton::Type::kLargeFloating;
}

}  // namespace

IconButton::IconButton(PressedCallback callback,
                       IconButton::Type type,
                       const gfx::VectorIcon& icon,
                       int accessible_name_id)
    : views::ImageButton(std::move(callback)), type_(type), icon_(icon) {
  const int button_size = GetButtonSizeOnType(type);
  SetPreferredSize(gfx::Size(button_size, button_size));

  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
  if (accessible_name_id)
    SetTooltipText(l10n_util::GetStringUTF16(accessible_name_id));
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false);
  views::InstallCircleHighlightPathGenerator(this);
}

IconButton::~IconButton() = default;

void IconButton::PaintButtonContents(gfx::Canvas* canvas) {
  if (!IsFloatingIconButton(type_)) {
    const gfx::Rect rect(GetContentsBounds());
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(AshColorProvider::Get()->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawCircle(gfx::PointF(rect.CenterPoint()), rect.width() / 2,
                       flags);
  }

  views::ImageButton::PaintButtonContents(canvas);
}

void IconButton::OnThemeChanged() {
  views::ImageButton::OnThemeChanged();

  auto* color_provider = AshColorProvider::Get();
  const SkColor icon_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);

  // Skip repainting if the incoming icon is the same as the current icon. If
  // the icon has been painted before, |gfx::CreateVectorIcon()| will simply
  // grab the ImageSkia from a cache, so it will be cheap. Note that this
  // assumes that toggled/disabled images changes at the same time as the normal
  // image, which it currently does.
  const gfx::ImageSkia new_normal_image =
      gfx::CreateVectorIcon(icon_, kIconSize, icon_color);
  const gfx::ImageSkia& old_normal_image =
      GetImage(views::Button::STATE_NORMAL);
  if (!new_normal_image.isNull() && !old_normal_image.isNull() &&
      new_normal_image.BackedBySameObjectAs(old_normal_image)) {
    return;
  }

  SetImage(views::Button::STATE_NORMAL, new_normal_image);
  SetImage(
      views::Button::STATE_DISABLED,
      gfx::CreateVectorIcon(icon_, kIconSize,
                            AshColorProvider::GetDisabledColor(icon_color)));

  views::FocusRing::Get(this)->SetColor(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kFocusRingColor));
}

BEGIN_METADATA(IconButton, views::ImageButton)
END_METADATA

}  // namespace ash
