// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_drag_drop_notifier.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

DlpDragDropNotifier::DlpDragDropNotifier() = default;
DlpDragDropNotifier::~DlpDragDropNotifier() = default;

void DlpDragDropNotifier::NotifyBlockedAction(
    base::optional_ref<const ui::DataTransferEndpoint> data_src,
    base::optional_ref<const ui::DataTransferEndpoint> data_dst) {
  DCHECK(data_src.has_value());
  DCHECK(data_src->GetURL());
  const std::u16string host_name =
      base::UTF8ToUTF16(data_src->GetURL()->host());

  ShowBlockBubble(l10n_util::GetStringFUTF16(
      IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_PASTE, host_name));
}

void DlpDragDropNotifier::WarnOnDrop(
    base::optional_ref<const ui::DataTransferEndpoint> data_src,
    base::optional_ref<const ui::DataTransferEndpoint> data_dst,
    base::OnceClosure drop_cb) {
  DCHECK(data_src.has_value());
  DCHECK(data_src->GetURL());

  CloseWidget(widget_.get(), views::Widget::ClosedReason::kUnspecified);

  const std::u16string host_name =
      base::UTF8ToUTF16(data_src->GetURL()->host());

  auto proceed_cb = base::BindOnce(&DlpDragDropNotifier::ProceedPressed,
                                   base::Unretained(this));
  auto cancel_cb = base::BindOnce(&DlpDragDropNotifier::CancelPressed,
                                  base::Unretained(this));

  ShowWarningBubble(l10n_util::GetStringFUTF16(
                        IDS_POLICY_DLP_CLIPBOARD_WARN_ON_PASTE, host_name),
                    std::move(proceed_cb), std::move(cancel_cb));

  SetPasteCallback(base::BindOnce(
      [](base::OnceClosure paste_cb, bool drop) {
        if (drop)
          std::move(paste_cb).Run();
      },
      std::move(drop_cb)));
}

void DlpDragDropNotifier::ProceedPressed(views::Widget* widget) {
  RunPasteCallback();
  CloseWidget(widget, views::Widget::ClosedReason::kAcceptButtonClicked);
}

void DlpDragDropNotifier::CancelPressed(views::Widget* widget) {
  CloseWidget(widget, views::Widget::ClosedReason::kCancelButtonClicked);
}

}  // namespace policy
