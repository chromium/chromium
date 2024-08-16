// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/bubble/bubble_utils.h"

#include <memory>

#include "ash/capture_mode/capture_mode_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/style/typography.h"
#include "base/check.h"
#include "base/types/cxx23_to_underlying.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/label.h"

namespace ash::bubble_utils {

bool ShouldCloseBubbleForEvent(const ui::LocatedEvent& event) {
  // Should only be called for "press" or scroll begin type events.
  DCHECK(event.type() == ui::EventType::kMousePressed ||
         event.type() == ui::EventType::kTouchPressed ||
         event.type() == ui::EventType::kGestureLongPress ||
         event.type() == ui::EventType::kGestureTap ||
         event.type() == ui::EventType::kGestureTwoFingerTap ||
         event.type() == ui::EventType::kGestureScrollBegin)
      << base::to_underlying(event.type());

  // Users in a capture session may be trying to capture the bubble.
  if (capture_mode_util::IsCaptureModeActive())
    return false;

  aura::Window* target = static_cast<aura::Window*>(event.target());
  if (!target)
    return false;

  RootWindowController* root_controller =
      RootWindowController::ForWindow(target);
  if (!root_controller)
    return false;

  // Bubbles can spawn menus, so don't close for clicks inside menus.
  aura::Window* menu_container =
      root_controller->GetContainer(kShellWindowId_MenuContainer);
  if (menu_container->Contains(target))
    return false;

  // Taps on virtual keyboard should not close bubbles.
  aura::Window* keyboard_container =
      root_controller->GetContainer(kShellWindowId_VirtualKeyboardContainer);
  if (keyboard_container->Contains(target))
    return false;

  // Touch text selection controls should not close bubbles.
  // https://crbug.com/1165938
  aura::Window* settings_bubble_container =
      root_controller->GetContainer(kShellWindowId_SettingBubbleContainer);
  if (settings_bubble_container->Contains(target))
    return false;

  // Ignore clicks in the help bubble container.
  aura::Window* help_bubble_container =
      root_controller->GetContainer(kShellWindowId_HelpBubbleContainer);
  if (help_bubble_container->Contains(target)) {
    return false;
  }

  // Ignore clicks in the shelf area containing app icons. This is to ensure
  // that the bubble is not closed when you click on a shelf arrow.
  Shelf* shelf = Shelf::ForWindow(target);
  if (target == shelf->hotseat_widget()->GetNativeWindow() &&
      shelf->hotseat_widget()->EventTargetsShelfView(event)) {
    return false;
  }

  return true;
}

void ApplyStyle(views::Label* label,
                TypographyToken style,
                ui::ColorId text_color_id) {
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColorId(text_color_id);

  if (chromeos::features::IsJellyEnabled()) {
    TypographyProvider::Get()->StyleLabel(style, *label);
    return;
  }

  switch (style) {
    case TypographyToken::kCrosAnnotation1:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 12,
                                       gfx::Font::Weight::NORMAL));
      break;
    case TypographyToken::kCrosAnnotation2:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 11,
                                       gfx::Font::Weight::NORMAL));
      break;
    case TypographyToken::kCrosBody1:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 14,
                                       gfx::Font::Weight::NORMAL));
      break;
    case TypographyToken::kCrosBody2:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 13,
                                       gfx::Font::Weight::NORMAL));
      break;
    case TypographyToken::kCrosButton1:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 14,
                                       gfx::Font::Weight::MEDIUM));
      break;
    case TypographyToken::kCrosButton2:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 13,
                                       gfx::Font::Weight::MEDIUM));
      break;
    case TypographyToken::kCrosDisplay7:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 18,
                                       gfx::Font::Weight::MEDIUM));
      break;
    case TypographyToken::kCrosHeadline1:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 15,
                                       gfx::Font::Weight::MEDIUM));
      break;
    case TypographyToken::kCrosLabel1:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 10,
                                       gfx::Font::Weight::MEDIUM));
      break;
    case TypographyToken::kCrosTitle1:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 16,
                                       gfx::Font::Weight::MEDIUM));
      break;
    default:
      NOTREACHED();
  }
}

std::unique_ptr<views::Label> CreateLabel(TypographyToken style,
                                          const std::u16string& text,
                                          ui::ColorId text_color_id) {
  auto label = std::make_unique<views::Label>(text);
  ApplyStyle(label.get(), style, text_color_id);
  return label;
}

}  // namespace ash::bubble_utils
