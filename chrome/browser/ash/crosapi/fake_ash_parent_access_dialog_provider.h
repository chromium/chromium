// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_FAKE_ASH_PARENT_ACCESS_DIALOG_PROVIDER_H_
#define CHROME_BROWSER_ASH_CROSAPI_FAKE_ASH_PARENT_ACCESS_DIALOG_PROVIDER_H_

#include <memory>

#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.mojom-forward.h"

namespace crosapi {

// Mock class for unit test usage.
class FakeAshParentAccessDialogProvider
    : public ash::ParentAccessDialogProvider {
 public:
  FakeAshParentAccessDialogProvider();
  ~FakeAshParentAccessDialogProvider() override;

  // ParentAccessDialogProvider:
  ash::ParentAccessDialogProvider::ShowError Show(
      parent_access_ui::mojom::ParentAccessParamsPtr params,
      ash::ParentAccessDialog::Callback callback) override;

  // Sets the next error that `Show` returns.
  void SetNextShowError(ParentAccessDialogProvider::ShowError error);

  // Getter for `last_params_received_`.
  parent_access_ui::mojom::ParentAccessParamsPtr GetLastParamsReceived();

  // Triggers `callback_` execution with a given result.
  void TriggerCallbackWithResult(
      std::unique_ptr<ash::ParentAccessDialog::Result> result);

 private:
  ash::ParentAccessDialog::Callback callback_;
  parent_access_ui::mojom::ParentAccessParamsPtr last_params_received_;
  ParentAccessDialogProvider::ShowError next_show_error_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_FAKE_ASH_PARENT_ACCESS_DIALOG_PROVIDER_H_
