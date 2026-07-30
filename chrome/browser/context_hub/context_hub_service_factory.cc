// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/context_hub_service_factory.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/context_hub/auto_todos/in_memory_auto_todos_store.h"
#include "chrome/browser/context_hub/context_hub_service.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/context_hub/memory_bank/database_memory_bank.h"
#include "chrome/browser/context_hub/memory_bank/in_memory_memory_bank.h"
#include "chrome/browser/context_hub/memory_bank/noop_memory_bank.h"
#include "chrome/browser/context_hub/storage/context_hub_backend_impl.h"
#include "chrome/browser/context_hub/tab_group_store/in_memory_tab_group_store.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/personal_context/personal_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-features.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "sql/database.h"

namespace {
constexpr base::FilePath::CharType kContextHubDatabaseFileName[] =
    FILE_PATH_LITERAL("ContextHub.db");
}  // namespace

// static
context_hub::ContextHubService* ContextHubServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<context_hub::ContextHubService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ContextHubServiceFactory* ContextHubServiceFactory::GetInstance() {
  static base::NoDestructor<ContextHubServiceFactory> instance;
  return instance.get();
}

ContextHubServiceFactory::ContextHubServiceFactory()
    : ProfileKeyedServiceFactory(
          "ContextHubService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(PersonalContextServiceFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

ContextHubServiceFactory::~ContextHubServiceFactory() = default;

std::unique_ptr<KeyedService>
ContextHubServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(context_hub::features::kContextHub)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  personal_context::PersonalContextService* personal_context_service =
      PersonalContextServiceFactory::GetForProfile(profile);
  if (!personal_context_service) {
    return nullptr;
  }
  OptimizationGuideKeyedService* optimization_guide_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!optimization_guide_service) {
    return nullptr;
  }

  base::FilePath db_path =
      profile->GetPath().Append(kContextHubDatabaseFileName);

  std::unique_ptr<context_hub::ContextHubBackend> backend;
  std::unique_ptr<context_hub::MemoryBank> memory_bank;
  if (base::FeatureList::IsEnabled(context_hub::features::kMemoryBanks)) {
    if (base::FeatureList::IsEnabled(
            context_hub::features::kContextHubDatabaseStorage)) {
      backend = std::make_unique<context_hub::ContextHubBackendImpl>(db_path);
      memory_bank = std::make_unique<context_hub::DatabaseMemoryBank>(*backend);
    } else {
      memory_bank = std::make_unique<context_hub::InMemoryMemoryBank>();
    }
  } else {
    memory_bank = std::make_unique<context_hub::NoOpMemoryBank>();
  }
  std::unique_ptr<context_hub::TabGroupStore> tab_group_store;
  if (base::FeatureList::IsEnabled(
          browser::context_hub::mojom::kAutoTabGroups)) {
    tab_group_store = std::make_unique<context_hub::InMemoryTabGroupStore>();
  }
  std::unique_ptr<context_hub::AutoTodosStore> auto_todos_store;
  if (base::FeatureList::IsEnabled(browser::context_hub::mojom::kAutoTodos)) {
    auto_todos_store = std::make_unique<context_hub::InMemoryAutoTodosStore>();
  }

  if (!backend) {
    // If database storage is disabled by feature flags, attempt to delete any
    // existing database on disk.
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(
            [](const base::FilePath& path) { sql::Database::Delete(path); },
            db_path),
        base::DoNothing());
  }

  return std::make_unique<context_hub::ContextHubService>(
      personal_context_service, optimization_guide_service,
      std::move(memory_bank), std::move(tab_group_store), std::move(backend),
      std::move(auto_todos_store));
}
