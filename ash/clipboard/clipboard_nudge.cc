// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_nudge.h"

#include <memory>

#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/system_nudge_label.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/events/keyboard_capability.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

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

bool IsAssistantAvailable() {
  AssistantStateBase* state = AssistantState::Get();
  return state->allowed_state() == assistant::AssistantAllowedState::ALLOWED &&
         state->settings_enabled().value_or(false);
}

// Determines the clipboard history keyboard shortcut icon for the user's
// keyboard layout and Assistant availability.
const gfx::VectorIcon& GetKeyboardShortcutIcon(bool use_launcher_key) {
  if (use_launcher_key) {
    return IsAssistantAvailable() ? kClipboardLauncherIcon
                                  : kClipboardLauncherNoAssistantIcon;
  }

  return kClipboardSearchIcon;
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
  bool use_launcher_key =
      Shell::Get()->keyboard_capability()->HasLauncherButton();
  std::u16string shortcut_key = l10n_util::GetStringUTF16(
      use_launcher_key ? IDS_ASH_SHORTCUT_MODIFIER_LAUNCHER
                       : IDS_ASH_SHORTCUT_MODIFIER_SEARCH);
  size_t offset;
  std::u16string label_text = l10n_util::GetStringFUTF16(
      nudge_type_ == kZeroStateNudge
          ? IDS_ASH_MULTIPASTE_ZERO_STATE_CONTEXTUAL_NUDGE
          : IDS_ASH_MULTIPASTE_CONTEXTUAL_NUDGE,
      shortcut_key, &offset);
  offset = offset + shortcut_key.length();
  auto label = std::make_unique<SystemNudgeLabel>(label_text, kMinLabelWidth);

  auto keyboard_shortcut_icon_image_view =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          GetKeyboardShortcutIcon(use_launcher_key), cros_tokens::kColorPrimary,
          kKeyboardShortcutIconSize));
  keyboard_shortcut_icon_image_view->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(2, 4, 0, 0)));

  // Transfer shortcut icon ownership to the label.
  label->AddCustomView(std::move(keyboard_shortcut_icon_image_view), offset);
  return label;
}

const gfx::VectorIcon& ClipboardNudge::GetIcon() const {
  return nudge_type_ == kZeroStateNudge ? kClipboardEmptyIcon : kClipboardIcon;
}

std::u16string ClipboardNudge::GetAccessibilityText() const {
  // TODO(crbug.com/1256854): Calculate text for screen readers.
  return u"";
}

}  // namespace ash
