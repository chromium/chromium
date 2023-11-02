// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_nudge.h"

#include <memory>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/system_nudge_label.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/events/keyboard_layout_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/wm/core/coordinate_conversion.h"

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

}  // namespace

ClipboardNudge::ClipboardNudge(ClipboardNudgeType nudge_type)
    : SystemNudge(kClipboardNudgeName,
                  NudgeCatalogName::kMultipaste,
                  kClipboardIconSize,
                  kIconLabelSpacing,
                  kNudgePadding),
      nudge_type_(nudge_type) {}

ClipboardNudge::~ClipboardNudge() = default;

std::unique_ptr<SystemNudgeLabel> ClipboardNudge::CreateLabelView() const {
  bool use_launcher_key = ui::DeviceUsesKeyboardLayout2();
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
  // Set the label's text.
  auto label = std::make_unique<SystemNudgeLabel>(label_text, kMinLabelWidth);

  // Set the keyboard shortcut icon depending on whether search button
  // or launcher button is being used.
  auto keyboard_shortcut_icon = std::make_unique<views::ImageView>();
  keyboard_shortcut_icon->SetImage(ui::ImageModel::FromImageGenerator(
      base::BindRepeating(
          [](bool use_launcher_key, const ui::ColorProvider*) {
            SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kIconColorPrimary);
            if (use_launcher_key) {
              return IsAssistantAvailable()
                         ? gfx::CreateVectorIcon(gfx::IconDescription(
                               kClipboardLauncherOuterIcon,
                               kKeyboardShortcutIconSize, icon_color,
                               &kClipboardLauncherInnerIcon))
                         : gfx::CreateVectorIcon(
                               kClipboardLauncherNoAssistantIcon,
                               kKeyboardShortcutIconSize, icon_color);
            }
            return gfx::CreateVectorIcon(kClipboardSearchIcon,
                                         kKeyboardShortcutIconSize, icon_color);
          },
          use_launcher_key),
      gfx::Size(kKeyboardShortcutIconSize, kKeyboardShortcutIconSize)));
  keyboard_shortcut_icon->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(2, 4, 0, -2)));

  // Transfer shortcut icon ownership to the label.
  label->AddCustomView(std::move(keyboard_shortcut_icon), offset);

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
