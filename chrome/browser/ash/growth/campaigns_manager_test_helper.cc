// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/campaigns_manager_test_helper.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/growth/campaigns_manager_client_impl.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/global_features.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/variations/service/test_variations_service.h"

CampaignsManagerTestHelper::CampaignsManagerTestHelper() = default;

CampaignsManagerTestHelper::~CampaignsManagerTestHelper() = default;

void CampaignsManagerTestHelper::InitializeCampaignsManager(
    scoped_refptr<component_updater::ComponentManagerAsh>
        component_manager_ash) {
  CHECK(component_manager_ash);

  browser_process_platform_part_test_api_ =
      std::make_unique<BrowserProcessPlatformPartTestApi>(
          TestingBrowserProcess::GetGlobal()->platform_part());
  browser_process_platform_part_test_api_->InitializeComponentManager(
      std::move(component_manager_ash));

  metrics_enabled_state_provider_ =
      std::make_unique<metrics::TestEnabledStateProvider>(/*consent=*/false,
                                                          /*enabled=*/false);
  metrics_state_manager_ = metrics::MetricsStateManager::Create(
      TestingBrowserProcess::GetGlobal()->local_state(),
      metrics_enabled_state_provider_.get(),
      /*backup_registry_key=*/std::wstring(),
      /*user_data_dir=*/base::FilePath(), metrics::StartupVisibility::kUnknown);

  variations_service_ = std::make_unique<variations::TestVariationsService>(
      TestingBrowserProcess::GetGlobal()->local_state(),
      metrics_state_manager_.get());
  variations_service_->OverrideStoredPermanentCountry("zz");

  TestingBrowserProcess::GetGlobal()->SetVariationsService(
      variations_service_.get());

  campaigns_manager_client_impl_ = std::make_unique<CampaignsManagerClientImpl>(
      TestingBrowserProcess::GetGlobal()->local_state(),
      TestingBrowserProcess::GetGlobal()
          ->GetFeatures()
          ->application_locale_storage(),
      TestingBrowserProcess::GetGlobal()->variations_service(),
      TestingBrowserProcess::GetGlobal()
          ->platform_part()
          ->component_manager_ash());
}

void CampaignsManagerTestHelper::ShutdownCampaignsManager() {
  campaigns_manager_client_impl_.reset();
  TestingBrowserProcess::GetGlobal()->SetVariationsService(nullptr);
  variations_service_.reset();
  metrics_state_manager_.reset();
  metrics_enabled_state_provider_.reset();
  browser_process_platform_part_test_api_->ShutdownComponentManager();
  browser_process_platform_part_test_api_.reset();
}
