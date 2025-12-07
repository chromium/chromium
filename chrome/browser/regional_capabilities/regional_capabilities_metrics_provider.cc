// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_metrics_provider.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_factory.h"
#include "components/regional_capabilities/program_settings.h"
#include "components/regional_capabilities/regional_capabilities_metrics.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace regional_capabilities {

void RegionalCapabilitiesMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  absl::flat_hash_set<ActiveRegionalProgram> programs;
  for (Profile* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    RegionalCapabilitiesService* regional_capabilities =
        RegionalCapabilitiesServiceFactory::GetForProfile(profile);
    if (!regional_capabilities) {
      // Ignore profiles such as the system profile that don't have a
      // RegionalCapabilitiesService.
      continue;
    }

    programs.insert(regional_capabilities->GetActiveProgramForMetrics());
  }

  RecordActiveRegionalProgram(programs);
}

}  // namespace regional_capabilities
