// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/managed_value_store_cache.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/one_shot_event.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/storage/policy_value_store.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/storage/storage_schema_manifest_handler.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/value_store/value_store_factory.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/value_store_util.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

using content::BrowserContext;
using content::BrowserThread;

namespace extensions {
class ExtensionRegistry;

namespace {

// Only extension settings are stored in the managed namespace - not apps.
const value_store_util::ModelType kManagedModelType =
    value_store_util::ModelType::EXTENSION;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
/// ExtensionTracker
////////////////////////////////////////////////////////////////////////////////

// This helper observes initialization of all the installed extensions and
// subsequent loads and unloads, and keeps the SchemaRegistry of the Profile
// in sync with the current list of extensions. This allows the PolicyService
// to fetch cloud policy for those extensions, and allows its providers to
// selectively load only extension policy that has users.
class ManagedValueStoreCache::ExtensionTracker
    : public ExtensionRegistryObserver {
 public:
  ExtensionTracker(Profile* profile, policy::PolicyDomain policy_domain);

  ExtensionTracker(const ExtensionTracker&) = delete;
  ExtensionTracker& operator=(const ExtensionTracker&) = delete;

  ~ExtensionTracker() override = default;

 private:
  // ExtensionRegistryObserver implementation.
  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              extensions::UninstallReason reason) override;

  // Handler for the signal from ExtensionSystem::ready().
  void OnExtensionsReady();

  // Starts a schema load for all extensions that use managed storage.
  void LoadSchemas(ExtensionSet added);

  bool UsesManagedStorage(const Extension* extension) const;

  // Loads the schemas of the |extensions| and passes a ComponentMap to
  // Register().
  static void LoadSchemasOnFileTaskRunner(ExtensionSet extensions,
                                          base::WeakPtr<ExtensionTracker> self);
  void Register(const policy::ComponentMap* components);

  raw_ptr<Profile> profile_;
  policy::PolicyDomain policy_domain_;
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
  raw_ptr<policy::SchemaRegistry> schema_registry_;
  base::WeakPtrFactory<ExtensionTracker> weak_factory_{this};
};

ManagedValueStoreCache::ExtensionTracker::ExtensionTracker(
    Profile* profile,
    policy::PolicyDomain policy_domain)
    : profile_(profile),
      policy_domain_(policy_domain),
      schema_registry_(profile->GetPolicySchemaRegistryService()->registry()) {
  extension_registry_observation_.Observe(ExtensionRegistry::Get(profile_));
  // Load schemas when the extension system is ready. It might be ready now.
  ExtensionSystem::Get(profile_)->ready().Post(
      FROM_HERE, base::BindOnce(&ExtensionTracker::OnExtensionsReady,
                                weak_factory_.GetWeakPtr()));
}

void ManagedValueStoreCache::ExtensionTracker::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    const std::string& old_name) {
  // Some extensions are installed on the first run before the ExtensionSystem
  // becomes ready. Wait until all of them are ready before registering the
  // schemas of managed extensions, so that the policy loaders are reloaded at
  // most once.
  if (!ExtensionSystem::Get(profile_)->ready().is_signaled())
    return;
  ExtensionSet added;
  added.Insert(extension);
  LoadSchemas(std::move(added));
}

void ManagedValueStoreCache::ExtensionTracker::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  if (!ExtensionSystem::Get(profile_)->ready().is_signaled())
    return;
  if (extension && UsesManagedStorage(extension)) {
    schema_registry_->UnregisterComponent(
        policy::PolicyNamespace(policy_domain_, extension->id()));
  }
}

void ManagedValueStoreCache::ExtensionTracker::OnExtensionsReady() {
  // Load schemas for all installed extensions.
  LoadSchemas(
      ExtensionRegistry::Get(profile_)->GenerateInstalledExtensionsSet());
}

void ManagedValueStoreCache::ExtensionTracker::LoadSchemas(ExtensionSet added) {
  // Filter out extensions that don't use managed storage.
  ExtensionSet::const_iterator it = added.begin();
  while (it != added.end()) {
    std::string to_remove;
    if (!UsesManagedStorage(it->get()))
      to_remove = (*it)->id();
    ++it;
    if (!to_remove.empty())
      added.Remove(to_remove);
  }

  GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ExtensionTracker::LoadSchemasOnFileTaskRunner,
                                std::move(added), weak_factory_.GetWeakPtr()));
}

