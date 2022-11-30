// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLIENT_DATA_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_POLICY_CLIENT_DATA_DELEGATE_ANDROID_H_

#include "components/policy/core/common/cloud/client_data_delegate.h"

namespace policy {

// Sets Android-specific fields in request protos for the DMServer.
class ClientDataDelegateAndroid : public ClientDataDelegate {
 public:
  ClientDataDelegateAndroid() = default;
  ClientDataDelegateAndroid(const ClientDataDelegateAndroid&) = delete;
  ClientDataDelegateAndroid& operator=(const ClientDataDelegateAndroid&) =
      delete;
  ~ClientDataDelegateAndroid() override = default;

  void FillRegisterBrowserRequest(
      enterprise_management::RegisterBrowserRequest* request,
      base::OnceClosure callback) const override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLIENT_DATA_DELEGATE_ANDROID_H_
