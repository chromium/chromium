// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_TASKS_NOTIFIER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_TASKS_NOTIFIER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace file_manager {
namespace file_tasks {

class FileTasksNotifier;

class FileTasksNotifierFactory : public BrowserContextKeyedServiceFactory {
 public:
  FileTasksNotifierFactory();

  static FileTasksNotifierFactory* GetInstance();

  FileTasksNotifier* GetForProfile(Profile* profile);

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileTasksNotifierFactory);
};

}  // namespace file_tasks
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_TASKS_NOTIFIER_FACTORY_H_
