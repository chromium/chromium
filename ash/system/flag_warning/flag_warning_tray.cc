// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/flag_warning/flag_warning_tray.h"

#include <memory>

#include "ash/components/quick_launch/public/mojom/constants.mojom.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

FlagWarningTray::FlagWarningTray(Shelf* shelf) : shelf_(shelf) {
  DCHECK(shelf_);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  container_ = new TrayContainer(shelf);
  AddChildView(container_);

  button_ = views::MdTextButton::Create(this, base::string16(),
                                        CONTEXT_LAUNCHER_BUTTON);
  button_->SetProminent(true);
  button_->SetBgColorOverride(gfx::kGoogleYellow300);
  button_->SetEnabledTextColors(SK_ColorBLACK);
  button_->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  UpdateButton();
  container_->AddChildView(button_);
  SetVisible(true);
}

FlagWarningTray::~FlagWarningTray() = default;

void FlagWarningTray::UpdateAfterShelfAlignmentChange() {
  if (!container_)
    return;

  container_->UpdateAfterShelfAlignmentChange();
  UpdateButton();
}

void FlagWarningTray::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  DCHECK_EQ(button_, sender);

  // Open the quick launch mojo mini-app to demonstrate that mini-apps work.
  Shell::Get()->connector()->StartService(quick_launch::mojom::kServiceName);
}

void FlagWarningTray::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  View::GetAccessibleNodeData(node_data);
  node_data->SetName(button_->GetText());
}

void FlagWarningTray::UpdateButton() {
  if (!button_)
    return;

  // Only the horizontal shelf is wide enough for a text label. Use an icon for
  // vertical shelf alignments.
  if (!shelf_->IsHorizontalAlignment()) {
    button_->SetText(base::string16());
    button_->SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(kSystemMenuInfoIcon, SK_ColorBLACK));
    button_->SetMinSize(gfx::Size(kTrayItemSize, kTrayItemSize));
    return;
  }

  if (::features::IsSingleProcessMash()) {
    button_->SetText(base::ASCIIToUTF16("SPM"));
    button_->SetTooltipText(
        base::ASCIIToUTF16("Running with feature SingleProcessMash"));
  } else if (::features::IsMultiProcessMash()) {
    button_->SetText(base::ASCIIToUTF16("Mash"));
    button_->SetTooltipText(base::ASCIIToUTF16("Running with feature Mash"));
  } else {
    NOTREACHED();
  }
  button_->SetImage(views::Button::STATE_NORMAL, gfx::ImageSkia());
  button_->SetMinSize(gfx::Size(0, kTrayItemSize));
}

}  // namespace ash
