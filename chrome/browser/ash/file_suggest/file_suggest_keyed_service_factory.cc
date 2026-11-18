// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"

#include "base/check.h"
#include "chrome/browser/ash/app_list/search/ranking/util.h"
#include "chrome/browser/ash/drive/drive_integration_service_factory.h"
#include "chrome/browser/ash/file_manager/file_tasks_notifier_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"

namespace ash {

// static
FileSuggestKeyedServiceFactory* FileSuggestKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<FileSuggestKeyedServiceFactory> factory;
  return factory.get();
}

FileSuggestKeyedServiceFactory::FileSuggestKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "FileSuggestKeyedService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // Ash Internals profiles (sign-in/lock-screen) are never
              // associated with a logged-in user, so there's no real file
              // suggestion data to serve for them.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(drive::DriveIntegrationServiceFactory::GetInstance());
  DependsOn(file_manager::file_tasks::FileTasksNotifierFactory::GetInstance());
}

FileSuggestKeyedServiceFactory::~FileSuggestKeyedServiceFactory() = default;

FileSuggestKeyedService* FileSuggestKeyedServiceFactory::GetService(
    content::BrowserContext* context) {
  return static_cast<FileSuggestKeyedService*>(
      GetServiceForBrowserContext(context, /*create=*/true));
}

std::unique_ptr<KeyedService>
FileSuggestKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // Regular and Guest profiles -- the only ones this factory is ever built
  // for, per the ctor's ProfileSelections -- always have an associated
  // logged-in user.
  user_manager::User* user =
      BrowserContextHelper::Get()->GetUserByBrowserContext(context);
  CHECK(user);

  Profile* profile = Profile::FromBrowserContext(context);

  PersistentProto<app_list::RemovedResultsProto> proto(
      app_list::RankerStateDirectory(*user).AppendASCII("removed_results.pb"),
      /*write_delay=*/base::TimeDelta());

  return std::make_unique<FileSuggestKeyedService>(
      g_browser_process->local_state(), profile, std::move(proto));
}

}  // namespace ash
