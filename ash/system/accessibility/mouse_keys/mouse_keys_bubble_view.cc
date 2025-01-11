// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/mouse_keys/mouse_keys_bubble_view.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

namespace {
constexpr int kSpaceBetweenIconAndTextDip = 4;

std::unique_ptr<views::Label> CreateLabelView(
    const std::u16string& text,
    ui::ColorId enabled_color_id,
    raw_ptr<views::Label>* destination_view) {
  return views::Builder<views::Label>()
      .SetText(text)
      .SetEnabledColorId(enabled_color_id)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetMultiLine(false)
      .CopyAddressTo(destination_view)
      .Build();
}

aura::Window* FindRootWindowAtMousePosition() {
  auto* screen = display::Screen::GetScreen();
  CHECK(screen);
  auto display = screen->GetDisplayNearestPoint(screen->GetCursorScreenPoint());
  return Shell::GetRootWindowForDisplayId(display.id());
}

}  // namespace

MouseKeysBubbleView::MouseKeysBubbleView() {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  auto* screen = display::Screen::GetScreen();
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

  AddChildView(
      CreateLabelView(std::u16string(), kColorAshTextColorPrimary, &label_));

  GetViewAccessibility().SetRole(ax::mojom::Role::kGenericContainer);
  // Note: this static variable is used so that this view can be identified
  // from tests. Do not change this, as it will cause test failures.
  GetViewAccessibility().SetClassName("MouseKeysBubbleView");
}

MouseKeysBubbleView::~MouseKeysBubbleView() = default;

void MouseKeysBubbleView::Update(const std::optional<std::u16string>& text) {
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

std::u16string MouseKeysBubbleView::GetTextForTesting() const {
  return label_->GetText();
}

BEGIN_METADATA(MouseKeysBubbleView)
END_METADATA
}  // namespace ash