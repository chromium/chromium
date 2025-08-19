// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/mouse_keys/mouse_keys_bubble_view.h"

#include <memory>
#include <string_view>
#include <vector>

#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

namespace {
constexpr int kIconSizeDip = 16;
constexpr int kSpaceBetweenIconAndTextDip = 4;

std::unique_ptr<views::ImageView> CreateImageView(
    const gfx::VectorIcon& icon,
    raw_ptr<views::ImageView>* destination_view) {
  return views::Builder<views::ImageView>()
      .SetImage(ui::ImageModel::FromVectorIcon(icon, kColorAshTextColorPrimary,
                                               kIconSizeDip))
      .CopyAddressTo(destination_view)
      .Build();
}

std::unique_ptr<views::Label> CreateLabelView(
    const std::u16string& text,
    ui::ColorId enabled_color_id,
    raw_ptr<views::Label>* destination_view) {
  return views::Builder<views::Label>()
      .SetText(text)
      .SetEnabledColor(enabled_color_id)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetMultiLine(false)
      .CopyAddressTo(destination_view)
      .Build();
}

aura::Window* FindRootWindowAtMousePosition() {
  auto* screen = display::Screen::Get();
  CHECK(screen);
  auto display = screen->GetDisplayNearestPoint(screen->GetCursorScreenPoint());
  return Shell::GetRootWindowForDisplayId(display.id());
}

}  // namespace

MouseKeysBubbleView::MouseKeysBubbleView() {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  auto* screen = display::Screen::Get();
  CHECK(screen);
  auto display = screen->GetDisplayNearestPoint(screen->GetCursorScreenPoint());

  set_parent_window(
      Shell::GetContainer(FindRootWindowAtMousePosition(),
                          kShellWindowId_AccessibilityBubbleContainer));

  // Set layout.
  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  layout->set_between_child_spacing(kSpaceBetweenIconAndTextDip);
  SetLayoutManager(std::move(layout));
  UseCompactMargins();

  // Add icons.
  AddChildView(
      CreateImageView(kSystemMenuMouseIcon, &mouse_button_change_icon_));
  AddChildView(CreateImageView(kMouseKeysDragIcon, &mouse_drag_icon_));

  // Add label.
  AddChildView(
      CreateLabelView(std::u16string(), kColorAshTextColorPrimary, &label_));

  GetViewAccessibility().SetRole(ax::mojom::Role::kGenericContainer);
  // Note: this static variable is used so that this view can be identified
  // from tests. Do not change this, as it will cause test failures.
  GetViewAccessibility().SetClassName("MouseKeysBubbleView");
}

MouseKeysBubbleView::~MouseKeysBubbleView() = default;

void MouseKeysBubbleView::Update(MouseKeysBubbleIconType icon,
                                 const std::optional<std::u16string>& text) {
  // Update icon visibility.
  mouse_button_change_icon_->SetVisible(
      icon == MouseKeysBubbleIconType::kButtonChanged);
  mouse_drag_icon_->SetVisible(icon == MouseKeysBubbleIconType::kMouseDrag);
  // Update label.
  label_->SetVisible(text.has_value());
  label_->SetText(text.has_value() ? text.value() : std::u16string());
  SizeToPreferredSize();
  SizeToContents();
}

void MouseKeysBubbleView::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  params->type = views::Widget::InitParams::TYPE_BUBBLE;
  params->opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params->activatable = views::Widget::InitParams::Activatable::kNo;
  params->shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params->name = "MouseKeysBubbleView";
}

std::u16string_view MouseKeysBubbleView::GetTextForTesting() const {
  return label_->GetText();
}

views::ImageView* MouseKeysBubbleView::GetMouseButtonChangeIconForTesting()
    const {
  return mouse_button_change_icon_;
}

views::ImageView* MouseKeysBubbleView::GetMouseDragIconForTesting() const {
  return mouse_drag_icon_;
}

BEGIN_METADATA(MouseKeysBubbleView)
END_METADATA
}  // namespace ash
