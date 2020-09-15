// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bloom/bloom_tray.h"

#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "chromeos/components/bloom/public/cpp/bloom_controller.h"
#include "chromeos/components/bloom/public/cpp/bloom_interaction_resolution.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"

namespace ash {

using chromeos::bloom::BloomController;
using chromeos::bloom::BloomInteractionResolution;

BloomTray::BloomTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      icon_(tray_container()->AddChildView(
          std::make_unique<views::ImageView>())) {
  SetIcon();
  SetTooltipText();
}

BloomTray::~BloomTray() = default;

void BloomTray::Initialize() {
  TrayBackgroundView::Initialize();

  // TODO(jeroendh): Should not be shown on login screen.
  SetVisiblePreferred(true);
}

base::string16 BloomTray::GetAccessibleNameForTray() {
  // TODO(jeroendh): Use correct name;
  return base::ASCIIToUTF16("Bloom (THIS STRING MUST BE LOCALIZED)");
}

void BloomTray::HandleLocaleChange() {
  SetTooltipText();
}

void BloomTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {}

void BloomTray::ClickedOutsideBubble() {}

bool BloomTray::PerformAction(const ui::Event& event) {
  // TODO(jeroendh): RecordUserClickOnTray

  auto* bloom_controller = BloomController::Get();

  if (!bloom_controller->HasInteraction()) {
    SetIsActive(true);
    bloom_controller->StartInteraction();
    VLOG(1) << "Starting Bloom interaction";
  } else {
    SetIsActive(false);
    bloom_controller->StopInteraction(BloomInteractionResolution::kNormal);
    VLOG(1) << "Stopping Bloom interaction";
  }

  return true;
}

void BloomTray::SetIcon() {
  // TODO(jeroendh): Use correct icon;
  gfx::ImageSkia image = gfx::CreateVectorIcon(
      kShelfGlobeIcon,
      TrayIconColor(Shell::Get()->session_controller()->GetSessionState()));
  icon_->SetImage(image);
  const int vertical_padding = (kTrayItemSize - image.height()) / 2;
  const int horizontal_padding = (kTrayItemSize - image.width()) / 2;
  icon_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(vertical_padding, horizontal_padding)));
}

void BloomTray::SetTooltipText() {
  // TODO(jeroendh): Use correct tooltip;
  icon_->SetTooltipText(
      base::ASCIIToUTF16("Enable Bloom (THIS STRING MUST BE LOCALIZED)"));
}

BEGIN_METADATA(BloomTray, TrayBackgroundView)
END_METADATA

}  // namespace ash
