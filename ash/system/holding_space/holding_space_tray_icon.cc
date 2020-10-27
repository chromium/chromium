// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray_icon.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

HoldingSpaceTrayIcon::HoldingSpaceTrayIcon() {
  InitLayout();
}

HoldingSpaceTrayIcon::~HoldingSpaceTrayIcon() = default;

int HoldingSpaceTrayIcon::GetPreferredMainAxisMargin() const {
  return kHoldingSpaceTrayIconMainAxisMargin;
}

void HoldingSpaceTrayIcon::OnLocaleChanged() {
  TooltipTextChanged();
}

base::string16 HoldingSpaceTrayIcon::GetTooltipText(
    const gfx::Point& point) const {
  return l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_TITLE);
}

void HoldingSpaceTrayIcon::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Image.
  auto* image_view = AddChildView(std::make_unique<views::ImageView>());
  image_view->SetImage(
      gfx::CreateVectorIcon(kHoldingSpaceIcon, kHoldingSpaceTrayIconSize,
                            ShelfConfig::Get()->shelf_icon_color()));

  // Disallow events on `image_view` so that tooltips will be retrieved from
  // `this`. Moving forward, `image_view` will not exist as we transition to a
  // more content forward tray icon.
  image_view->SetCanProcessEventsWithinSubtree(false);
}

BEGIN_METADATA(HoldingSpaceTrayIcon, views::View)
END_METADATA

}  // namespace ash
