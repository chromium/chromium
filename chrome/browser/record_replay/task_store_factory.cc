// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/task_store_factory.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "components/record_replay/core/browser/task_store_impl.h"
#include "components/record_replay/core/common/record_replay_features.h"
#include "components/record_replay/core/common/record_replay_switches.h"

namespace record_replay {

// static
TaskStore* TaskStoreFactory::GetForProfile(Profile* profile) {
  return static_cast<TaskStore*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
TaskStoreFactory* TaskStoreFactory::GetInstance() {
  static base::NoDestructor<TaskStoreFactory> instance;
  return instance.get();
}

TaskStoreFactory::TaskStoreFactory()
    : ProfileKeyedServiceFactory("TaskStore",
                                 ProfileSelections::BuildForRegularProfile()) {}

TaskStoreFactory::~TaskStoreFactory() = default;

bool TaskStoreFactory::ServiceIsCreatedWithBrowserContext() const {
  if (!base::FeatureList::IsEnabled(features::kRecordReplayBase)) {
    return false;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool has_seeding_file =
      command_line->HasSwitch(switches::kTaskDefinitionFile);
  bool has_seeding_param =
      !features::kRecordReplayTaskDefinitionSeed.Get().empty();
  bool has_wipe_switch = command_line->HasSwitch(switches::kWipeRecordings);

  return has_seeding_file || has_seeding_param || has_wipe_switch;
}

std::unique_ptr<KeyedService>
TaskStoreFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kRecordReplayBase)) {
    return nullptr;
  }
  return std::make_unique<TaskStoreImpl>(
      Profile::FromBrowserContext(context)->GetPath());
}

}  // namespace record_replay
