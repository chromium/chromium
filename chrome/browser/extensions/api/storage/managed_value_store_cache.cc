// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/managed_value_store_cache.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "base/scoped_observer.h"
#include "base/task/post_task.h"
#include "chrome/browser/extensions/api/storage/policy_value_store.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/storage/storage_schema_manifest_handler.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/core/common/schema_registry.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/value_store/value_store_change.h"
#include "extensions/browser/value_store/value_store_factory.h"
#include "extensions/common/api/storage.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif

using content::BrowserContext;
using content::BrowserThread;

namespace extensions {
class ExtensionRegistry;


namespace {

// Only extension settings are stored in the managed namespace - not apps.
const ValueStoreFactory::ModelType kManagedModelType =
    ValueStoreFactory::ModelType::EXTENSION;

}  // namespace

// This helper observes initialization of all the installed extensions and
// subsequent loads and unloads, and keeps the SchemaRegistry of the Profile
// in sync with the current list of extensions. This allows the PolicyService
// to fetch cloud policy for those extensions, and allows its providers to
// selectively load only extension policy that has users.
class ManagedValueStoreCache::ExtensionTracker
    : public ExtensionRegistryObserver {
 public:
  ExtensionTracker(Profile* profile, policy::PolicyDomain policy_domain);
  ~ExtensionTracker() override {}

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
  void LoadSchemas(std::unique_ptr<ExtensionSet> added);

  bool UsesManagedStorage(const Extension* extension) const;

  // Loads the schemas of the |extensions| and passes a ComponentMap to
  // Register().
  static void LoadSchemasOnFileTaskRunner(
      std::unique_ptr<ExtensionSet> extensions,
      base::WeakPtr<ExtensionTracker> self);
  void Register(const policy::ComponentMap* components);

  Profile* profile_;
  policy::PolicyDomain policy_domain_;
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};
  policy::SchemaRegistry* schema_registry_;
  base::WeakPtrFactory<ExtensionTracker> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionTracker);
};

ManagedValueStoreCache::ExtensionTracker::ExtensionTracker(
    Profile* profile,
    policy::PolicyDomain policy_domain)
    : profile_(profile),
      policy_domain_(policy_domain),
      schema_registry_(profile->GetPolicySchemaRegistryService()->registry()) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
  // Load schemas when the extension system is ready. It might be ready now.
  ExtensionSystem::Get(profile_)->ready().Post(
      FROM_HERE,
      base::Bind(&ExtensionTracker::OnExtensionsReady,
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
  std::unique_ptr<ExtensionSet> added(new ExtensionSet);
  added->Insert(extension);
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

void ManagedValueStoreCache::ExtensionTracker::LoadSchemas(
    std::unique_ptr<ExtensionSet> added) {
  // Filter out extensions that don't use managed storage.
  ExtensionSet::const_iterator it = added->begin();
  while (it != added->end()) {
    std::string to_remove;
    if (!UsesManagedStorage(it->get()))
      to_remove = (*it)->id();
    ++it;
    if (!to_remove.empty())
      added->Remove(to_remove);
  }

  GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ExtensionTracker::LoadSchemasOnFileTaskRunner,
                                std::move(added), weak_factory_.GetWeakPtr()));
}

bool ManagedValueStoreCache::ExtensionTracker::UsesManagedStorage(
    const Extension* extension) const {
  return extension->manifest()->HasPath(manifest_keys::kStorageManagedSchema);
}

// static
void ManagedValueStoreCache::ExtensionTracker::LoadSchemasOnFileTaskRunner(
    std::unique_ptr<ExtensionSet> extensions,
    base::WeakPtr<ExtensionTracker> self) {
  std::unique_ptr<policy::ComponentMap> components(new policy::ComponentMap);

  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    std::string schema_file;
    if (!(*it)->manifest()->GetString(
            manifest_keys::kStorageManagedSchema, &schema_file)) {
      // TODO(joaodasilva): Remove this. http://crbug.com/325349
      (*components)[(*it)->id()] = policy::Schema();
      continue;
    }
    // The extension should have been validated, so assume the schema exists
    // and is valid.
    std::string error;
    policy::Schema schema =
        StorageSchemaManifestHandler::GetSchema(it->get(), &error);
    // If the schema is invalid then proceed with an empty schema. The extension
    // will be listed in chrome://policy but won't be able to load any policies.
    if (!schema.valid())
      schema = policy::Schema();
    (*components)[(*it)->id()] = schema;
  }

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&ExtensionTracker::Register, self,
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

