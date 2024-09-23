// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/informed_restore_test_api.h"

#include "ash/shell.h"
#include "ash/style/system_dialog_delegate_view.h"
#include "ash/wm/window_restore/informed_restore_contents_data.h"
#include "ash/wm/window_restore/informed_restore_controller.h"
#include "ui/views/view_utils.h"

namespace ash {

InformedRestoreTestApi::InformedRestoreTestApi() = default;

InformedRestoreTestApi::~InformedRestoreTestApi() = default;

void InformedRestoreTestApi::SetInformedRestoreContentsDataForTesting(
    std::unique_ptr<InformedRestoreContentsData> contents_data) {
  Shell::Get()->informed_restore_controller()->contents_data_ =
      std::move(contents_data);
}

SystemDialogDelegateView* InformedRestoreTestApi::GetOnboardingDialog() {
  auto* onboarding_widget =
      Shell::Get()->informed_restore_controller()->onboarding_widget_.get();
  return onboarding_widget ? views::AsViewClass<SystemDialogDelegateView>(
                                 onboarding_widget->GetContentsView())
                           : nullptr;
}

}  // namespace ash
