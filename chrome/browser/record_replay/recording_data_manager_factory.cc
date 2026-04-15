// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/recording_data_manager_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "components/record_replay/core/browser/recording_data_manager_impl.h"
#include "components/record_replay/core/common/record_replay_features.h"

namespace record_replay {

// static
RecordingDataManager* RecordingDataManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<RecordingDataManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
RecordingDataManagerFactory* RecordingDataManagerFactory::GetInstance() {
  static base::NoDestructor<RecordingDataManagerFactory> instance;
  return instance.get();
}

RecordingDataManagerFactory::RecordingDataManagerFactory()
    : ProfileKeyedServiceFactory(
          "RecordingDataManager",
          ProfileSelections::BuildRedirectedInIncognito()) {}

RecordingDataManagerFactory::~RecordingDataManagerFactory() = default;

bool RecordingDataManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return base::FeatureList::IsEnabled(features::kRecordReplayBase);
}

std::unique_ptr<KeyedService>
RecordingDataManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kRecordReplayBase)) {
    return nullptr;
  }
  return std::make_unique<RecordingDataManagerImpl>(
      Profile::FromBrowserContext(context)->GetPath());
}

}  // namespace record_replay
