// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/task_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/record_replay/recording_data_manager_factory.h"
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
  // The TaskService depends on the RecordingDataManager to be able to use the
  // TaskDatabase.
  DependsOn(RecordingDataManagerFactory::GetInstance());
}

TaskServiceFactory::~TaskServiceFactory() = default;

std::unique_ptr<KeyedService>
TaskServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kRecordReplayBase)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<TaskService>(
      RecordingDataManagerFactory::GetForProfile(profile));
}

}  // namespace record_replay
