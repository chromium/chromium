// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_textfield.h"

#include "ash/style/system_textfield_controller.h"
#include "system_textfield.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {

// The name of font family.
constexpr char kGoogleSansFont[] = "Google Sans";
// The heights of textfield containers for different font sizes.
constexpr int kSmallContainerHeight = 24;
constexpr int kMediumContainerHeight = 28;
constexpr int kLargeContainerHeight = 28;
// The font sizes in pixels.
constexpr int kSmallFontSize = 12;
constexpr int kMediumFontSize = 14;
constexpr int kLargeFontSize = 16;
// The minimum textfield container width.
constexpr int kMinWidth = 80;
// The gap between the focus ring and textfield container.
constexpr float kFocusRingGap = 2.0f;
// The border insets to add horizontal paddings in container.
constexpr gfx::Insets kBorderInsets = gfx::Insets::VH(0, 8);
// The rounded conner radius of textfield container.
constexpr int kCornerRadius = 4;

// Gets textfield container heights for different types.
int GetContainerHeightFromType(SystemTextfield::Type type) {
  int container_height;
  switch (type) {
    case SystemTextfield::Type::kSmall:
      container_height = kSmallContainerHeight;
      break;
    case SystemTextfield::Type::kMedium:
      container_height = kMediumContainerHeight;
      break;
    case SystemTextfield::Type::kLarge:
      container_height = kLargeContainerHeight;
      break;
  }
  return container_height;
}

// Gets font list for different types.
gfx::FontList GetFontListFromType(SystemTextfield::Type type) {
  int font_size;
  switch (type) {
    case SystemTextfield::Type::kSmall:
      font_size = kSmallFontSize;
      break;
    case SystemTextfield::Type::kMedium:
      font_size = kMediumFontSize;
      break;
    case SystemTextfield::Type::kLarge:
      font_size = kLargeFontSize;
      break;
  }
  return gfx::FontList({kGoogleSansFont}, gfx::Font::NORMAL, font_size,
                       gfx::Font::Weight::NORMAL);
}

}  // namespace

SystemTextfield::SystemTextfield(Type type) : type_(type) {
  SetFontList(GetFontListFromType(type_));
  SetBorder(views::CreateEmptyBorder(kBorderInsets));

  // Configure focus ring.
  auto* focus_ring = views::FocusRing::Get(this);
  DCHECK(focus_ring);
  const float halo_thickness = focus_ring->GetHaloThickness();
  focus_ring->SetHaloInset(-kFocusRingGap - 0.5f * halo_thickness);
  focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);
  focus_ring->SetHasFocusPredicate(
      [&](views::View* view) -> bool { return show_focus_ring_; });

  enabled_changed_subscription_ = AddEnabledChangedCallback(base::BindRepeating(
      &SystemTextfield::OnEnabledStateChanged, base::Unretained(this)));
}

SystemTextfield::~SystemTextfield() = default;

void SystemTextfield::SetActive(bool active) {
  if (IsActive() == active) {
    return;
  }

  if (active) {
    // Activate the textfield and record the text content.
    views::Textfield::OnFocus();
    restored_text_content_ = GetText();
  } else {
    // Clear selection when the textfield is deactivated.
    ClearSelection();
    views::Textfield::OnBlur();
  }

  SetShowFocusRing(active);
  UpdateBackground();
}

bool SystemTextfield::IsActive() const {
  return GetRenderText()->focused();
}

void SystemTextfield::SetShowFocusRing(bool show) {
  if (show_focus_ring_ == show) {
    return;
  }
  show_focus_ring_ = show;
  views::FocusRing::Get(this)->SchedulePaint();
}

void SystemTextfield::RestoreText() {
  SetText(restored_text_content_);
}

gfx::Size SystemTextfield::CalculatePreferredSize() const {
  // The width of container equals to the content width with horizontal padding.
  // The height of the container dependents on the type.
  const std::u16string& text = GetText();
  int width = 0;
  int height = 0;
  gfx::Canvas::SizeStringInt(text.empty() ? GetPlaceholderText() : text,
                             GetFontListFromType(type_), &width, &height, 0,
                             gfx::Canvas::NO_ELLIPSIS);
  return gfx::Size(
      std::max(width + GetCaretBounds().width() + GetInsets().width(),
               kMinWidth),
      GetContainerHeightFromType(type_));
}

void SystemTextfield::SetBorder(std::unique_ptr<views::Border> b) {
  // The base `Textfield` has a preset border. When a new border is set, the
  // focus ring will be removed. The `SystemTextfield` needs an empty border for
  // horizontal padding and keeps the focus ring.
  views::View::SetBorder(std::move(b));
}

void SystemTextfield::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateBackground();
}

void SystemTextfield::OnMouseExited(const ui::MouseEvent& event) {
  UpdateBackground();
}

void SystemTextfield::OnThemeChanged() {
  views::View::OnThemeChanged();

  // Only update the text color since the background color will be handled by
  // themed background.
  UpdateTextColor();
}

void SystemTextfield::OnFocus() {
  if (delegate_) {
    delegate_->OnTextfieldFocused(this);
  } else {
    SetActive(true);
  }

  UpdateBackground();
}

void SystemTextfield::OnBlur() {
  if (delegate_) {
    delegate_->OnTextfieldBlurred(this);
  } else {
    SetActive(false);
  }

  UpdateBackground();
}

void SystemTextfield::OnEnabledStateChanged() {
  UpdateBackground();
  UpdateTextColor();
  SchedulePaint();
}

void SystemTextfield::UpdateTextColor() {
  if (!GetWidget()) {
    return;
  }

  // Set text color.
  auto* color_provider = GetColorProvider();
  gfx::RenderText* render_text = GetRenderText();
  if (!GetEnabled()) {
    SetColor(color_provider->GetColor(cros_tokens::kCrosSysDisabled));
    return;
  }

  // Set text color and selection text and background (highlight part) colors.
  SetColor(color_provider->GetColor(cros_tokens::kCrosSysOnSurface));
  render_text->set_selection_color(
      color_provider->GetColor(cros_tokens::kCrosSysOnSurface));
  render_text->set_selection_background_focused_color(
      color_provider->GetColor(cros_tokens::kCrosSysHighlightText));
}

void SystemTextfield::UpdateBackground() {
  // Create a themed rounded rect background when the mouse hovers on the
  // textfield or the textfield is focused.
  if (IsMouseHovered() || HasFocus()) {
    SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysHoverOnSubtle, kCornerRadius));
    return;
  }

  // In other cases, use a transparent background.
  SetBackground(nullptr);
}

BEGIN_METADATA(SystemTextfield, views::Textfield)
END_METADATA

}  // namespace ash
