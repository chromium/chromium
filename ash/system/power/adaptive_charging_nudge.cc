// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/adaptive_charging_nudge.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/system/tray/system_nudge_label.h"
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
                  NudgeCatalogName::kAdaptiveCharging,
                  kIconSize,
                  kIconLabelSpacing,
                  kNudgePadding) {}

AdaptiveChargingNudge::~AdaptiveChargingNudge() = default;

std::unique_ptr<SystemNudgeLabel> AdaptiveChargingNudge::CreateLabelView()
    const {
  std::u16string label_text = l10n_util::GetStringUTF16(
      IDS_ASH_ADAPTIVE_CHARGING_EDUCATIONAL_NUDGE_TEXT);
  auto label = std::make_unique<SystemNudgeLabel>(label_text, kMinLabelWidth);
  label->set_font_size_delta(kFontSizeDelta);
  return label;
}

const gfx::VectorIcon& AdaptiveChargingNudge::GetIcon() const {
  return DarkLightModeControllerImpl::Get()->IsDarkModeEnabled()
             ? kAdaptiveChargingNudgeDarkIcon
             : kAdaptiveChargingNudgeLightIcon;
}

std::u16string AdaptiveChargingNudge::GetAccessibilityText() const {
  // TODO(b:216035485): Calculate text for screen readers.
  return u"";
}

}  // namespace ash
