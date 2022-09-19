// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/camera/autozoom_nudge.h"

#include <memory>

#include "ash/constants/notifier_catalogs.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/system_nudge_label.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {

namespace {

// The size of the autozoom icon.
constexpr int kAutozoomIconSize = 20;

// The minimum width of the label.
constexpr int kMinLabelWidth = 220;

// The spacing between the icon and label in the nudge view.
constexpr int kIconLabelSpacing = 16;

// The padding which separates the nudge's border with its inner contents.
constexpr int kNudgePadding = 16;

constexpr char kAutozoomNudgeName[] = "AutozoomNudge";

}  // namespace

AutozoomNudge::AutozoomNudge()
    : SystemNudge(kAutozoomNudgeName,
                  NudgeCatalogName::kAutozoom,
                  kAutozoomIconSize,
                  kIconLabelSpacing,
                  kNudgePadding,
                  AshColorProvider::ContentLayerType::kIconColorProminent) {}

AutozoomNudge::~AutozoomNudge() = default;

std::unique_ptr<SystemNudgeLabel> AutozoomNudge::CreateLabelView() const {
  std::u16string label_text = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_AUTOZOOM_EDUCATIONAL_NUDGE_TEXT);
  // Set the label's text.
  auto label = std::make_unique<SystemNudgeLabel>(label_text, kMinLabelWidth);
  label->set_font_size_delta(2);
  return label;
}

const gfx::VectorIcon& AutozoomNudge::GetIcon() const {
  return kUnifiedMenuAutozoomIcon;
}

std::u16string AutozoomNudge::GetAccessibilityText() const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_AUTOZOOM_EDUCATIONAL_NUDGE_TEXT);
}

}  // namespace ash
