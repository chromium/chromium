// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/strike_database_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/strike_databases/strike_database.h"
#include "content/public/browser/storage_partition.h"

namespace autofill {

// static
StrikeDatabase* StrikeDatabaseFactory::GetForProfile(Profile* profile) {
  return static_cast<StrikeDatabase*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
StrikeDatabaseFactory* StrikeDatabaseFactory::GetInstance() {
  static base::NoDestructor<StrikeDatabaseFactory> instance;
  return instance.get();
}

StrikeDatabaseFactory::StrikeDatabaseFactory()
    : ProfileKeyedServiceFactory(
          "AutofillStrikeDatabase",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

StrikeDatabaseFactory::~StrikeDatabaseFactory() = default;

KeyedService* StrikeDatabaseFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  leveldb_proto::ProtoDatabaseProvider* db_provider =
      profile->GetDefaultStoragePartition()->GetProtoDatabaseProvider();

  // Note: This instance becomes owned by an object that never gets destroyed,
  // effectively leaking it until browser close. Only one is created per
  // profile, and closing-then-opening a profile returns the same instance.
  return new StrikeDatabase(db_provider, profile->GetPath());
}

}  // namespace autofill
