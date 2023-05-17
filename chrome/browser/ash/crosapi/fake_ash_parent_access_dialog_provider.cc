// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/fake_ash_parent_access_dialog_provider.h"

#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"

namespace crosapi {

FakeAshParentAccessDialogProvider::FakeAshParentAccessDialogProvider() =
    default;

FakeAshParentAccessDialogProvider::~FakeAshParentAccessDialogProvider() =
    default;

ash::ParentAccessDialogProvider::ShowError
FakeAshParentAccessDialogProvider::Show(
    parent_access_ui::mojom::ParentAccessParamsPtr params,
    ash::ParentAccessDialog::Callback callback) {
  callback_ = std::move(callback);
  last_params_received_ = std::move(params);
  return next_show_error_;
}

void FakeAshParentAccessDialogProvider::SetNextShowError(
    ParentAccessDialogProvider::ShowError error) {
  next_show_error_ = error;
}

parent_access_ui::mojom::ParentAccessParamsPtr
FakeAshParentAccessDialogProvider::GetLastParamsReceived() {
  return std::move(last_params_received_);
}

void FakeAshParentAccessDialogProvider::TriggerCallbackWithResult(
    std::unique_ptr<ash::ParentAccessDialog::Result> result) {
  std::move(callback_).Run(std::move(result));
}

}  // namespace crosapi
