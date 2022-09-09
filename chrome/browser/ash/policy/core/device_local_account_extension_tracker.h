// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_EXTENSION_TRACKER_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_EXTENSION_TRACKER_H_

#include "base/memory/raw_ptr.h"
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
  DeviceLocalAccountExtensionTracker(const DeviceLocalAccount& account,
                                     CloudPolicyStore* store,
                                     SchemaRegistry* schema_registry);

  DeviceLocalAccountExtensionTracker(
      const DeviceLocalAccountExtensionTracker&) = delete;
  DeviceLocalAccountExtensionTracker& operator=(
      const DeviceLocalAccountExtensionTracker&) = delete;

  ~DeviceLocalAccountExtensionTracker() override;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

 private:
  void UpdateFromStore();

  raw_ptr<CloudPolicyStore> store_;
  raw_ptr<SchemaRegistry> schema_registry_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_EXTENSION_TRACKER_H_
