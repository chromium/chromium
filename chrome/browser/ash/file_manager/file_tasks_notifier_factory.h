// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_FILE_TASKS_NOTIFIER_FACTORY_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_FILE_TASKS_NOTIFIER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace file_manager {
namespace file_tasks {

class FileTasksNotifier;

class FileTasksNotifierFactory : public ProfileKeyedServiceFactory {
 public:
  FileTasksNotifierFactory();

  FileTasksNotifierFactory(const FileTasksNotifierFactory&) = delete;
  FileTasksNotifierFactory& operator=(const FileTasksNotifierFactory&) = delete;

  static FileTasksNotifierFactory* GetInstance();

  static FileTasksNotifier* GetForProfile(Profile* profile);

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace file_tasks
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_FILE_TASKS_NOTIFIER_FACTORY_H_
