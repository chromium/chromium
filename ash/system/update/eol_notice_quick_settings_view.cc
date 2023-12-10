// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/update/eol_notice_quick_settings_view.h"

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
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

constexpr float kButtonStrokeWidth = 1.0f;
constexpr int kIconSize = 20;
constexpr int kButtonHeight = 32;

}  // namespace

EolNoticeQuickSettingsView::EolNoticeQuickSettingsView()
    : views::LabelButton(
          base::BindRepeating([](const ui::Event& event) {
            quick_settings_metrics_util::RecordQsButtonActivated(
                QsButtonCatalogName::kEolNoticeButton);

            Shell::Get()->system_tray_model()->client()->ShowEolInfoPage();
          }),
          l10n_util::GetStringUTF16(IDS_ASH_QUICK_SETTINGS_BUBBLE_EOL_NOTICE)) {
  SetID(VIEW_ID_QS_EOL_NOTICE_BUTTON);
  SetMinSize(gfx::Size(0, kButtonHeight));
  SetImageLabelSpacing(8);

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);

  views::InkDrop::Get(this)->SetBaseColorId(kColorAshInkDropOpaqueColor);
  SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(
          kUpgradeIcon, cros_tokens::kCrosSysOnSurfaceVariant, kIconSize));
  SetEnabledTextColorIds(cros_tokens::kCrosSysOnSurfaceVariant);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2, *label());
  SetHorizontalAlignment(gfx::ALIGN_CENTER);

  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(0), 16);

  SetInstallFocusRingOnFocus(true);
  views::FocusRing::Get(this)->SetColorId(
      static_cast<ui::ColorId>(cros_tokens::kCrosSysFocusRing));

  Shell::Get()->system_tray_model()->client()->RecordEolNoticeShown();
}

EolNoticeQuickSettingsView::~EolNoticeQuickSettingsView() = default;

void EolNoticeQuickSettingsView::SetNarrowLayout(bool narrow) {
  label()->SetText(l10n_util::GetStringUTF16(
      narrow ? IDS_ASH_QUICK_SETTINGS_BUBBLE_EOL_NOTICE_SHORT
             : IDS_ASH_QUICK_SETTINGS_BUBBLE_EOL_NOTICE));
}

void EolNoticeQuickSettingsView::PaintButtonContents(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  gfx::RectF bounds(GetLocalBounds());
  flags.setColor(GetColorProvider()->GetColor(cros_tokens::kCrosSysSeparator));
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kButtonStrokeWidth);
  bounds.Inset(kButtonStrokeWidth / 2.0f);

  flags.setAntiAlias(true);
  canvas->DrawPath(SkPath().addRoundRect(gfx::RectFToSkRect(bounds), 16, 16),
                   flags);
}

BEGIN_METADATA(EolNoticeQuickSettingsView)
END_METADATA

}  // namespace ash
