// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/device_local_account_external_policy_loader.h"

#include <utility>

#include "base/callback.h"
#include "base/check_op.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/external_cache_impl.h"
#include "chrome/browser/extensions/policy_handlers.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "components/policy/core/common/policy_map.h"
#include "components/prefs/pref_value_map.h"
#include "extensions/browser/pref_names.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

DeviceLocalAccountExternalPolicyLoader::
    DeviceLocalAccountExternalPolicyLoader(policy::CloudPolicyStore* store,
                                           const base::FilePath& cache_dir)
    : store_(store),
      cache_dir_(cache_dir) {
}

bool DeviceLocalAccountExternalPolicyLoader::IsCacheRunning() const {
  return external_cache_ != nullptr;
}

void DeviceLocalAccountExternalPolicyLoader::StartCache(
    const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner) {
  DCHECK(!external_cache_);
  store_->AddObserver(this);

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory =
      g_browser_process->shared_url_loader_factory();
  external_cache_ = std::make_unique<ExternalCacheImpl>(
      cache_dir_, shared_url_loader_factory, cache_task_runner, this,
      true /* always_check_updates */,
      false /* wait_for_cache_initialization */);

  if (store_->is_initialized())
    UpdateExtensionListFromStore();
}

void DeviceLocalAccountExternalPolicyLoader::StopCache(
    base::OnceClosure callback) {
  if (external_cache_) {
    external_cache_->Shutdown(std::move(callback));
    external_cache_.reset();
    store_->RemoveObserver(this);
  }

  base::DictionaryValue empty_prefs;
  OnExtensionListsUpdated(&empty_prefs);
}

void DeviceLocalAccountExternalPolicyLoader::StartLoading() {
  DCHECK(has_owner());

  // Through OnExtensionListsUpdated(), |prefs_| might have already loaded but
  // not consumed because we didn't have an owner then. Pass |prefs_| in that
  // case.
  if (prefs_)
    LoadFinished(std::move(prefs_));
}

void DeviceLocalAccountExternalPolicyLoader::OnStoreLoaded(
    policy::CloudPolicyStore* store) {
  DCHECK(external_cache_);
  DCHECK_EQ(store_, store);
  UpdateExtensionListFromStore();
}

void DeviceLocalAccountExternalPolicyLoader::OnStoreError(
    policy::CloudPolicyStore* store) {
  DCHECK(external_cache_);
  DCHECK_EQ(store_, store);
}

void DeviceLocalAccountExternalPolicyLoader::OnExtensionListsUpdated(
    const base::DictionaryValue* prefs) {
  DCHECK(external_cache_ || prefs->empty());
  prefs_ = prefs->CreateDeepCopy();
  // Only call LoadFinished() when there is an owner to consume |prefs_|.
  if (has_owner())
    LoadFinished(std::move(prefs_));
}

ExternalCache*
DeviceLocalAccountExternalPolicyLoader::GetExternalCacheForTesting() {
  return external_cache_.get();
}

DeviceLocalAccountExternalPolicyLoader::
    ~DeviceLocalAccountExternalPolicyLoader() {
  DCHECK(!external_cache_);
}

void DeviceLocalAccountExternalPolicyLoader::UpdateExtensionListFromStore() {
  std::unique_ptr<base::DictionaryValue> prefs(new base::DictionaryValue);
  const policy::PolicyMap& policy_map = store_->policy_map();
  // TODO(binjin): Use two policy handlers here after
  // ExtensionManagementPolicyHandler is introduced.
  extensions::ExtensionInstallForceListPolicyHandler policy_handler;
  if (policy_handler.CheckPolicySettings(policy_map, NULL)) {
    PrefValueMap pref_value_map;
    policy_handler.ApplyPolicySettings(policy_map, &pref_value_map);
    const base::Value* value = NULL;
    const base::DictionaryValue* dict = NULL;
    if (pref_value_map.GetValue(extensions::pref_names::kInstallForceList,
                                &value) &&
        value->GetAsDictionary(&dict)) {
      prefs.reset(dict->DeepCopy());
    }
  }

  external_cache_->UpdateExtensionsList(std::move(prefs));
}

}  // namespace chromeos
