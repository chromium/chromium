// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_recent_app_button.h"

#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

// Appearance in DIPs.
constexpr int kRecentAppButtonSize = 32;

}  // namespace

PhoneHubRecentAppButton::PhoneHubRecentAppButton()
    : views::ImageButton(
          base::BindRepeating(&PhoneHubRecentAppButton::ButtonPressed,
                              base::Unretained(this))) {
  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
  TrayPopupUtils::ConfigureTrayPopupButton(this);
  views::InstallCircleHighlightPathGenerator(this);
}

PhoneHubRecentAppButton::~PhoneHubRecentAppButton() = default;

gfx::Size PhoneHubRecentAppButton::CalculatePreferredSize() const {
  return gfx::Size(kRecentAppButtonSize, kRecentAppButtonSize);
}

void PhoneHubRecentAppButton::PaintButtonContents(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive));
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawPath(views::GetHighlightPath(this), flags);
  views::ImageButton::PaintButtonContents(canvas);
}

void PhoneHubRecentAppButton::OnThemeChanged() {
  views::ImageButton::OnThemeChanged();
  views::FocusRing::Get(this)->SetColor(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kFocusRingColor));
  SchedulePaint();
}

const char* PhoneHubRecentAppButton::GetClassName() const {
  return "PhoneHubRecentAppButton";
}

void PhoneHubRecentAppButton::ButtonPressed() {
  // TODO(paulzchen): Launch the recent apps with package name.
}

}  // namespace ash
