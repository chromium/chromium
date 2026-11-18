// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_AI_AI_PAGE_ACTION_CONTROLLER_H_

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/on_device_model_names.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class PrefService;

namespace actions {
class ActionItem;
}

namespace tabs {
class TabInterface;
}

// Controls whether the on-device AI page action chip and bubble are shown.
BASE_DECLARE_FEATURE(kOnDeviceAiPageAction);

DECLARE_ELEMENT_IDENTIFIER_VALUE(kOnDeviceAiBubbleElementId);

// Shows an informative omnibox chip and bubble on the first on-device session
// using a newly installed model, e.g. Gemma 4.
class AIPageActionController
    : public optimization_guide::OnDeviceModelControl::Observer {
 public:
  DECLARE_USER_DATA(AIPageActionController);

  explicit AIPageActionController(BrowserWindowInterface& browser);
  ~AIPageActionController() override;

  AIPageActionController(const AIPageActionController&) = delete;
  AIPageActionController& operator=(const AIPageActionController&) = delete;

  static AIPageActionController* From(BrowserWindowInterface* browser);

  // optimization_guide::OnDeviceModelControl::Observer:
  void OnModelInstalled(
      optimization_guide::OnDeviceBaseModel base_model) override;
  void OnModelUninstalled(
      optimization_guide::OnDeviceBaseModel base_model) override;

  // Shows the page action chip on `tab` when a `base_model` session is created.
  void OnModelSessionCreated(optimization_guide::OnDeviceBaseModel base_model,
                             tabs::TabInterface* tab);

  // Shows the bubble anchored to the omnibox chip or icon.
  void ShowBubble(actions::ActionItem* action_item);

 private:
  const raw_ref<BrowserWindowInterface> browser_;
  raw_ptr<PrefService> local_state_ = nullptr;
  raw_ptr<optimization_guide::OnDeviceModelControl> model_control_ = nullptr;
  base::OneShotTimer chip_collapse_timer_;
  base::ScopedObservation<optimization_guide::OnDeviceModelControl,
                          optimization_guide::OnDeviceModelControl::Observer>
      observation_{this};
  ui::ScopedUnownedUserData<AIPageActionController> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_AI_AI_PAGE_ACTION_CONTROLLER_H_
