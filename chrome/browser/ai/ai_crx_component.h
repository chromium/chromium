// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_CRX_COMPONENT_H_
#define CHROME_BROWSER_AI_AI_CRX_COMPONENT_H_

#include "chrome/browser/ai/ai_model_download_progress_manager.h"
#include "components/component_updater/component_updater_service.h"

namespace on_device_ai {

// A `AIModelDownloadProgressManager::Component` to report progress updates from
// the `ComponentUpdateService` for the `component_id`.
class AICrxComponent : public AIModelDownloadProgressManager::Component,
                       public component_updater::ServiceObserver {
 public:
  // Helper function to convert a set of crx component ids to a set of
  // `std::unique_ptr`s of this class.
  static base::flat_set<std::unique_ptr<Component>> FromComponentIds(
      component_updater::ComponentUpdateService* component_update_service,
      base::flat_set<std::string> component_ids);

  AICrxComponent(
      component_updater::ComponentUpdateService* component_update_service,
      std::string component_id);
  ~AICrxComponent() override;

  // component_updater::ServiceObserver:
  void OnEvent(const component_updater::CrxUpdateItem& item) override;

 private:
  std::string component_id_;

  base::ScopedObservation<component_updater::ComponentUpdateService,
                          component_updater::ComponentUpdateService::Observer>
      component_updater_observation_{this};
};
}  // namespace on_device_ai

#endif  // CHROME_BROWSER_AI_AI_CRX_COMPONENT_H_
