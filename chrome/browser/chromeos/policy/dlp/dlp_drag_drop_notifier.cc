// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_drag_drop_notifier.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/notreached.h"
#include "chrome/browser/chromeos/policy/dlp/clipboard_bubble.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_clipboard_bubble_constants.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

DlpDragDropNotifier::DlpDragDropNotifier() = default;
DlpDragDropNotifier::~DlpDragDropNotifier() = default;

void DlpDragDropNotifier::NotifyBlockedAction(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst) {
  DCHECK(data_src);
  DCHECK(data_src->GetURL());
  const std::u16string host_name =
      base::UTF8ToUTF16(data_src->GetURL()->host());

  ShowBlockBubble(l10n_util::GetStringFUTF16(
      IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_PASTE, host_name));
}

void DlpDragDropNotifier::WarnOnDrop(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst,
    base::OnceClosure drop_cb) {
  DCHECK(data_src);
  DCHECK(data_src->GetURL());

  CloseWidget(widget_.get(), views::Widget::ClosedReason::kUnspecified);

  const std::u16string host_name =
      base::UTF8ToUTF16(data_src->GetURL()->host());

  drop_cb_ = std::move(drop_cb);
  auto proceed_cb = base::BindRepeating(&DlpDragDropNotifier::ProceedPressed,
                                        base::Unretained(this));
  auto cancel_cb = base::BindRepeating(&DlpDragDropNotifier::CancelPressed,
                                       base::Unretained(this));

  ShowWarningBubble(l10n_util::GetStringFUTF16(
                        IDS_POLICY_DLP_CLIPBOARD_WARN_ON_PASTE, host_name),
                    std::move(proceed_cb), std::move(cancel_cb));
}

void DlpDragDropNotifier::ProceedPressed(views::Widget* widget) {
  if (drop_cb_)
    std::move(drop_cb_).Run();
  CloseWidget(widget, views::Widget::ClosedReason::kAcceptButtonClicked);
}

void DlpDragDropNotifier::CancelPressed(views::Widget* widget) {
  CloseWidget(widget, views::Widget::ClosedReason::kCancelButtonClicked);
}

void DlpDragDropNotifier::OnWidgetClosing(views::Widget* widget) {
  drop_cb_.Reset();

  DlpDataTransferNotifier::OnWidgetClosing(widget);
}

}  // namespace policy
