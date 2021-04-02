// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_EXTENSION_TRACKER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_EXTENSION_TRACKER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

namespace policy {

struct DeviceLocalAccount;
class SchemaRegistry;

// Helper class that keeps all the extensions that a device-local account uses
// registered in a SchemaRegistry.
// This makes it possible to precache the policy for extensions for public
// sessions before the session is started (e.g. during enrollment).
// Otherwise, the ComponentCloudPolicyService would ignore the
// PolicyFetchResponses from the DMServer because the SchemaRegistry for this
// account doesn't have this extension "installed".
class DeviceLocalAccountExtensionTracker : public CloudPolicyStore::Observer {
 public:
  DeviceLocalAccountExtensionTracker(
      const DeviceLocalAccount& account,
      CloudPolicyStore* store,
      SchemaRegistry* schema_registry);

  ~DeviceLocalAccountExtensionTracker() override;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

 private:
  void UpdateFromStore();

  CloudPolicyStore* store_;
  SchemaRegistry* schema_registry_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountExtensionTracker);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_LOCAL_ACCOUNT_EXTENSION_TRACKER_H_
