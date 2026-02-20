// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_metrics_provider.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "components/skills/public/skills_metrics.h"
#include "components/skills/public/skills_service.h"

namespace skills {

SkillsMetricsProvider::SkillsMetricsProvider(
    ProfileListCallback profile_list_callback)
    : profile_list_callback_(std::move(profile_list_callback)) {}

SkillsMetricsProvider::~SkillsMetricsProvider() = default;

void SkillsMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  std::vector<Profile*> profiles = profile_list_callback_.Run();
  // Iterate over all loaded profiles to capture the user's state across
  // different contexts.
  for (Profile* profile : profiles) {
    // Explicitly skip non regular profiles.
    if (!profile->IsRegularProfile()) {
      continue;
    }
    if (auto* service = SkillsServiceFactory::GetForProfile(profile)) {
      RecordUserSkillCount(service->GetSkills().size());
    }
  }
}

}  // namespace skills
