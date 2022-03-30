// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/adaptive_charging_nudge.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/styled_label.h"

namespace ash {

namespace {

// The size of the icon.
constexpr int kIconSize = 60;

// The spacing between the icon and label in the nudge view.
constexpr int kIconLabelSpacing = 20;

// The padding which separates the nudge's border with its inner contents.
constexpr int kNudgePadding = 20;

// The minimum width of the label.
constexpr int kMinLabelWidth = 232;

// Use a bigger font size than the default one.
constexpr int kFontSizeDelta = 2;

constexpr char kAdaptiveChargingNudgeName[] =
    "AdaptiveChargingEducationalNudge";

}  // namespace

AdaptiveChargingNudge::AdaptiveChargingNudge()
    : SystemNudge(kAdaptiveChargingNudgeName,
                  kIconSize,
                  kIconLabelSpacing,
                  kNudgePadding) {}

AdaptiveChargingNudge::~AdaptiveChargingNudge() = default;

std::unique_ptr<views::View> AdaptiveChargingNudge::CreateLabelView() const {
  std::unique_ptr<views::StyledLabel> label =
      std::make_unique<views::StyledLabel>();
  label->SetPaintToLayer();
  label->layer()->SetFillsBoundsOpaquely(false);
  label->SetPosition(
      gfx::Point(kNudgePadding + kIconSize + kIconLabelSpacing, kNudgePadding));

  std::u16string label_text = l10n_util::GetStringUTF16(
      IDS_ASH_ADAPTIVE_CHARGING_EDUCATIONAL_NUDGE_TEXT);
  label->SetText(label_text);

  // Text color and size.
  views::StyledLabel::RangeStyleInfo text_style;
  text_style.override_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  text_style.custom_font =
      label->GetFontList().DeriveWithSizeDelta(kFontSizeDelta);
  label->AddStyleRange(gfx::Range(0, label_text.length()), text_style);

  label->SizeToFit(kMinLabelWidth);
  label->SetDisplayedOnBackgroundColor(SK_ColorTRANSPARENT);
  return std::move(label);
}

const gfx::VectorIcon& AdaptiveChargingNudge::GetIcon() const {
  return AshColorProvider::Get()->IsDarkModeEnabled()
             ? kAdaptiveChargingNudgeDarkIcon
             : kAdaptiveChargingNudgeLightIcon;
}

std::u16string AdaptiveChargingNudge::GetAccessibilityText() const {
  // TODO(b:216035485): Calculate text for screen readers.
  return u"";
}

}  // namespace ash
