// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geic/geic_button.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/geic/geic_side_panel_coordinator.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "chrome/grit/branded_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view_class_properties.h"

namespace geic {

namespace {

// TODO(crbug.com/544844434): Make these semantic dimensions.
gfx::Insets GetIconMargins(bool label_shown) {
  int left = 6;
  int right = 5;
  if (label_shown) {
    left += 2;
  }
  return gfx::Insets().set_left_right(left, right);
}

}  // namespace

// static
std::unique_ptr<GeicButton> GeicButton::Create(
    BrowserWindowInterface* browser_window_interface) {
  std::u16string tooltip_text =
      l10n_util::GetStringUTF16(IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP);

  auto geic_button = std::make_unique<GeicButton>(
      browser_window_interface, tooltip_text,
      base::BindRepeating(
          [](BrowserWindowInterface* bwi) {
            if (auto* coordinator = GeicSidePanelCoordinator::From(bwi)) {
              coordinator->Toggle();
            }
          },
          browser_window_interface));

  geic_button->SetProperty(views::kCrossAxisAlignmentKey,
                           views::LayoutAlignment::kCenter);

  return geic_button;
}

GeicButton::GeicButton(BrowserWindowInterface* browser_window_interface,
                       const std::u16string& tooltip,
                       PressedCallback pressed_callback)
    : glic::GlicButton<TabStripNudgeButton>(browser_window_interface,
                                            base::DoNothing(),
                                            tooltip,
                                            kIconSize,
                                            /** TabStripNudgeButton args */
                                            browser_window_interface,
                                            std::move(pressed_callback),
                                            PressedCallback(),
                                            GetLabelText(),
                                            ui::ElementIdentifier(),
                                            Edge::kNone,
                                            gfx::VectorIcon::EmptyIcon(),
                                            /*show_close_button=*/false) {
  OnLabelVisibilityChanged();
  auto* image_view = static_cast<views::ImageView*>(image_container_view());
  image_view->SetImageSize({kIconSize, kIconSize});

  SetLabelMargins();
  UpdateColors();
}

GeicButton::~GeicButton() = default;

bool GeicButton::GetIsShowingNudge() const {
  return width_state_ ==
         glic::GlicButton<TabStripNudgeButton>::WidthState::kNudge;
}

void GeicButton::OnLabelVisibilityChanged() {
  image_container_view()->SetProperty(
      views::kMarginsKey, GetIconMargins(!IsAnimatingTextVisibility()));
}

void GeicButton::ResetSplitButtonCornerStyling() {
  SetLeftRightCornerRadii(TabStripNudgeButton::GetCornerRadius(),
                          TabStripNudgeButton::GetCornerRadius());
}

void GeicButton::SetLabelMargins() {
  int bottom = 1;
  int right = glic::kLabelRightMargin;
  if (!close_button() || !close_button()->GetVisible()) {
    right += 4;
  }
  label()->SetProperty(views::kMarginsKey,
                       gfx::Insets().set_right(right).set_bottom(bottom));
}

gfx::SlideAnimation* GeicButton::GetExpansionAnimationForTesting() {
  return width_animation_controller_->GetAnimationForTesting();  // IN-TEST
}

float GeicButton::GetWidthFactor() const {
  return TabStripNudgeButton::GetWidthFactor();
}

ui::ColorId GeicButton::GetCustomThemeForegroundId() const {
  return kColorTabSearchButtonCRForegroundFrameActive;
}

ui::ColorId GeicButton::GetCustomThemeBackgroundActiveId() const {
  return kColorNewTabButtonCRBackgroundFrameActive;
}

ui::ColorId GeicButton::GetCustomThemeBackgroundInactiveId() const {
  return kColorNewTabButtonCRBackgroundFrameInactive;
}

ui::ColorId GeicButton::GetCustomThemeForegroundActiveId() const {
  return kColorTabSearchButtonCRForegroundFrameActive;
}

ui::ColorId GeicButton::GetCustomThemeForegroundInactiveId() const {
  return kColorTabSearchButtonCRForegroundFrameInactive;
}

BEGIN_METADATA(GeicButton)
END_METADATA

}  // namespace geic
