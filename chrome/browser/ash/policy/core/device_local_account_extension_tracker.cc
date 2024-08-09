// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_local_account_extension_tracker.h"

#include "base/notreached.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/extensions/policy_handlers.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/prefs/pref_value_map.h"
#include "extensions/browser/pref_names.h"

namespace policy {

DeviceLocalAccountExtensionTracker::DeviceLocalAccountExtensionTracker(
    const DeviceLocalAccount& account,
    CloudPolicyStore* store,
    SchemaRegistry* schema_registry)
    : store_(store), schema_registry_(schema_registry) {
  switch (account.type) {
    case DeviceLocalAccountType::kKioskApp: {
      // This is easy: Just add a component for the app id.
      PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, account.kiosk_app_id);
      schema_registry_->RegisterComponent(ns, Schema());
      break;
    }
    case DeviceLocalAccountType::kPublicSession:
    case DeviceLocalAccountType::kSamlPublicSession:
      // For public sessions, track the value of the ExtensionInstallForcelist
      // policy.
      store_->AddObserver(this);
      UpdateFromStore();
      break;
    case DeviceLocalAccountType::kWebKioskApp:
    case DeviceLocalAccountType::kKioskIsolatedWebApp:
      NOTREACHED_IN_MIGRATION();
  }
}

DeviceLocalAccountExtensionTracker::~DeviceLocalAccountExtensionTracker() {
  store_->RemoveObserver(this);
}

void DeviceLocalAccountExtensionTracker::OnStoreLoaded(
    CloudPolicyStore* store) {
  UpdateFromStore();
}

void DeviceLocalAccountExtensionTracker::OnStoreError(CloudPolicyStore* store) {
  UpdateFromStore();
}

void DeviceLocalAccountExtensionTracker::UpdateFromStore() {
  const PolicyMap& policy_map = store_->policy_map();

  // TODO(binjin): Use two policy handlers here after
  // ExtensionManagementPolicyHandler is introduced.
  extensions::ExtensionInstallForceListPolicyHandler policy_handler;
  if (!policy_handler.CheckPolicySettings(policy_map, nullptr))
    return;

  PrefValueMap pref_value_map;
  policy_handler.ApplyPolicySettings(policy_map, &pref_value_map);

  const base::Value* value = nullptr;
  if (!pref_value_map.GetValue(extensions::pref_names::kInstallForceList,
                               &value) ||
      !value->is_dict()) {
    return;
  }

  for (const auto item : value->GetDict()) {
    PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, item.first);
    if (schema_registry_->schema_map()->GetSchema(ns)) {
      // Important detail: Don't register the component again if it already
      // has a schema! If the session already started for this public session
      // then the real Schema for the extension has already been set by the
      // ManagedValueStoreCache::ExtensionTracker. Do not override that schema
      // with an invalid one now, or the policy for the extension will be
      // dropped.
      // However, if the forcelist is updated then we need to register the new
      // component ID so that its remote policy data can be fetched.
      continue;
    }
    schema_registry_->RegisterComponent(ns, Schema());
  }

  // Removing an extension from a public session at runtime can happen but is
  // a rare event. In that case we leave the extension ID in the SchemaRegistry,
  // and it will be purged on the next restart.
}

}  // namespace policy
