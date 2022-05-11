// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_EXTERNAL_ACTION_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_EXTERNAL_ACTION_DELEGATE_H_

#include "components/autofill_assistant/browser/public/external_action_delegate.h"

class ApcExternalActionDelegate
    : public autofill_assistant::ExternalActionDelegate {
 public:
  ApcExternalActionDelegate();
  ApcExternalActionDelegate(const ApcExternalActionDelegate&) = delete;
  ApcExternalActionDelegate& operator=(const ApcExternalActionDelegate&) =
      delete;
  ~ApcExternalActionDelegate() = default;

  // ExternalActionDelegate
  void OnActionRequested(
      const autofill_assistant::external::Action& action_info,
      base::OnceCallback<void(
          autofill_assistant::ExternalActionDelegate::ActionResult)> callback)
      override;
};

#endif  // CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_EXTERNAL_ACTION_DELEGATE_H_
