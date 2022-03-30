// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_nudge.h"

#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
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
  return state->allowed_state() ==
             chromeos::assistant::AssistantAllowedState::ALLOWED &&
         state->settings_enabled().value_or(false);
}

}  // namespace

ClipboardNudge::ClipboardNudge(ClipboardNudgeType nudge_type)
    : SystemNudge(kClipboardNudgeName,
                  kClipboardIconSize,
                  kIconLabelSpacing,
                  kNudgePadding),
      nudge_type_(nudge_type) {}

ClipboardNudge::~ClipboardNudge() = default;

std::unique_ptr<views::View> ClipboardNudge::CreateLabelView() const {
  std::unique_ptr<views::StyledLabel> label =
      std::make_unique<views::StyledLabel>();
  label->SetPaintToLayer();
  label->layer()->SetFillsBoundsOpaquely(false);

  bool use_launcher_key = ui::DeviceUsesKeyboardLayout2();

  // Set the keyboard shortcut icon depending on whether search button or
  // launcher button is being used.
  gfx::ImageSkia shortcut_icon;
  SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);
  if (use_launcher_key) {
    if (IsAssistantAvailable()) {
      shortcut_icon = gfx::CreateVectorIcon(gfx::IconDescription(
          kClipboardLauncherOuterIcon, kKeyboardShortcutIconSize, icon_color,
          &kClipboardLauncherInnerIcon));
    } else {
      shortcut_icon =
          gfx::CreateVectorIcon(kClipboardLauncherNoAssistantIcon,
                                kKeyboardShortcutIconSize, icon_color);
    }
  } else {
    shortcut_icon = gfx::CreateVectorIcon(
        kClipboardSearchIcon, kKeyboardShortcutIconSize, icon_color);
  }
  auto keyboard_shortcut_icon = std::make_unique<views::ImageView>();
  keyboard_shortcut_icon->SetImage(shortcut_icon);
  keyboard_shortcut_icon->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(2, 4, 0, -2)));

  // Set the text for |label_|.
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
  label->SetText(label_text);

  // Set the color of the text surrounding the shortcut icon.
  views::StyledLabel::RangeStyleInfo text_color;
  text_color.override_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  label->AddStyleRange(gfx::Range(0, offset), text_color);
  label->AddStyleRange(gfx::Range(offset + 1, label_text.length()), text_color);

  // Add the shortcut icon to |label_|.
  views::StyledLabel::RangeStyleInfo icon_style;
  icon_style.custom_view = keyboard_shortcut_icon.get();
  label->AddCustomView(std::move(keyboard_shortcut_icon));
  label->AddStyleRange(gfx::Range(offset, offset + 1), icon_style);

  label->SizeToFit(kMinLabelWidth);
  label->SetDisplayedOnBackgroundColor(SK_ColorTRANSPARENT);
  return std::move(label);
}

const gfx::VectorIcon& ClipboardNudge::GetIcon() const {
  return nudge_type_ == kZeroStateNudge ? kClipboardEmptyIcon : kClipboardIcon;
}

std::u16string ClipboardNudge::GetAccessibilityText() const {
  // TODO(crbug.com/1256854): Calculate text for screen readers.
  return u"";
}

}  // namespace ash
