// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/bubble/bubble_utils.h"

#include <memory>
#include <utility>

#include "ash/capture_mode/capture_mode_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "base/check.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace bubble_utils {

bool ShouldCloseBubbleForEvent(const ui::LocatedEvent& event) {
  // Should only be called for "press" type events.
  DCHECK(event.type() == ui::ET_MOUSE_PRESSED ||
         event.type() == ui::ET_TOUCH_PRESSED ||
         event.type() == ui::ET_GESTURE_LONG_PRESS ||
         event.type() == ui::ET_GESTURE_TAP ||
         event.type() == ui::ET_GESTURE_TWO_FINGER_TAP)
      << event.type();

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

  return true;
}

void ApplyStyle(views::Label* label,
                TypographyStyle style,
                ui::ColorId text_color_id) {
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColorId(text_color_id);

  switch (style) {
    case TypographyStyle::kAnnotation1:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 12,
                                       gfx::Font::Weight::NORMAL));
      break;
    case TypographyStyle::kAnnotation2:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 11,
                                       gfx::Font::Weight::NORMAL));
      break;
    case TypographyStyle::kBody1:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 14,
                                       gfx::Font::Weight::NORMAL));
      break;
    case TypographyStyle::kBody2:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 13,
                                       gfx::Font::Weight::NORMAL));
      break;
    case TypographyStyle::kButton1:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 14,
                                       gfx::Font::Weight::MEDIUM));
      break;
    case TypographyStyle::kButton2:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 13,
                                       gfx::Font::Weight::MEDIUM));
      break;
    case TypographyStyle::kLabel1:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 10,
                                       gfx::Font::Weight::MEDIUM));
      break;
    case TypographyStyle::kTitle1:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 16,
                                       gfx::Font::Weight::MEDIUM));
      break;
  }
}

std::unique_ptr<views::Label> CreateLabel(TypographyStyle style,
                                          const std::u16string& text,
                                          ui::ColorId text_color_id) {
  auto label = std::make_unique<views::Label>(text);
  ApplyStyle(label.get(), style, text_color_id);
  return label;
}

}  // namespace bubble_utils
}  // namespace ash
