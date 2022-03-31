// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/edit_mode_exit_view.h"

#include "ash/style/pill_button.h"
#include "base/bind.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace arc {
namespace input_overlay {

namespace {
// General sizes.
constexpr int kMenuWidth = 140;
constexpr int kMenuHeight = 184;

// Individual entry size.
constexpr int kButtonHeight = 56;

// Spaces in between and misc.
constexpr int kSpaceRow = 8;
}  // namespace

// static
std::unique_ptr<EditModeExitView> EditModeExitView::BuildView(
    DisplayOverlayController* display_overlay_controller,
    gfx::Point position) {
  auto menu_view_ptr =
      std::make_unique<EditModeExitView>(display_overlay_controller);
  menu_view_ptr->Init(position);

  return menu_view_ptr;
}

EditModeExitView::EditModeExitView(
    DisplayOverlayController* display_overlay_controller)
    : display_overlay_controller_(display_overlay_controller) {}

EditModeExitView::~EditModeExitView() {}

void EditModeExitView::Init(gfx::Point position) {
  // TODO(djacobo): Set proper fonts, also check if states needs to be manually
  // tuned or if whatever its done by default works fine.
  DCHECK(display_overlay_controller_);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetDefault(views::kMarginsKey, gfx::Insets::VH(kSpaceRow, 0));
  SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  SetSize(gfx::Size(kMenuWidth, kMenuHeight));

  reset_button_ = AddChildView(std::make_unique<ash::PillButton>(
      base::BindRepeating(&EditModeExitView::OnResetButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_EDIT_MODE_RESET),
      ash::PillButton::Type::kIconless,
      /*icon=*/nullptr));
  reset_button_->SetSize(gfx::Size(kMenuWidth, kButtonHeight));
  reset_button_->SetButtonTextColor(gfx::kGoogleGrey200);

  save_button_ = AddChildView(std::make_unique<ash::PillButton>(
      base::BindRepeating(&EditModeExitView::OnSaveButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_EDIT_MODE_SAVE),
      ash::PillButton::Type::kIconless,
      /*icon=*/nullptr));
  save_button_->SetSize(gfx::Size(kMenuWidth, kButtonHeight));
  save_button_->SetBackgroundColor(gfx::kGoogleBlue300);
  save_button_->SetButtonTextColor(gfx::kGoogleGrey900);

  cancel_button_ = AddChildView(std::make_unique<ash::PillButton>(
      base::BindRepeating(&EditModeExitView::OnCancelButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_EDIT_MODE_CANCEL),
      ash::PillButton::Type::kIconless,
      /*icon=*/nullptr));
  cancel_button_->SetSize(gfx::Size(kMenuWidth, kButtonHeight));
  cancel_button_->SetButtonTextColor(gfx::kGoogleGrey200);

  SetPosition(position);
}

void EditModeExitView::OnResetButtonPressed() {
  DCHECK(display_overlay_controller_);
  if (!display_overlay_controller_)
    return;
  display_overlay_controller_->OnCustomizeRestore();
}

void EditModeExitView::OnSaveButtonPressed() {
  DCHECK(display_overlay_controller_);
  if (!display_overlay_controller_)
    return;
  display_overlay_controller_->OnCustomizeSave();
}

void EditModeExitView::OnCancelButtonPressed() {
  DCHECK(display_overlay_controller_);
  if (!display_overlay_controller_)
    return;
  display_overlay_controller_->OnCustomizeCancel();
}

}  // namespace input_overlay
}  // namespace arc
