// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_TEST_HELPER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"

namespace component_updater {
class ComponentManagerAsh;
}  // namespace component_updater

namespace metrics {
class MetricsStateManager;
class TestEnabledStateProvider;
}  // namespace metrics

namespace variations {
class TestVariationsService;
}  // namespace variations

class CampaignsManagerClientImpl;

// Sets up CampaignsManager and CampaignsManagerClientImpl for unit tests.
class CampaignsManagerTestHelper {
 public:
  CampaignsManagerTestHelper();
  CampaignsManagerTestHelper(const CampaignsManagerTestHelper&) = delete;
  CampaignsManagerTestHelper& operator=(const CampaignsManagerTestHelper&) =
      delete;
  ~CampaignsManagerTestHelper();

  // Creates CampaignsManagerClientImpl and its dependencies.
  // `BrowserProcessPlatformPart::component_manager_ash` is overridden with
  // `component_manager_ash`.
  // `component_manager_ash` must be non-null.
  void InitializeCampaignsManager(
      scoped_refptr<component_updater::ComponentManagerAsh>
          component_manager_ash);

  // Destroys CampaignsManagerClientImpl and its dependencies.
  // `BrowserProcessPlatformPart::component_manager_ash` is reset.
  void ShutdownCampaignsManager();

  CampaignsManagerClientImpl* campaigns_manager_client_impl() {
    return campaigns_manager_client_impl_.get();
  }

 private:
  std::unique_ptr<BrowserProcessPlatformPartTestApi>
      browser_process_platform_part_test_api_;

  std::unique_ptr<metrics::TestEnabledStateProvider>
      metrics_enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::TestVariationsService> variations_service_;
  std::unique_ptr<CampaignsManagerClientImpl> campaigns_manager_client_impl_;
};

#endif  // CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_TEST_HELPER_H_
