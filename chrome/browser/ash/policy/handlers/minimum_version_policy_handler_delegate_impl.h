// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_MINIMUM_VERSION_POLICY_HANDLER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_MINIMUM_VERSION_POLICY_HANDLER_DELEGATE_IMPL_H_

#include "base/version.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler.h"

namespace policy {

class MinimumVersionPolicyHandlerDelegateImpl
    : public MinimumVersionPolicyHandler::Delegate {
 public:
  MinimumVersionPolicyHandlerDelegateImpl();

  bool IsKioskMode() const override;
  bool IsDeviceEnterpriseManaged() const override;
  bool IsUserLoggedIn() const override;
  bool IsUserEnterpriseManaged() const override;
  bool IsLoginSessionState() const override;
  bool IsLoginInProgress() const override;
  void ShowUpdateRequiredScreen() override;
  void RestartToLoginScreen() override;
  void HideUpdateRequiredScreenIfShown() override;
  base::Version GetCurrentVersion() const override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_MINIMUM_VERSION_POLICY_HANDLER_DELEGATE_IMPL_H_
