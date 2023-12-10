// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reading_list/reading_list_model_factory.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/reading_list/core/dual_reading_list_model.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/reading_list/core/reading_list_model_storage_impl.h"
#include "components/reading_list/core/reading_list_pref_names.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/sync/base/features.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

std::unique_ptr<KeyedService> BuildReadingListModel(
    content::BrowserContext* context) {
  Profile* const profile = Profile::FromBrowserContext(context);
  syncer::OnceModelTypeStoreFactory store_factory =
      ModelTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();
  auto local_storage =
      std::make_unique<ReadingListModelStorageImpl>(std::move(store_factory));
  auto reading_list_model_for_local_storage =
      std::make_unique<ReadingListModelImpl>(
          std::move(local_storage), syncer::StorageType::kUnspecified,
          syncer::WipeModelUponSyncDisabledBehavior::kNever,
          base::DefaultClock::GetInstance());

  syncer::OnceModelTypeStoreFactory store_factory_for_account_storage =
      ModelTypeStoreServiceFactory::GetForProfile(profile)
          ->GetStoreFactoryForAccountStorage();
  auto account_storage = std::make_unique<ReadingListModelStorageImpl>(
      std::move(store_factory_for_account_storage));
  auto reading_list_model_for_account_storage =
      std::make_unique<ReadingListModelImpl>(
          std::move(account_storage), syncer::StorageType::kAccount,
          syncer::WipeModelUponSyncDisabledBehavior::kAlways,
          base::DefaultClock::GetInstance());
  return std::make_unique<reading_list::DualReadingListModel>(
      /*local_or_syncable_model=*/std::move(
          reading_list_model_for_local_storage),
      /*account_model=*/std::move(reading_list_model_for_account_storage));
}

}  // namespace

// static
ReadingListModel* ReadingListModelFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ReadingListModel*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ReadingListModelFactory* ReadingListModelFactory::GetInstance() {
  static base::NoDestructor<ReadingListModelFactory> instance;
  return instance.get();
}

// static
BrowserContextKeyedServiceFactory::TestingFactory
ReadingListModelFactory::GetDefaultFactoryForTesting() {
  return base::BindRepeating(&BuildReadingListModel);
}

ReadingListModelFactory::ReadingListModelFactory()
    : ProfileKeyedServiceFactory(
          "ReadingListModel",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
}

ReadingListModelFactory::~ReadingListModelFactory() = default;

std::unique_ptr<KeyedService>
ReadingListModelFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildReadingListModel(context);
}

void ReadingListModelFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(
      reading_list::prefs::kReadingListDesktopFirstUseExperienceShown, false,
      PrefRegistry::NO_REGISTRATION_FLAGS);
#endif  // !BUILDFLAG(IS_ANDROID)
}

bool ReadingListModelFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
