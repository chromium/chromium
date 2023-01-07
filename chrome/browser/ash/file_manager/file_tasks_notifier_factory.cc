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
    : ProfileKeyedServiceFactory("FileTasksNotifier") {}

FileTasksNotifierFactory* FileTasksNotifierFactory::GetInstance() {
  static base::NoDestructor<FileTasksNotifierFactory> instance;
  return instance.get();
}

FileTasksNotifier* FileTasksNotifierFactory::GetForProfile(Profile* profile) {
  return static_cast<FileTasksNotifier*>(
      GetServiceForBrowserContext(profile, true));
}

KeyedService* FileTasksNotifierFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new FileTasksNotifier(Profile::FromBrowserContext(context));
}

}  // namespace file_tasks
}  // namespace file_manager