bool ManagedValueStoreCache::ExtensionTracker::UsesManagedStorage(
    const Extension* extension) const {
  return extension->manifest()->FindPath(manifest_keys::kStorageManagedSchema);
}

// static
void ManagedValueStoreCache::ExtensionTracker::LoadSchemasOnFileTaskRunner(
    ExtensionSet extensions,
    base::WeakPtr<ExtensionTracker> self) {
  auto components = std::make_unique<policy::ComponentMap>();

  for (const auto& extension : extensions) {
    if (!extension->manifest()->FindStringPath(
            manifest_keys::kStorageManagedSchema)) {
      // TODO(joaodasilva): Remove this. http://crbug.com/325349
      (*components)[extension->id()] = policy::Schema();
      continue;
    }
    // The extension should have been validated, so assume the schema exists
    // and is valid. If the schema is invalid then proceed with an empty schema.
    // The extension will be listed in chrome://policy but won't be able to load
    // any policies.
    (*components)[extension->id()] =
        StorageSchemaManifestHandler::GetSchema(extension.get())
            .value_or(policy::Schema());
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ExtensionTracker::Register, self,
                                base::Owned(components.release())));
}

void ManagedValueStoreCache::ExtensionTracker::Register(
    const policy::ComponentMap* components) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  schema_registry_->RegisterComponents(policy_domain_, *components);

  // The first SetExtensionsDomainsReady() call is performed after the
  // ExtensionSystem is ready, even if there are no managed extensions. It will
  // trigger a loading of the initial policy for any managed extensions, and
  // eventually the PolicyService will become ready for policy for extensions,
  // and OnPolicyServiceInitialized() will be invoked.
  // Subsequent calls to SetExtensionsDomainsReady() are ignored.
  //
  // Note that there is only ever one |ManagedValueStoreCache| instance for each
  // profile, regardless of its type, therefore all extensions policy domains
  // are marked as ready here.
  schema_registry_->SetExtensionsDomainsReady();
}

////////////////////////////////////////////////////////////////////////////////
/// ManagedValueStoreCache
////////////////////////////////////////////////////////////////////////////////

ManagedValueStoreCache::ManagedValueStoreCache(
    Profile& profile,
    scoped_refptr<value_store::ValueStoreFactory> factory,
    SettingsChangedCallback observer)
    : profile_(profile),
      policy_domain_(GetPolicyDomain(profile)),
      policy_service_(*profile.GetProfilePolicyConnector()->policy_service()),
      storage_factory_(std::move(factory)),
      observer_(GetSequenceBoundSettingsChangedCallback(
          base::SequencedTaskRunner::GetCurrentDefault(),
          std::move(observer))) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DETACH_FROM_SEQUENCE(backend_sequence_checker_);

  policy_service_->AddObserver(policy_domain_, this);

  extension_tracker_ =
      std::make_unique<ExtensionTracker>(&profile, policy_domain_);

  if (policy_service_->IsInitializationComplete(policy_domain_))
    OnPolicyServiceInitialized(policy_domain_);
}

ManagedValueStoreCache::~ManagedValueStoreCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);
  // Delete the PolicyValueStores on FILE.
  store_map_.clear();
}

policy::PolicyDomain ManagedValueStoreCache::policy_domain() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  return policy_domain_;
}

void ManagedValueStoreCache::ShutdownOnUI() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  policy_service_->RemoveObserver(policy_domain_, this);
  extension_tracker_.reset();
}

void ManagedValueStoreCache::RunWithValueStoreForExtension(
    StorageCallback callback,
    scoped_refptr<const Extension> extension) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);

  if (is_policy_service_initialized_) {
    std::move(callback).Run(&GetOrCreateStore(extension->id()));
  } else {
    // Delay invoking the callback if the store has not been initialized yet.
    // The store will be initialized as soon as the policy service is
    // initialized, and returning the store beforehand leads to race conditions
    // where the extension can try to fetch a policy value before the store is
    // populated, which results in an empty policy value being returned.
    pending_storage_callbacks_.emplace_back(extension->id(),
                                            std::move(callback));
  }
}

