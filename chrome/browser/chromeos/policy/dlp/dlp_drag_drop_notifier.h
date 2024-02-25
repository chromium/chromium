// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_DRAG_DROP_NOTIFIER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_DRAG_DROP_NOTIFIER_H_

#include "chrome/browser/chromeos/policy/dlp/dlp_data_transfer_notifier.h"

namespace policy {

class DlpDragDropNotifier : public DlpDataTransferNotifier {
 public:
  DlpDragDropNotifier();
  ~DlpDragDropNotifier() override;

  DlpDragDropNotifier(const DlpDragDropNotifier&) = delete;
  void operator=(const DlpDragDropNotifier&) = delete;

  // DlpDataTransferNotifier::
  void NotifyBlockedAction(
      base::optional_ref<const ui::DataTransferEndpoint> data_src,
      base::optional_ref<const ui::DataTransferEndpoint> data_dst) override;

  // Warns the user that this drop action is not recommended.
  void WarnOnDrop(base::optional_ref<const ui::DataTransferEndpoint> data_src,
                  base::optional_ref<const ui::DataTransferEndpoint> data_dst,
                  base::OnceClosure drop_cb);

 protected:
  // Added as protected so tests can refer to them.
  void ProceedPressed(views::Widget* widget);

  void CancelPressed(views::Widget* widget);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_DRAG_DROP_NOTIFIER_H_
