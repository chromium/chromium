// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_tasks_notifier_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace file_manager {
namespace file_tasks {

FileTasksNotifierFactory ::FileTasksNotifierFactory()
    : BrowserContextKeyedServiceFactory(
          "FileTasksNotifier",
          BrowserContextDependencyManager::GetInstance()) {}

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
