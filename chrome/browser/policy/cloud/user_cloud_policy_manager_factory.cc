// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"

namespace policy {

namespace {

// Directory inside the profile directory where policy-related resources are
// stored.
const base::FilePath::CharType kPolicy[] = FILE_PATH_LITERAL("Policy");

// Directory under kPolicy, in the user's profile dir, where policy for
// components is cached.
const base::FilePath::CharType kComponentsDir[] =
    FILE_PATH_LITERAL("Components");

}  // namespace

// A KeyedService that wraps a UserCloudPolicyManager.
class UserCloudPolicyManagerFactory::ManagerWrapper : public KeyedService {
 public:
  explicit ManagerWrapper(UserCloudPolicyManager* manager)
      : manager_(manager) {
    DCHECK(manager);
  }
  ~ManagerWrapper() override {}

  void Shutdown() override { manager_->Shutdown(); }

  UserCloudPolicyManager* manager() { return manager_; }

 private:
  UserCloudPolicyManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(ManagerWrapper);
};

// static
UserCloudPolicyManagerFactory* UserCloudPolicyManagerFactory::GetInstance() {
  return base::Singleton<UserCloudPolicyManagerFactory>::get();
}

// static
UserCloudPolicyManager* UserCloudPolicyManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return GetInstance()->GetManagerForBrowserContext(context);
}

// static
std::unique_ptr<UserCloudPolicyManager>
UserCloudPolicyManagerFactory::CreateForOriginalBrowserContext(
    content::BrowserContext* context,
    bool force_immediate_load,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner) {
  UserCloudPolicyManagerFactory* factory = GetInstance();
  // If there's a testing factory set, don't bother creating a new one.
  if (factory->testing_factory_)
    return std::unique_ptr<UserCloudPolicyManager>();
  return factory->CreateManagerForOriginalBrowserContext(
      context, force_immediate_load, background_task_runner);
}

// static
UserCloudPolicyManager*
UserCloudPolicyManagerFactory::RegisterForOffTheRecordBrowserContext(
    content::BrowserContext* original_context,
    content::BrowserContext* off_the_record_context) {
  return GetInstance()->RegisterManagerForOffTheRecordBrowserContext(
      original_context, off_the_record_context);
}

void UserCloudPolicyManagerFactory::RegisterTestingFactory(
    TestingFactory factory) {
  // Can't set a testing factory when a testing factory has already been
  // created, or after UCPMs have already been built.
  DCHECK(!testing_factory_);
  DCHECK(factory);
  DCHECK(manager_wrappers_.empty());
  testing_factory_ = std::move(factory);
}

void UserCloudPolicyManagerFactory::ClearTestingFactory() {
  testing_factory_ = TestingFactory();
}

UserCloudPolicyManagerFactory::UserCloudPolicyManagerFactory()
    : BrowserContextKeyedBaseFactory(
          "UserCloudPolicyManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SchemaRegistryServiceFactory::GetInstance());
}

UserCloudPolicyManagerFactory::~UserCloudPolicyManagerFactory() {
  DCHECK(manager_wrappers_.empty());
}

UserCloudPolicyManager*
UserCloudPolicyManagerFactory::GetManagerForBrowserContext(
    content::BrowserContext* context) {
  // In case |context| is an incognito Profile/Context, |manager_wrappers_|
  // will have a matching entry pointing to the manager of the original context.
  ManagerWrapperMap::const_iterator it = manager_wrappers_.find(context);
  return it != manager_wrappers_.end() ? it->second->manager() : NULL;
}

std::unique_ptr<UserCloudPolicyManager>
UserCloudPolicyManagerFactory::CreateManagerForOriginalBrowserContext(
    content::BrowserContext* context,
    bool force_immediate_load,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner) {
  DCHECK(!context->IsOffTheRecord());

  // This should never be called if we're using a testing factory.
  // Instead, instances are instantiated via CreateServiceNow().
  DCHECK(!testing_factory_);

  std::unique_ptr<UserCloudPolicyStore> store(
      UserCloudPolicyStore::Create(context->GetPath(), background_task_runner));
  if (force_immediate_load)
    store->LoadImmediately();

  const base::FilePath component_policy_cache_dir =
      context->GetPath().Append(kPolicy).Append(kComponentsDir);

  std::unique_ptr<UserCloudPolicyManager> manager;
  manager.reset(new UserCloudPolicyManager(
      std::move(store), component_policy_cache_dir,
      std::unique_ptr<CloudExternalDataManager>(),
      base::ThreadTaskRunnerHandle::Get(),
      base::BindRepeating(&content::GetNetworkConnectionTracker)));
  manager->Init(
      SchemaRegistryServiceFactory::GetForContext(context)->registry());
  manager_wrappers_[context] = new ManagerWrapper(manager.get());
  return manager;
}

UserCloudPolicyManager*
UserCloudPolicyManagerFactory::RegisterManagerForOffTheRecordBrowserContext(
    content::BrowserContext* original_context,
    content::BrowserContext* off_the_record_context) {
  // Register the UserCloudPolicyManager of the original context for the
  // respective incognito context. See also GetManagerForBrowserContext.
  UserCloudPolicyManager* manager =
      GetManagerForBrowserContext(original_context);
  manager_wrappers_[off_the_record_context] = new ManagerWrapper(manager);
  return manager;
}

void UserCloudPolicyManagerFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  if (context->IsOffTheRecord())
    return;
  auto it = manager_wrappers_.find(context);
  // E.g. for a TestingProfile there might not be a manager created.
  if (it != manager_wrappers_.end())
    it->second->Shutdown();
}

void UserCloudPolicyManagerFactory::BrowserContextDestroyed(
    content::BrowserContext* context) {
  auto it = manager_wrappers_.find(context);
  if (it != manager_wrappers_.end()) {
    // The manager is not owned by the factory, so it's not deleted here.
    delete it->second;
    manager_wrappers_.erase(it);
  }
}

void UserCloudPolicyManagerFactory::SetEmptyTestingFactory(
    content::BrowserContext* context) {}

bool UserCloudPolicyManagerFactory::HasTestingFactory(
    content::BrowserContext* context) {
  return !testing_factory_.is_null();
}

// If there's a TestingFactory set, then create a service during BrowserContext
// initialization.
bool UserCloudPolicyManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return !testing_factory_.is_null();
}

void UserCloudPolicyManagerFactory::CreateServiceNow(
    content::BrowserContext* context) {
  DCHECK(testing_factory_);
  manager_wrappers_[context] =
      new ManagerWrapper(testing_factory_.Run(context));
}

}  // namespace policy
