// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_nudge.h"

#include <memory>
#include <string>

#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/clipboard_nudge_constants.h"
#include "ash/clipboard/views/clipboard_history_view_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/system_nudge_label.h"
#include "base/notreached.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

// Constants -------------------------------------------------------------------

// The size of the clipboard icon.
constexpr int kClipboardIconSize = 20;

// The size of the keyboard shortcut icon.
constexpr int kKeyboardShortcutIconSize = 14;

// The minimum width of the label.
constexpr int kMinLabelWidth = 200;

// The spacing between the icon and label in the nudge view.
constexpr int kIconLabelSpacing = 16;

// The padding which separates the nudge's border with its inner contents.
constexpr int kNudgePadding = 16;

constexpr char kClipboardNudgeName[] = "ClipboardContextualNudge";

// Helpers ---------------------------------------------------------------------

int GetNudgeStringId(ClipboardNudgeType nudge_type) {
  switch (nudge_type) {
    case ClipboardNudgeType::kDuplicateCopyNudge:
      return IDS_ASH_MULTIPASTE_DUPLICATE_COPY_NUDGE;
    case ClipboardNudgeType::kOnboardingNudge:
      return IDS_ASH_MULTIPASTE_CONTEXTUAL_NUDGE;
    case ClipboardNudgeType::kScreenshotNotificationNudge:
      return IDS_ASH_MULTIPASTE_SCREENSHOT_NOTIFICATION_NUDGE;
    case ClipboardNudgeType::kZeroStateNudge:
      return IDS_ASH_MULTIPASTE_ZERO_STATE_CONTEXTUAL_NUDGE;
  }
}

}  // namespace

ClipboardNudge::ClipboardNudge(ClipboardNudgeType nudge_type,
                               NudgeCatalogName catalog_name)
    : SystemNudge(kClipboardNudgeName,
                  catalog_name,
                  kClipboardIconSize,
                  kIconLabelSpacing,
                  kNudgePadding),
      nudge_type_(nudge_type) {}

ClipboardNudge::~ClipboardNudge() = default;

std::unique_ptr<SystemNudgeLabel> ClipboardNudge::CreateLabelView() const {
  const std::u16string shortcut_key =
      clipboard_history_util::GetShortcutKeyName();

  size_t offset;
  auto label = std::make_unique<SystemNudgeLabel>(
      l10n_util::GetStringFUTF16(GetNudgeStringId(nudge_type_), shortcut_key,
                                 &offset),
      kMinLabelWidth);
  offset = offset + shortcut_key.length();

  auto keyboard_shortcut_icon_image_view =
      views::Builder<views::ImageView>()
          .SetBorder(views::CreateEmptyBorder(
              ClipboardHistoryViews::kInlineIconMargins))
          .SetImage(ui::ImageModel::FromVectorIcon(
              clipboard_history_util::GetShortcutKeyIcon(),
              cros_tokens::kColorPrimary, kKeyboardShortcutIconSize))
          .Build();

  // Transfer shortcut icon ownership to the label.
  label->AddCustomView(std::move(keyboard_shortcut_icon_image_view), offset);
  return label;
}

const gfx::VectorIcon& ClipboardNudge::GetIcon() const {
  return nudge_type_ == kZeroStateNudge ? kClipboardEmptyIcon : kClipboardIcon;
}

std::u16string ClipboardNudge::GetAccessibilityText() const {
  return l10n_util::GetStringFUTF16(
      GetNudgeStringId(nudge_type_),
      clipboard_history_util::GetShortcutKeyName());
}

}  // namespace ash