void ManagedValueStoreCache::DeleteStorageSoon(
    const ExtensionId& extension_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);
  // It's possible that the store exists, but hasn't been loaded yet
  // (because the extension is unloaded, for example). Open the database to
  // clear it if it exists.
  if (!HasStore(extension_id))
    return;
  GetOrCreateStore(extension_id).DeleteStorage();
  store_map_.erase(extension_id);
}

void ManagedValueStoreCache::OnPolicyServiceInitialized(
    policy::PolicyDomain domain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  if (domain != policy_domain_)
    return;

  // The PolicyService now has all the initial policies ready. Send policy
  // for all the managed extensions to their backing stores now.
  policy::SchemaRegistry* registry =
      profile_->GetPolicySchemaRegistryService()->registry();
  const policy::ComponentMap* map =
      registry->schema_map()->GetComponents(policy_domain_);

  if (map) {
    const policy::PolicyMap empty_map;
    for (const auto& [extension_id, _] : *map) {
      const policy::PolicyNamespace ns(policy_domain_, extension_id);
      // If there is no policy for |ns| then this will clear the previous
      // store, if there is one.
      OnPolicyUpdated(ns, empty_map, policy_service_->GetPolicies(ns));
    }
  }

  GetBackendTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ManagedValueStoreCache::InitializeOnBackend,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ManagedValueStoreCache::InitializeOnBackend() {
  is_policy_service_initialized_ = true;

  auto pending_callbacks = std::move(pending_storage_callbacks_);
  for (auto& [extension_id, callback] : pending_callbacks) {
    std::move(callback).Run(&GetOrCreateStore(extension_id));
  }
}

void ManagedValueStoreCache::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                             const policy::PolicyMap& previous,
                                             const policy::PolicyMap& current) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  // This WeakPtr usage *should* be safe. Even though we are "vending" WeakPtrs
  // from the UI thread, they are only ever dereferenced or invalidated from
  // the background sequence, since this object is destroyed on the
  // background sequence.
  GetBackendTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ManagedValueStoreCache::UpdatePolicyOnBackend,
                                weak_ptr_factory_.GetWeakPtr(), ns.component_id,
                                current.Clone()));
}

// static
policy::PolicyDomain ManagedValueStoreCache::GetPolicyDomain(
    const Profile& profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ash::ProfileHelper::IsSigninProfile(&profile)
             ? policy::POLICY_DOMAIN_SIGNIN_EXTENSIONS
             : policy::POLICY_DOMAIN_EXTENSIONS;
#else
  return policy::POLICY_DOMAIN_EXTENSIONS;
#endif
}

void ManagedValueStoreCache::UpdatePolicyOnBackend(
    const ExtensionId& extension_id,
    const policy::PolicyMap& new_policy) {
  if (!HasStore(extension_id) && new_policy.empty()) {
    // Don't create the store now if there are no policies configured for this
    // extension. If the extension uses the storage.managed API then the store
    // will be created at RunWithValueStoreForExtension().
    return;
  }

  GetOrCreateStore(extension_id).SetCurrentPolicy(new_policy);
}

PolicyValueStore& ManagedValueStoreCache::GetOrCreateStore(
    const ExtensionId& extension_id) {
  const auto& it = store_map_.find(extension_id);
  if (it != store_map_.end())
    return *it->second;

  // Create the store now, and serve the cached policy until the PolicyService
  // sends updated values.
  auto store = std::make_unique<PolicyValueStore>(
      extension_id, observer_,
      value_store_util::CreateSettingsStore(settings_namespace::MANAGED,
                                            kManagedModelType, extension_id,
                                            storage_factory_));
  PolicyValueStore* raw_store = store.get();
  store_map_[extension_id] = std::move(store);

  return *raw_store;
}

bool ManagedValueStoreCache::HasStore(const ExtensionId& extension_id) const {
  // Note: Currently only manage extensions (not apps).
  return value_store_util::HasValueStore(settings_namespace::MANAGED,
                                         kManagedModelType, extension_id,
                                         storage_factory_);
}

}  // namespace extensions
