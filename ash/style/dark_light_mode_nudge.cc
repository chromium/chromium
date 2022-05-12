// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/dark_light_mode_nudge.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

// The size of the dark light mode icon.
constexpr int kIconSize = 20;

// The minimum width of the label.
constexpr int kMinLabelWidth = 200;

// The spacing between the icon and label in the nudge view.
constexpr int kIconLabelSpacing = 16;

// The padding which separates the nudge's border with its inner contents.
constexpr int kNudgePadding = 16;

constexpr char kDarkLightModeNudgeName[] = "DarkLightModeEducationalNudge";

}  // namespace

DarkLightModeNudge::DarkLightModeNudge()
    : SystemNudge(kDarkLightModeNudgeName,
                  kIconSize,
                  kIconLabelSpacing,
                  kNudgePadding) {}

DarkLightModeNudge::~DarkLightModeNudge() = default;

std::unique_ptr<views::View> DarkLightModeNudge::CreateLabelView() const {
  std::unique_ptr<views::Label> label = std::make_unique<views::Label>();
  label->SetPaintToLayer();
  label->layer()->SetFillsBoundsOpaquely(false);

  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  label->SetAutoColorReadabilityEnabled(false);

  label->SetText(GetAccessibilityText());
  label->SetMultiLine(true);
  label->SizeToFit(kMinLabelWidth);

  return std::move(label);
}

const gfx::VectorIcon& DarkLightModeNudge::GetIcon() const {
  return kUnifiedMenuDarkModeIcon;
}

std::u16string DarkLightModeNudge::GetAccessibilityText() const {
  return l10n_util::GetStringUTF16(
      TabletMode::Get()->InTabletMode()
          ? IDS_ASH_DARK_LIGHT_MODE_EDUCATIONAL_NUDGE_IN_TABLET_MODE
          : IDS_ASH_DARK_LIGHT_MODE_EDUCATIONAL_NUDGE_IN_CLAMSHELL_MODE);
}

}  // namespace ash
