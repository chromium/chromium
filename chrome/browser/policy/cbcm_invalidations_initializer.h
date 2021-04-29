// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CBCM_INVALIDATIONS_INITIALIZER_H_
#define CHROME_BROWSER_POLICY_CBCM_INVALIDATIONS_INITIALIZER_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {
class CloudPolicyClient;

namespace {
class MachineLevelDeviceAccountInitializerHelper;
}

class CBCMInvalidationsInitializer {
 public:
  class Delegate {
   public:
    // Starts the services required for Policy Invalidations over FCM to be
    // enabled.
    virtual void StartInvalidations() = 0;

    // Return the SharedURLLoaderFactory to use to make GAIA requests.
    virtual scoped_refptr<network::SharedURLLoaderFactory>
    GetURLLoaderFactory() = 0;

    // Return |true| if the invalidations-related services managed by this
    // delegate are already running.
    virtual bool IsInvalidationsServiceStarted() const = 0;
  };

  explicit CBCMInvalidationsInitializer(Delegate* delegate);
  ~CBCMInvalidationsInitializer();

  void OnServiceAccountSet(CloudPolicyClient* client,
                           const std::string& account_email);

 private:
  // Called by the DeviceAccountInitializer when the device service account is
  // ready.
  void AccountInitCallback(const std::string& account_email, bool success);

  Delegate* delegate_;
  std::unique_ptr<MachineLevelDeviceAccountInitializerHelper>
      account_initializer_helper_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CBCM_INVALIDATIONS_INITIALIZER_H_
