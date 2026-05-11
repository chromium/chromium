// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_TASK_PARAMETERS_EXTRACTOR_FACTORY_H_
#define CHROME_BROWSER_RECORD_REPLAY_TASK_PARAMETERS_EXTRACTOR_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace record_replay {

class TaskParametersExtractor;

// Singleton factory to manage `TaskParametersExtractor` instances per
// `Profile`.
class TaskParametersExtractorFactory : public ProfileKeyedServiceFactory {
 public:
  static TaskParametersExtractor* GetForProfile(Profile* profile);
  static TaskParametersExtractorFactory* GetInstance();

  TaskParametersExtractorFactory(const TaskParametersExtractorFactory&) =
      delete;
  TaskParametersExtractorFactory& operator=(
      const TaskParametersExtractorFactory&) = delete;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

 private:
  friend base::NoDestructor<TaskParametersExtractorFactory>;

  TaskParametersExtractorFactory();
  ~TaskParametersExtractorFactory() override;
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_RECORD_REPLAY_TASK_PARAMETERS_EXTRACTOR_FACTORY_H_
