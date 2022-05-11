// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_external_action_delegate.h"

// TODO(crbug.comgit /1324089): Implement once the side panel and
// UpdateDesktopSideAction are available.
ApcExternalActionDelegate::ApcExternalActionDelegate() = default;

void ApcExternalActionDelegate::OnActionRequested(
    const autofill_assistant::external::Action& action_info,
    base::OnceCallback<void(
        autofill_assistant::ExternalActionDelegate::ActionResult)> callback) {
  autofill_assistant::ExternalActionDelegate::ActionResult result;
  result.success = true;
  std::move(callback).Run(result);
}
