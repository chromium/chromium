// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/remove_query_confirmation_dialog.h"

#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {

RemoveQueryConfirmationDialog::RemoveQueryConfirmationDialog(
    RemovalConfirmationCallback confirm_callback,
    const std::u16string& result_title)
    : confirm_callback_(std::move(confirm_callback)) {
  SetModalType(ui::mojom::ModalType::kWindow);
  SetTitleText(
      l10n_util::GetStringUTF16(IDS_REMOVE_ZERO_STATE_SUGGESTION_TITLE));
  SetDescription(l10n_util::GetStringFUTF16(
      IDS_REMOVE_ZERO_STATE_SUGGESTION_DETAILS, result_title));
  SetAcceptButtonText(
      l10n_util::GetStringUTF16(IDS_REMOVE_SUGGESTION_BUTTON_LABEL));
  SetAcceptButtonVisible(true);

  SetCancelButtonText(l10n_util::GetStringUTF16(IDS_APP_CANCEL));
  SetCancelButtonVisible(true);

  auto run_callback = [](RemoveQueryConfirmationDialog* dialog, bool accept) {
    if (!dialog->confirm_callback_)
      return;

    if (accept) {
      Shell::Get()
          ->accessibility_controller()
          ->TriggerAccessibilityAlertWithMessage(
              l10n_util::GetStringUTF8(IDS_REMOVE_SUGGESTION_ANNOUNCEMENT));
    }

    std::move(dialog->confirm_callback_).Run(accept);

    dialog->GetWidget()->CloseWithReason(
        accept ? views::Widget::ClosedReason::kAcceptButtonClicked
               : views::Widget::ClosedReason::kCancelButtonClicked);
  };

  SetAcceptCallback(base::BindOnce(run_callback, base::Unretained(this), true));
  SetCancelCallback(
      base::BindOnce(run_callback, base::Unretained(this), false));
}

RemoveQueryConfirmationDialog::~RemoveQueryConfirmationDialog() = default;

BEGIN_METADATA(RemoveQueryConfirmationDialog)
END_METADATA

}  // namespace ash
