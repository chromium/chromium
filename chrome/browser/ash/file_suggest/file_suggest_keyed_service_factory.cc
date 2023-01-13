// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"

#include "chrome/browser/ash/app_list/search/ranking/util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/file_tasks_notifier_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {

// static
FileSuggestKeyedServiceFactory* FileSuggestKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<FileSuggestKeyedServiceFactory> factory;
  return factory.get();
}

FileSuggestKeyedServiceFactory::FileSuggestKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "FileSuggestKeyedService",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(drive::DriveIntegrationServiceFactory::GetInstance());
  DependsOn(file_manager::file_tasks::FileTasksNotifierFactory::GetInstance());
}

FileSuggestKeyedServiceFactory::~FileSuggestKeyedServiceFactory() = default;

FileSuggestKeyedService* FileSuggestKeyedServiceFactory::GetService(
    content::BrowserContext* context) {
  return static_cast<FileSuggestKeyedService*>(
      GetServiceForBrowserContext(context, /*create=*/true));
}

KeyedService* FileSuggestKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  // TODO(https://crbug.com/1368833): Right now, the service reuses the proto
  // originally for app list. The service should have its own proto that
  // contains file ids only.
  app_list::PersistentProto<app_list::RemovedResultsProto> proto(
      app_list::RankerStateDirectory(profile).AppendASCII("removed_results.pb"),
      /*write_delay=*/base::TimeDelta());

  return new FileSuggestKeyedService(profile, std::move(proto));
}

}  // namespace ash
