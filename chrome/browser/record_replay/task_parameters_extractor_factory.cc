// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/task_parameters_extractor_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "components/record_replay/core/browser/task_parameters_extractor.h"
#include "components/record_replay/core/common/record_replay_features.h"

namespace record_replay {

// static
TaskParametersExtractor* TaskParametersExtractorFactory::GetForProfile(
    Profile* profile) {
  return static_cast<TaskParametersExtractor*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
TaskParametersExtractorFactory* TaskParametersExtractorFactory::GetInstance() {
  static base::NoDestructor<TaskParametersExtractorFactory> instance;
  return instance.get();
}

TaskParametersExtractorFactory::TaskParametersExtractorFactory()
    : ProfileKeyedServiceFactory("TaskParametersExtractor",
                                 ProfileSelections::BuildForRegularProfile()) {}

TaskParametersExtractorFactory::~TaskParametersExtractorFactory() = default;

std::unique_ptr<KeyedService>
TaskParametersExtractorFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kRecordReplayBase)) {
    return nullptr;
  }
  return std::make_unique<TaskParametersExtractor>();
}

}  // namespace record_replay
