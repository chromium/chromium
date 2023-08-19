// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_disabled_detailed_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/color_util.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/check.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

using views::BoxLayout;
using views::ImageView;
using views::Label;

// The desired label baseline. Top padding is added that is equal to the
// difference between this value and the actual label baseline value.
const int kDesiredLabelBaselineY = 20;

}  // namespace

BluetoothDisabledDetailedView::BluetoothDisabledDetailedView() {
  DCHECK(!features::IsQsRevampEnabled());
  std::unique_ptr<BoxLayout> box_layout =
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical);
  box_layout->set_main_axis_alignment(BoxLayout::MainAxisAlignment::kCenter);
  SetLayoutManager(std::move(box_layout));

  ImageView* image_view =
      AddChildView(std::make_unique<ImageView>(ui::ImageModel::FromVectorIcon(
          kSystemMenuBluetoothDisabledIcon, kColorAshButtonIconDisabledColor)));
  image_view->SetVerticalAlignment(ImageView::Alignment::kTrailing);

  Label* label = AddChildView(std::make_unique<Label>(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED)));
  TrayPopupUtils::SetLabelFontList(
      label, TrayPopupUtils::FontStyle::kDetailedViewLabel);
  label->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kDesiredLabelBaselineY - label->GetBaseline(), 0, 0, 0)));
  label->SetEnabledColorId(kColorAshTextColorPrimary);

  // Make top padding of the icon equal to the height of the label so that the
  // icon is vertically aligned to center of the container.
  image_view->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(label->GetPreferredSize().height(), 0, 0, 0)));
}

const char* BluetoothDisabledDetailedView::GetClassName() const {
  return "BluetoothDisabledDetailedView";
}

}  // namespace ash
