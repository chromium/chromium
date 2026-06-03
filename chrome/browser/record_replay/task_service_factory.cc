// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/task_service_factory.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/record_replay/task_executor.h"
#include "chrome/browser/record_replay/task_parameters_extractor_factory.h"
#include "chrome/browser/record_replay/task_store_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "components/record_replay/core/browser/task_service.h"
#include "components/record_replay/core/common/record_replay_features.h"

namespace record_replay {

// static
TaskService* TaskServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<TaskService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
TaskServiceFactory* TaskServiceFactory::GetInstance() {
  static base::NoDestructor<TaskServiceFactory> instance;
  return instance.get();
}

TaskServiceFactory::TaskServiceFactory()
    : ProfileKeyedServiceFactory("TaskService") {
  // The TaskService depends on the TaskStore to be able to use the
  // TaskDatabase.
  DependsOn(TaskStoreFactory::GetInstance());
  DependsOn(TaskParametersExtractorFactory::GetInstance());
}

TaskServiceFactory::~TaskServiceFactory() = default;

std::unique_ptr<KeyedService>
TaskServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kRecordReplayBase)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);

  auto execution_callback = base::BindRepeating(
      [](Profile* profile, const TaskDefinition& definition,
         const std::vector<TaskParameter>& parameter_values) {
        BrowserWindowInterface* browser =
            ProfileBrowserCollection::GetForProfile(profile)
                ->GetLastActiveBrowser();
        if (browser) {
          TaskExecutor::ExecuteTask(profile, browser, definition,
                                    parameter_values);
        }
      },
      profile);

  return std::make_unique<TaskService>(
      TaskStoreFactory::GetForProfile(profile),
      TaskParametersExtractorFactory::GetForProfile(profile),
      std::move(execution_callback));
}

}  // namespace record_replay
