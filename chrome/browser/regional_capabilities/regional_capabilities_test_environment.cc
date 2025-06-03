// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_test_environment.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/service/test_variations_service.h"

namespace regional_capabilities {

RegionalCapabilitiesTestEnvironment::RegionalCapabilitiesTestEnvironment()
    : enabled_state_provider_(/*consent=*/false, /*enabled=*/false) {
  variations::TestVariationsService::RegisterPrefs(pref_service_.registry());
  metrics_state_manager_ = metrics::MetricsStateManager::Create(
      &pref_service_, &enabled_state_provider_,
      /*backup_registry_key=*/std::wstring(),
      /*user_data_dir=*/base::FilePath(), metrics::StartupVisibility::kUnknown);
  variations_service_ = std::make_unique<variations::TestVariationsService>(
      &pref_service_, metrics_state_manager_.get());
}

RegionalCapabilitiesTestEnvironment::~RegionalCapabilitiesTestEnvironment() =
    default;

TestingPrefServiceSimple& RegionalCapabilitiesTestEnvironment::pref_service() {
  return pref_service_;
}

variations::TestVariationsService&
RegionalCapabilitiesTestEnvironment::variations_service() {
  return *variations_service_;
}

}  // namespace regional_capabilities
