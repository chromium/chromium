// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/strike_database_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/payments/strike_database.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/storage_partition.h"

namespace autofill {

// static
StrikeDatabase* StrikeDatabaseFactory::GetForProfile(Profile* profile) {
  return static_cast<StrikeDatabase*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
StrikeDatabaseFactory* StrikeDatabaseFactory::GetInstance() {
  return base::Singleton<StrikeDatabaseFactory>::get();
}

StrikeDatabaseFactory::StrikeDatabaseFactory()
    : BrowserContextKeyedServiceFactory(
          "AutofillStrikeDatabase",
          BrowserContextDependencyManager::GetInstance()) {
}

StrikeDatabaseFactory::~StrikeDatabaseFactory() {}

KeyedService* StrikeDatabaseFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  leveldb_proto::ProtoDatabaseProvider* db_provider =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetProtoDatabaseProvider();

  // Note: This instance becomes owned by an object that never gets destroyed,
  // effectively leaking it until browser close. Only one is created per
  // profile, and closing-then-opening a profile returns the same instance.
  return new StrikeDatabase(db_provider, profile->GetPath());
}

}  // namespace autofill