ManagedValueStoreCache::ManagedValueStoreCache(
    BrowserContext* context,
    scoped_refptr<ValueStoreFactory> factory,
    scoped_refptr<SettingsObserverList> observers)
    : profile_(Profile::FromBrowserContext(context)),
      policy_domain_(GetPolicyDomain(profile_)),
      policy_service_(profile_->GetProfilePolicyConnector()->policy_service()),
      storage_factory_(std::move(factory)),
      observers_(std::move(observers)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  policy_service_->AddObserver(policy_domain_, this);

  extension_tracker_.reset(new ExtensionTracker(profile_, policy_domain_));

  if (policy_service_->IsInitializationComplete(policy_domain_))
    OnPolicyServiceInitialized(policy_domain_);
}

ManagedValueStoreCache::~ManagedValueStoreCache() {
  DCHECK(IsOnBackendSequence());
  // Delete the PolicyValueStores on FILE.
  store_map_.clear();
}

void ManagedValueStoreCache::ShutdownOnUI() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  policy_service_->RemoveObserver(policy_domain_, this);
  extension_tracker_.reset();
}

void ManagedValueStoreCache::RunWithValueStoreForExtension(
    const StorageCallback& callback,
    scoped_refptr<const Extension> extension) {
  DCHECK(IsOnBackendSequence());
  callback.Run(GetStoreFor(extension->id()));
}

void ManagedValueStoreCache::DeleteStorageSoon(
    const std::string& extension_id) {
  DCHECK(IsOnBackendSequence());
  // It's possible that the store exists, but hasn't been loaded yet
  // (because the extension is unloaded, for example). Open the database to
  // clear it if it exists.
  if (!HasStore(extension_id))
    return;
  GetStoreFor(extension_id)->DeleteStorage();
  store_map_.erase(extension_id);
}

void ManagedValueStoreCache::OnPolicyServiceInitialized(
    policy::PolicyDomain domain) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (domain != policy_domain_)
    return;

  // The PolicyService now has all the initial policies ready. Send policy
  // for all the managed extensions to their backing stores now.
  policy::SchemaRegistry* registry =
      profile_->GetPolicySchemaRegistryService()->registry();
  const policy::ComponentMap* map =
      registry->schema_map()->GetComponents(policy_domain_);
  if (!map)
    return;

  const policy::PolicyMap empty_map;
  for (auto it = map->cbegin(); it != map->cend(); ++it) {
    const policy::PolicyNamespace ns(policy_domain_, it->first);
    // If there is no policy for |ns| then this will clear the previous store,
    // if there is one.
    OnPolicyUpdated(ns, empty_map, policy_service_->GetPolicies(ns));
  }
}

void ManagedValueStoreCache::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                             const policy::PolicyMap& previous,
                                             const policy::PolicyMap& current) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!policy_service_->IsInitializationComplete(policy_domain_)) {
    // OnPolicyUpdated is called whenever a policy changes, but it doesn't
    // mean that all the policy providers are ready; wait until we get the
    // final policy values before passing them to the store.
    return;
  }

  GetBackendTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ManagedValueStoreCache::UpdatePolicyOnBackend,
                                base::Unretained(this), ns.component_id,
                                current.DeepCopy()));
}

// static
policy::PolicyDomain ManagedValueStoreCache::GetPolicyDomain(Profile* profile) {
#if defined(OS_CHROMEOS)
  return chromeos::ProfileHelper::IsSigninProfile(profile)
             ? policy::POLICY_DOMAIN_SIGNIN_EXTENSIONS
             : policy::POLICY_DOMAIN_EXTENSIONS;
#else
  return policy::POLICY_DOMAIN_EXTENSIONS;
#endif
}

void ManagedValueStoreCache::UpdatePolicyOnBackend(
    const std::string& extension_id,
    std::unique_ptr<policy::PolicyMap> current_policy) {
  DCHECK(IsOnBackendSequence());

  if (!HasStore(extension_id) && current_policy->empty()) {
    // Don't create the store now if there are no policies configured for this
    // extension. If the extension uses the storage.managed API then the store
    // will be created at RunWithValueStoreForExtension().
    return;
  }

  GetStoreFor(extension_id)->SetCurrentPolicy(*current_policy);
}

PolicyValueStore* ManagedValueStoreCache::GetStoreFor(
    const std::string& extension_id) {
  DCHECK(IsOnBackendSequence());

  auto it = store_map_.find(extension_id);
  if (it != store_map_.end())
    return it->second.get();

  // Create the store now, and serve the cached policy until the PolicyService
  // sends updated values.
  std::unique_ptr<PolicyValueStore> store(new PolicyValueStore(
      extension_id, observers_,
      storage_factory_->CreateSettingsStore(settings_namespace::MANAGED,
                                            kManagedModelType, extension_id)));
  PolicyValueStore* raw_store = store.get();
  store_map_[extension_id] = std::move(store);

  return raw_store;
}

bool ManagedValueStoreCache::HasStore(const std::string& extension_id) const {
  // Note: Currently only manage extensions (not apps).
  return storage_factory_->HasSettings(settings_namespace::MANAGED,
                                       kManagedModelType, extension_id);
}

}  // namespace extensions
