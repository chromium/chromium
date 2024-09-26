// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_tasks_notifier_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/file_manager/file_tasks_notifier.h"
#include "chrome/browser/profiles/profile.h"

namespace file_manager {
namespace file_tasks {

FileTasksNotifierFactory ::FileTasksNotifierFactory()
    : ProfileKeyedServiceFactory(
          "FileTasksNotifier",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

// static
FileTasksNotifierFactory* FileTasksNotifierFactory::GetInstance() {
  static base::NoDestructor<FileTasksNotifierFactory> instance;
  return instance.get();
}

// static
FileTasksNotifier* FileTasksNotifierFactory::GetForProfile(Profile* profile) {
  return static_cast<FileTasksNotifier*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

std::unique_ptr<KeyedService>
FileTasksNotifierFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<FileTasksNotifier>(
      Profile::FromBrowserContext(context));
}

}  // namespace file_tasks
}  // namespace file_manager
