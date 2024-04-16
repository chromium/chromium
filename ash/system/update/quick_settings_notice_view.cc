// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/update/quick_settings_notice_view.h"

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "base/functional/bind.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kButtonHeight = 32;
constexpr int kImageLabelSpacing = 8;
constexpr int kIconSize = 20;
constexpr int kRoundedCornerRadius = 16;
constexpr float kButtonStrokeWidth = 1.0f;

// Wrapper function that records the quick settings button being pressed,
// then runs the provided callback.
void PressedCallbackWrapper(views::Button::PressedCallback::Callback callback,
                            QsButtonCatalogName catalog_name,
                            const ui::Event& event) {
  quick_settings_metrics_util::RecordQsButtonActivated(catalog_name);
  callback.Run(event);
}

}  // namespace

QuickSettingsNoticeView::QuickSettingsNoticeView(
    ash::ViewID view_id,
    QsButtonCatalogName catalog_name,
    int text_id,
    const gfx::VectorIcon& icon,
    views::Button::PressedCallback::Callback callback)
    : views::LabelButton(
          base::BindRepeating(&PressedCallbackWrapper, callback, catalog_name),
          l10n_util::GetStringUTF16(text_id)),
      text_id_(text_id) {
  SetID(view_id);
  SetMinSize(gfx::Size(0, kButtonHeight));
  SetImageLabelSpacing(kImageLabelSpacing);

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);

  views::InkDrop::Get(this)->SetBaseColorId(kColorAshInkDropOpaqueColor);
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(
                    icon, cros_tokens::kCrosSysOnSurfaceVariant, kIconSize));
  SetEnabledTextColorIds(cros_tokens::kCrosSysOnSurfaceVariant);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2, *label());
  SetHorizontalAlignment(gfx::ALIGN_CENTER);

  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(0),
                                                kRoundedCornerRadius);

  SetInstallFocusRingOnFocus(true);
  views::FocusRing::Get(this)->SetColorId(
      static_cast<ui::ColorId>(cros_tokens::kCrosSysFocusRing));
}

QuickSettingsNoticeView::~QuickSettingsNoticeView() = default;

void QuickSettingsNoticeView::PaintButtonContents(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  gfx::RectF bounds(GetLocalBounds());
  flags.setColor(GetColorProvider()->GetColor(cros_tokens::kCrosSysSeparator));
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kButtonStrokeWidth);
  bounds.Inset(kButtonStrokeWidth / 2.0f);

  flags.setAntiAlias(true);
  canvas->DrawPath(
      SkPath().addRoundRect(gfx::RectFToSkRect(bounds), kRoundedCornerRadius,
                            kRoundedCornerRadius),
      flags);
}

void QuickSettingsNoticeView::SetNarrowLayout(bool narrow) {
  label()->SetText(
      l10n_util::GetStringUTF16(narrow ? GetShortTextId() : text_id_));
}

int QuickSettingsNoticeView::GetShortTextId() const {
  return text_id_;
}

BEGIN_METADATA(QuickSettingsNoticeView)
END_METADATA

}  // namespace ash
