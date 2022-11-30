// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/dictation_nudge.h"

#include "ash/accessibility/dictation_nudge_controller.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/system_nudge_label.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The size of the clipboard icon.
constexpr int kIconSize = 20;

// The minimum width of the label.
constexpr int kMinLabelWidth = 200;

// The spacing between the icon and label in the nudge view.
constexpr int kIconLabelSpacing = 16;

// The padding which separates the nudge's border with its inner contents.
constexpr int kNudgePadding = 16;

constexpr char kDictationNudgeName[] = "DictationOfflineContextualNudge";

}  // namespace

DictationNudge::DictationNudge(DictationNudgeController* controller)
    : SystemNudge(kDictationNudgeName,
                  NudgeCatalogName::kDictation,
                  kIconSize,
                  kIconLabelSpacing,
                  kNudgePadding),
      controller_(controller) {}

DictationNudge::~DictationNudge() = default;

std::unique_ptr<SystemNudgeLabel> DictationNudge::CreateLabelView() const {
  return std::make_unique<SystemNudgeLabel>(GetAccessibilityText(),
                                            kMinLabelWidth);
}

const gfx::VectorIcon& DictationNudge::GetIcon() const {
  return kDictationOffNewuiIcon;
}

std::u16string DictationNudge::GetAccessibilityText() const {
  const std::u16string language_name = l10n_util::GetDisplayNameForLocale(
      controller_->dictation_locale(), controller_->application_locale(),
      /*is_for_ui=*/true);

  return l10n_util::GetStringFUTF16(
      IDS_ASH_DICTATION_LANGUAGE_SUPPORTED_OFFLINE_NUDGE, language_name);
}

}  // namespace ash
