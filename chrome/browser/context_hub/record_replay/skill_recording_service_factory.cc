// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/skill_recording_service_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/context_hub/record_replay/skill_recording_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_manager_service_factory.h"
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
              .Build()) {
  DependsOn(BrowserManagerServiceFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

SkillRecordingServiceFactory::~SkillRecordingServiceFactory() = default;

std::unique_ptr<KeyedService>
SkillRecordingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          browser::context_hub::mojom::kRecordReplay)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  OptimizationGuideKeyedService* opt_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  return std::make_unique<context_hub::SkillRecordingService>(profile,
                                                              opt_guide);
}
