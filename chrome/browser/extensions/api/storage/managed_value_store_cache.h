// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_MANAGED_VALUE_STORE_CACHE_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_MANAGED_VALUE_STORE_CACHE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "extensions/browser/api/storage/settings_observer.h"
#include "extensions/browser/api/storage/value_store_cache.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace policy {
class PolicyMap;
}

namespace value_store {
class ValueStoreFactory;
}

namespace extensions {

class PolicyValueStore;

// A `ValueStoreCache` that manages a `PolicyValueStore` for each extension
// that uses the storage.managed namespace. This class observes policy changes
// and which extensions listen for storage.onChanged(), and sends the
// appropriate updates to the corresponding `PolicyValueStore` on the
// BACKEND sequence.
class ManagedValueStoreCache : public ValueStoreCache,
                               public policy::PolicyService::Observer {
 public:
  // `factory` is used to create databases for the `PolicyValueStore`s.
  // `observer` is invoked/notified when a `ValueStore` changes.
  ManagedValueStoreCache(Profile& profile,
                         scoped_refptr<value_store::ValueStoreFactory> factory,
                         SettingsChangedCallback observer);

  ManagedValueStoreCache(const ManagedValueStoreCache&) = delete;
  ManagedValueStoreCache& operator=(const ManagedValueStoreCache&) = delete;

  ~ManagedValueStoreCache() override;

  policy::PolicyDomain policy_domain() const;

  // `ValueStoreCache` implementation:
  void ShutdownOnUI() override;
  void RunWithValueStoreForExtension(
      StorageCallback callback,
      scoped_refptr<const Extension> extension) override;
  void DeleteStorageSoon(const ExtensionId& extension_id) override;

  // Returns the policy domain that should be used for the specified profile.
  static policy::PolicyDomain GetPolicyDomain(const Profile& profile);

 private:
  class ExtensionTracker;

  // `PolicyService::Observer` implementation:
  void OnPolicyServiceInitialized(policy::PolicyDomain domain) override;
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

  // Posted by `OnPolicyServiceInitialized()` to signal we can fulfill all
  // pending access requests.
  void InitializeOnBackend() VALID_CONTEXT_REQUIRED(backend_sequence_checker_);

  // Posted by `OnPolicyUpdated()` to update a `PolicyValueStore` on the backend
  // sequence.
  void UpdatePolicyOnBackend(const ExtensionId& extension_id,
                             const policy::PolicyMap& new_policy)
      VALID_CONTEXT_REQUIRED(backend_sequence_checker_);

  // Returns or creates a `PolicyValueStore` for `extension_id`.
  PolicyValueStore& GetOrCreateStore(const ExtensionId& extension_id)
      VALID_CONTEXT_REQUIRED(backend_sequence_checker_);

  // Returns true if a backing store has been created for `extension_id`.
  bool HasStore(const ExtensionId& extension_id) const
      VALID_CONTEXT_REQUIRED(backend_sequence_checker_);

  // The profile that owns the extension system being used.
  const raw_ref<Profile, AcrossTasksDanglingUntriaged> profile_
      GUARDED_BY_CONTEXT(ui_sequence_checker_);

  // The policy domain. This is used for observing the policy updates.
  const policy::PolicyDomain policy_domain_
      GUARDED_BY_CONTEXT(ui_sequence_checker_);

  // The `profile_`'s `PolicyService`.
  const raw_ref<policy::PolicyService, AcrossTasksDanglingUntriaged>
      policy_service_ GUARDED_BY_CONTEXT(ui_sequence_checker_);

  // Observes extension loading and unloading, and keeps the `Profile`'s
  // `PolicyService` aware of the current list of extensions.
  std::unique_ptr<ExtensionTracker> extension_tracker_
      GUARDED_BY_CONTEXT(ui_sequence_checker_);

  scoped_refptr<value_store::ValueStoreFactory> storage_factory_
      GUARDED_BY_CONTEXT(backend_sequence_checker_);
  SequenceBoundSettingsChangedCallback observer_
      GUARDED_BY_CONTEXT(backend_sequence_checker_);

  // All the `PolicyValueStore`s live on the FILE/backend thread, and
  // `store_map_` can be accessed only on this thread as well.
  std::map<std::string, std::unique_ptr<PolicyValueStore>> store_map_
      GUARDED_BY_CONTEXT(backend_sequence_checker_);

  // Tracks if the policy service is initialized (which is signalled by calling
  // `OnPolicyServiceInitialized()`.
  // We must store this in a local boolean because the policy service lives on
  // the UI thread and we need to check this on the backend thread.
  bool is_policy_service_initialized_
      GUARDED_BY_CONTEXT(backend_sequence_checker_) = false;

  // The storage callbacks that could not be fulfilled yet because the value
  // stores are not ready yet, which is the case until
  // `OnPolicyServiceInitialized()` is called.
  std::vector<std::pair<std::string, StorageCallback>>
      pending_storage_callbacks_;

  SEQUENCE_CHECKER(ui_sequence_checker_);
  SEQUENCE_CHECKER(backend_sequence_checker_);

  base::WeakPtrFactory<ManagedValueStoreCache> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_MANAGED_VALUE_STORE_CACHE_H_
