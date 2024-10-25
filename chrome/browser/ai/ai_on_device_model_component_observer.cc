// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_on_device_model_component_observer.h"

#include "base/types/pass_key.h"
#include "chrome/browser/ai/ai_manager_keyed_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/optimization_guide_on_device_model_installer.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"

AIOnDeviceModelComponentObserver::AIOnDeviceModelComponentObserver(
    AIManagerKeyedService* ai_manager)
    : ai_manager_(ai_manager) {
  if (g_browser_process->component_updater()) {
    component_updater_observation_.Observe(
        g_browser_process->component_updater());
  }
}

AIOnDeviceModelComponentObserver::~AIOnDeviceModelComponentObserver() {
  component_updater_observation_.Reset();
}

void AIOnDeviceModelComponentObserver::OnEvent(
    const component_updater::CrxUpdateItem& item) {
  if (item.id !=
      component_updater::OptimizationGuideOnDeviceModelInstallerPolicy::
          GetOnDeviceModelExtensionId()) {
    return;
  }

  if (item.state == update_client::ComponentState::kDownloading ||
      item.state == update_client::ComponentState::kDownloadingDiff ||
      item.state == update_client::ComponentState::kUpToDate) {
    if (item.downloaded_bytes >= 0 && item.total_bytes >= 0) {
      ai_manager_->OnTextModelDownloadProgressChange(
          base::PassKey<AIOnDeviceModelComponentObserver>(),
          item.downloaded_bytes, item.total_bytes);
    }
  }
}
