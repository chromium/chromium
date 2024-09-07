// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/unified_media_controls_detailed_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tri_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"

namespace ash {

UnifiedMediaControlsDetailedView::UnifiedMediaControlsDetailedView(
    DetailedViewDelegate* delegate,
    std::unique_ptr<views::View> notification_list_view)
    : TrayDetailedView(delegate) {
  CreateTitleRow(IDS_ASH_GLOBAL_MEDIA_CONTROLS_TITLE);

  notification_list_view->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, 0, kMediaNotificationListViewBottomPadding, 0)));

  AddChildView(std::move(notification_list_view));
}

void UnifiedMediaControlsDetailedView::CreateExtraTitleRowButtons() {
  tri_view()->SetContainerVisible(TriView::Container::END, true);
  tri_view()->AddView(TriView::Container::END, new MediaTray::PinButton());
}

BEGIN_METADATA(UnifiedMediaControlsDetailedView)
END_METADATA

}  // namespace ash
