// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CBCM_INVALIDATIONS_INITIALIZER_H_
#define CHROME_BROWSER_POLICY_CBCM_INVALIDATIONS_INITIALIZER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {
class CloudPolicyClient;

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
  class MachineLevelDeviceAccountInitializerHelper;

  // Called by the DeviceAccountInitializer when the device service account is
  // ready.
  void AccountInitCallback(const std::string& account_email, bool success);

  raw_ptr<Delegate> delegate_;
  std::unique_ptr<MachineLevelDeviceAccountInitializerHelper>
      account_initializer_helper_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CBCM_INVALIDATIONS_INITIALIZER_H_
