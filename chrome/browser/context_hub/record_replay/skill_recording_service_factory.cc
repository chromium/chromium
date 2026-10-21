// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/skill_recording_service_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/context_hub/record_replay/skill_recording_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-features.h"

// static
context_hub::SkillRecordingService* SkillRecordingServiceFactory::GetForProfile(
    Profile* profile) {
  if (!profile) {
    return nullptr;
  }
  return static_cast<context_hub::SkillRecordingService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
SkillRecordingServiceFactory* SkillRecordingServiceFactory::GetInstance() {
  static base::NoDestructor<SkillRecordingServiceFactory> instance;
  return instance.get();
}

SkillRecordingServiceFactory::SkillRecordingServiceFactory()
    : ProfileKeyedServiceFactory(
          "SkillRecordingService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

SkillRecordingServiceFactory::~SkillRecordingServiceFactory() = default;

std::unique_ptr<KeyedService>
SkillRecordingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* /*context*/) const {
  if (!base::FeatureList::IsEnabled(
          browser::context_hub::mojom::kRecordReplay)) {
    return nullptr;
  }
  return std::make_unique<context_hub::SkillRecordingService>();
}
