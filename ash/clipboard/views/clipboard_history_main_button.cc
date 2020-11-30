// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_main_button.h"

#include "ash/clipboard/views/clipboard_history_item_view.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/scoped_light_mode_as_default.h"
#include "ui/gfx/canvas.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {
namespace {

// The menu background's color type.
constexpr ash::AshColorProvider::BaseLayerType kMenuBackgroundColorType =
    ash::AshColorProvider::BaseLayerType::kOpaque;

}  // namespace

ClipboardHistoryMainButton::ClipboardHistoryMainButton(
    ClipboardHistoryItemView* container)
    : Button(base::BindRepeating(
          [](ClipboardHistoryItemView* item, const ui::Event& event) {
            item->HandleMainButtonPressEvent(event);
          },
          base::Unretained(container))),
      container_(container) {
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  // Let the parent handle accessibility features.
  GetViewAccessibility().OverrideIsIgnored(/*value=*/true);
}

ClipboardHistoryMainButton::~ClipboardHistoryMainButton() = default;

void ClipboardHistoryMainButton::SetShouldHighlight(bool should_highlight) {
  if (should_highlight_ == should_highlight)
    return;

  should_highlight_ = should_highlight;
  SchedulePaint();
}

const char* ClipboardHistoryMainButton::GetClassName() const {
  return "ClipboardHistoryMainButton";
}

void ClipboardHistoryMainButton::OnThemeChanged() {
  views::Button::OnThemeChanged();
  SchedulePaint();
}

void ClipboardHistoryMainButton::OnGestureEvent(ui::GestureEvent* event) {
  // Give `container_` a chance to handle `event`.
  container_->MaybeHandleGestureEventFromMainButton(event);
  if (event->handled())
    return;

  views::Button::OnGestureEvent(event);

  // Prevent the menu controller from handling gesture events. The menu
  // controller may bring side-effects such as canceling the item selection.
  event->SetHandled();
}

void ClipboardHistoryMainButton::PaintButtonContents(gfx::Canvas* canvas) {
  if (!should_highlight_)
    return;

  // Use the light mode as default because the light mode is the default mode
  // of the native theme which decides the context menu's background color.
  // TODO(andrewxu): remove this line after https://crbug.com/1143009 is
  // fixed.
  ScopedLightModeAsDefault scoped_light_mode_as_default;

  // Highlight the background when the menu item is selected or pressed.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);

  const auto* color_provider = AshColorProvider::Get();
  const AshColorProvider::RippleAttributes ripple_attributes =
      color_provider->GetRippleAttributes(
          color_provider->GetBaseLayerColor(kMenuBackgroundColorType));
  flags.setColor(SkColorSetA(ripple_attributes.base_color,
                             ripple_attributes.highlight_opacity * 0xFF));
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawRect(GetLocalBounds(), flags);
}

}  // namespace ash
