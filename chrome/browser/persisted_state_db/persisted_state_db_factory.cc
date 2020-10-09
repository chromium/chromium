// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/persisted_state_db/persisted_state_db_factory.h"

#include "chrome/browser/persisted_state_db/persisted_state_db.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace {
const char kPersistedStateDBFolder[] = "persisted_state_db";
}  // namespace

// static
PersistedStateDBFactory* PersistedStateDBFactory::GetInstance() {
  return base::Singleton<PersistedStateDBFactory>::get();
}

// static
PersistedStateDB* PersistedStateDBFactory::GetForProfile(
    content::BrowserContext* context) {
  // Incognito is currently not supported
  if (context->IsOffTheRecord())
    return nullptr;

  return static_cast<PersistedStateDB*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

PersistedStateDBFactory::PersistedStateDBFactory()
    : BrowserContextKeyedServiceFactory(
          "PersistedStateDBKeyedService",
          BrowserContextDependencyManager::GetInstance()) {}

PersistedStateDBFactory::~PersistedStateDBFactory() = default;

KeyedService* PersistedStateDBFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(!context->IsOffTheRecord());

  leveldb_proto::ProtoDatabaseProvider* proto_database_provider =
      content::BrowserContext::GetDefaultStoragePartition(context)
          ->GetProtoDatabaseProvider();
  base::FilePath tab_state_db_dir(
      context->GetPath().AppendASCII(kPersistedStateDBFolder));
  return new PersistedStateDB(proto_database_provider, tab_state_db_dir);
}
