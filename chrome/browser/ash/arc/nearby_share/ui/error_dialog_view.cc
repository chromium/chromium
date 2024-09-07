// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/ui/error_dialog_view.h"

#include <memory>

#include "ash/frame/non_client_frame_view_ash.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_resource_getter.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/view.h"

namespace arc {

ErrorDialogView::ErrorDialogView(views::View* anchor_view,
                                 base::OnceClosure close_callback)
    : BaseDialogDelegateView(anchor_view) {
  SetShowTitle(false);

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  // Set up OK button
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_CLOSE));
  SetAcceptCallback(std::move(close_callback));

  AddDialogMessage(
      features::IsNameEnabled()
          ? NearbyShareResourceGetter::GetInstance()->GetStringWithFeatureName(
                IDS_ASH_ARC_NEARBY_SHARE_ERROR_DIALOG_MESSAGE_PH)
          : l10n_util::GetStringUTF16(
                IDS_ASH_ARC_NEARBY_SHARE_ERROR_DIALOG_MESSAGE));
}

ErrorDialogView::~ErrorDialogView() = default;

void ErrorDialogView::Show(aura::Window* arc_window,
                           base::OnceClosure callback) {
  views::BubbleDialogDelegateView::CreateBubble(
      new ErrorDialogView(ash::NonClientFrameViewAsh::Get(arc_window),
                          std::move(callback)))
      ->Show();
}

}  // namespace arc
