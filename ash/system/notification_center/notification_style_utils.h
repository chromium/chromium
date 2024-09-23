// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_STYLE_UTILS_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_STYLE_UTILS_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"

namespace message_center {
class MessageView;
class Notification;
class NotificationItem;
}  // namespace message_center

namespace ui {
class ColorProvider;
}  // namespace ui

namespace views {
class Background;
class FlexLayoutView;
class Label;
class LabelButton;
}  // namespace views

namespace ash::notification_style_utils {

// Returns a masked icon with a contrasting background using `small_image()` in
// `notification`. If `small_image()` is not provided returns a masked chrome
// icon.
gfx::ImageSkia CreateNotificationAppIcon(
    const message_center::Notification* notification);

// Returns a circular icon using the `ImageModel` provided in
// `NotificationItem`. If no image is provided a default icon is returned.
gfx::ImageSkia CreateNotificationItemIcon(
    const message_center::NotificationItem* item);

// Calculates the background color for the icon based on the current theme.
SkColor CalculateIconBackgroundColor(
    const message_center::Notification* notification);

// Configures the style for labels in notification views. `is_color_primary`
// indicates if the color of the text is primary or secondary text color.
void ConfigureLabelStyle(
    views::Label* label,
    int size,
    bool is_color_primary,
    gfx::Font::Weight font_weight = gfx::Font::Weight::NORMAL);

// Returns the default color provider.
ui::ColorProvider* GetColorProviderForNativeTheme();

// Creates a themed background with the provided corner radii for the provided
// notification specs.
std::unique_ptr<views::Background> CreateNotificationBackground(
    int top_radius,
    int bottom_radius,
    bool is_popup_notification,
    bool is_grouped_child_notification);

// Applies background color, background blur, highlight border and rounded
// corners for notification views that are contained in notification popups.
void StyleNotificationPopup(message_center::MessageView* notification_view);

// Returns a floating iconless `PillButton` with the provided callback and
// label.
std::unique_ptr<views::LabelButton> GenerateNotificationLabelButton(
    views::Button::PressedCallback callback,
    const std::u16string& label);

// Creates a view containing a `turn_off_notifications_button` and
// `cancel_button` that is used for inline settings in any `MessageView`.
std::unique_ptr<views::FlexLayoutView> CreateInlineSettingsViewForMessageView(
    message_center::MessageView* message_view);

}  // namespace ash::notification_style_utils

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_STYLE_UTILS_H_
