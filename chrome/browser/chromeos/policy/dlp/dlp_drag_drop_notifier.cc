// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_drag_drop_notifier.h"

#include <memory>

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
  DCHECK(data_src->origin());
  const std::u16string host_name =
      base::UTF8ToUTF16(data_src->origin()->host());

  ShowBlockBubble(l10n_util::GetStringFUTF16(
      IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_PASTE, host_name));
}
}  // namespace policy
