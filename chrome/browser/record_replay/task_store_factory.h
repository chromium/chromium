// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_TASK_STORE_FACTORY_H_
#define CHROME_BROWSER_RECORD_REPLAY_TASK_STORE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace record_replay {

class TaskStore;

// Manages the creation and retrieval of `TaskStore` (1 per
// `Profile`).
//
// It is a `ProfileKeyedServiceFactory` and exists as a singleton for the
// lifetime of the browser process.
class TaskStoreFactory : public ProfileKeyedServiceFactory {
 public:
  static TaskStore* GetForProfile(Profile* profile);
  static TaskStoreFactory* GetInstance();

  TaskStoreFactory(const TaskStoreFactory&) = delete;
  TaskStoreFactory& operator=(const TaskStoreFactory&) = delete;

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;

 private:
  friend base::NoDestructor<TaskStoreFactory>;

  TaskStoreFactory();
  ~TaskStoreFactory() override;
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_RECORD_REPLAY_TASK_STORE_FACTORY_H_
