// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_ON_DEVICE_MODEL_COMPONENT_OBSERVER_H_
#define CHROME_BROWSER_AI_AI_ON_DEVICE_MODEL_COMPONENT_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/component_updater/component_updater_service.h"

class AIManagerKeyedService;

class AIOnDeviceModelComponentObserver
    : public component_updater::ServiceObserver {
 public:
  explicit AIOnDeviceModelComponentObserver(AIManagerKeyedService* ai_manager);
  ~AIOnDeviceModelComponentObserver() override;
  AIOnDeviceModelComponentObserver(const AIOnDeviceModelComponentObserver&) =
      delete;
  AIOnDeviceModelComponentObserver& operator=(
      const AIOnDeviceModelComponentObserver&) = delete;

 protected:
  // component_updater::ServiceObserver:
  void OnEvent(const component_updater::CrxUpdateItem& item) override;

 private:
  base::ScopedObservation<component_updater::ComponentUpdateService,
                          component_updater::ComponentUpdateService::Observer>
      component_updater_observation_{this};

  raw_ptr<AIManagerKeyedService> ai_manager_;
};

#endif  // CHROME_BROWSER_AI_AI_ON_DEVICE_MODEL_COMPONENT_OBSERVER_H_
