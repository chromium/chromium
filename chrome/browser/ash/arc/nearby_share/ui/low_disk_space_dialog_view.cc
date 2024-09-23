// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/ui/low_disk_space_dialog_view.h"

#include <memory>

#include "ash/frame/non_client_frame_view_ash.h"
#include "base/i18n/message_formatter.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_resource_getter.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/views/view.h"

namespace arc {

LowDiskSpaceDialogView::LowDiskSpaceDialogView(views::View* anchor_view,
                                               int file_count,
                                               int64_t required_disk_space,
                                               OnCloseCallback close_callback)
    : BaseDialogDelegateView(anchor_view),
      close_callback_(std::move(close_callback)) {
  SetTitle(l10n_util::GetStringUTF16(
      IDS_ASH_ARC_NEARBY_SHARE_LOW_DISK_SPACE_DIALOG_TITLE));

  // Set up OK button
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_OK));
  SetAcceptCallback(base::BindOnce(
      [](LowDiskSpaceDialogView* dialog) {
        std::move(dialog->close_callback_).Run(/*should_open_storage=*/false);
      },
      base::Unretained(this)));

  // Set up Cancel button as "Storage" button
  SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(
          IDS_ASH_ARC_NEARBY_SHARE_LOW_DISK_SPACE_DIALOG_STORAGE_BUTTON));
  SetCancelCallback(base::BindOnce(
      [](LowDiskSpaceDialogView* dialog) {
        std::move(dialog->close_callback_).Run(/*should_open_storage=*/true);
      },
      base::Unretained(this)));

  std::u16string low_disk_space_dialog_message;
  if (features::IsNameEnabled()) {
    low_disk_space_dialog_message =
        base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(
                IDS_ASH_ARC_NEARBY_SHARE_LOW_DISK_SPACE_DIALOG_MESSAGE_PH),
            file_count,
            NearbyShareResourceGetter::GetInstance()->GetFeatureName(),
            ui::FormatBytes(required_disk_space));
  } else {
    low_disk_space_dialog_message = base::ReplaceStringPlaceholders(
        l10n_util::GetPluralStringFUTF16(
            IDS_ASH_ARC_NEARBY_SHARE_LOW_DISK_SPACE_DIALOG_MESSAGE, file_count),
        ui::FormatBytes(required_disk_space), /*offset=*/nullptr);
  }
  AddDialogMessage(low_disk_space_dialog_message);
}

LowDiskSpaceDialogView::~LowDiskSpaceDialogView() = default;

void LowDiskSpaceDialogView::Show(aura::Window* arc_window,
                                  int file_count,
                                  int64_t required_disk_space,
                                  OnCloseCallback callback) {
  DCHECK(arc_window);
  DCHECK(callback);

  DVLOG(1) << __func__;
  views::BubbleDialogDelegateView::CreateBubble(
      new LowDiskSpaceDialogView(ash::NonClientFrameViewAsh::Get(arc_window),
                                 file_count, required_disk_space,
                                 std::move(callback)))
      ->Show();
}

}  // namespace arc
