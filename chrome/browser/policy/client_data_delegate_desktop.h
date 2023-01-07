// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLIENT_DATA_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_POLICY_CLIENT_DATA_DELEGATE_DESKTOP_H_

#include "components/policy/core/common/cloud/client_data_delegate.h"

namespace policy {

// Sets Desktop-specific fields in request protos for the DMServer.
class ClientDataDelegateDesktop : public ClientDataDelegate {
 public:
  ClientDataDelegateDesktop() = default;
  ClientDataDelegateDesktop(const ClientDataDelegateDesktop&) = delete;
  ClientDataDelegateDesktop& operator=(const ClientDataDelegateDesktop&) =
      delete;
  ~ClientDataDelegateDesktop() override = default;

  void FillRegisterBrowserRequest(
      enterprise_management::RegisterBrowserRequest* request,
      base::OnceClosure callback) const override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLIENT_DATA_DELEGATE_DESKTOP_H_
