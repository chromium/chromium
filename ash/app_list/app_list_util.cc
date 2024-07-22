// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_util.h"

#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// The cardified apps grid and app icons should scale down by this factor.
constexpr float kAppsGridCardifiedScale = 0.9f;

}  // namespace

bool IsUnhandledUnmodifiedEvent(const ui::KeyEvent& event) {
  if (event.handled() || event.type() != ui::EventType::kKeyPressed) {
    return false;
  }

  if (event.IsShiftDown() || event.IsControlDown() || event.IsAltDown())
    return false;

  return true;
}

bool IsUnhandledLeftRightKeyEvent(const ui::KeyEvent& event) {
  if (!IsUnhandledUnmodifiedEvent(event))
    return false;

  return event.key_code() == ui::VKEY_LEFT ||
         event.key_code() == ui::VKEY_RIGHT;
}

bool IsUnhandledUpDownKeyEvent(const ui::KeyEvent& event) {
  if (!IsUnhandledUnmodifiedEvent(event))
    return false;

  return event.key_code() == ui::VKEY_UP || event.key_code() == ui::VKEY_DOWN;
}

bool IsUnhandledArrowKeyEvent(const ui::KeyEvent& event) {
  if (!IsUnhandledUnmodifiedEvent(event))
    return false;

  return IsArrowKeyEvent(event);
}

bool IsArrowKeyEvent(const ui::KeyEvent& event) {
  return IsArrowKey(event.key_code());
}

bool IsArrowKey(const ui::KeyboardCode& key_code) {
  return key_code == ui::VKEY_DOWN || key_code == ui::VKEY_RIGHT ||
         key_code == ui::VKEY_LEFT || key_code == ui::VKEY_UP;
}

bool IsFolderItem(AppListItem* item) {
  return item && item->GetItemType() == AppListFolderItem::kItemType;
}

bool LeftRightKeyEventShouldExitText(views::Textfield* textfield,
                                     const ui::KeyEvent& key_event) {
  DCHECK(IsUnhandledLeftRightKeyEvent(key_event));

  if (textfield->GetText().empty())
    return true;

  if (textfield->HasSelection())
    return false;

  if (textfield->GetCursorPosition() != 0 &&
      textfield->GetCursorPosition() != textfield->GetText().length()) {
    return false;
  }

  // For RTL language, the beginning position of the cursor will be at the right
  // side and it grows towards left as we are typing.
  const bool text_rtl =
      textfield->GetTextDirection() == base::i18n::RIGHT_TO_LEFT;
  const bool cursor_at_beginning = textfield->GetCursorPosition() == 0;
  const bool move_cursor_reverse =
      (text_rtl && key_event.key_code() == ui::VKEY_RIGHT) ||
      (!text_rtl && key_event.key_code() == ui::VKEY_LEFT);

  if ((cursor_at_beginning && !move_cursor_reverse) ||
      (!cursor_at_beginning && move_cursor_reverse)) {
    // Cursor is at either the beginning or the end of the textfield, and it
    // will move inward.
    return false;
  }

  return true;
}

bool ProcessLeftRightKeyTraversalForTextfield(views::Textfield* textfield,
                                              const ui::KeyEvent& key_event) {
  DCHECK(IsUnhandledLeftRightKeyEvent(key_event));

  if (!LeftRightKeyEventShouldExitText(textfield, key_event))
    return false;

  const bool move_focus_reverse = base::i18n::IsRTL()
                                      ? key_event.key_code() == ui::VKEY_RIGHT
                                      : key_event.key_code() == ui::VKEY_LEFT;

  // Move focus outside the textfield.
  textfield->GetFocusManager()->AdvanceFocus(move_focus_reverse);
  return true;
}

gfx::ImageSkia CreateIconWithCircleBackground(
    const gfx::ImageSkia& icon,
    const ui::ColorProvider* color_provider) {
  DCHECK_EQ(icon.width(), icon.height());
  return gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
      icon.width() / 2, color_provider->GetColor(kColorAshShieldAndBaseOpaque),
      icon);
}

void PaintFocusBar(gfx::Canvas* canvas,
                   const gfx::Point& content_origin,
                   int height,
                   SkColor color) {
  SkPath path;
  gfx::Rect focus_bar_bounds(content_origin.x() - kFocusBarThickness,
                             content_origin.y(), kFocusBarThickness * 2,
                             height);
  path.addRRect(SkRRect::MakeRectXY(RectToSkRect(focus_bar_bounds),
                                    kFocusBarThickness, kFocusBarThickness));
  canvas->ClipPath(path, true);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kFocusBarThickness);
  gfx::Point top_point = content_origin + gfx::Vector2d(kFocusBarThickness, 0);
  gfx::Point bottom_point =
      content_origin + gfx::Vector2d(kFocusBarThickness, height);
  canvas->DrawLine(top_point, bottom_point, flags);
}

void SetViewIgnoredForAccessibility(views::View* view, bool ignored) {
  auto& view_accessibility = view->GetViewAccessibility();
  view_accessibility.SetIsLeaf(ignored);
  view_accessibility.SetIsIgnored(ignored);
}

float GetAppsGridCardifiedScale() {
  return kAppsGridCardifiedScale;
}

}  // namespace ash
