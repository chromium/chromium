// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_SETUP_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_SETUP_CONTROLLER_H_

#include <map>
#include <string>

#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/public/cpp/assistant/assistant_setup.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace ash {

class AssistantController;

class AssistantSetupController : public AssistantControllerObserver,
                                 public AssistantViewDelegateObserver {
 public:
  explicit AssistantSetupController(AssistantController* assistant_controller);
  ~AssistantSetupController() override;

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;

  // AssistantViewDelegateObserver:
  void OnOptInButtonPressed() override;

  void StartOnboarding(bool relaunch, FlowType type = FlowType::kConsentFlow);

 private:
  void OnOptInFlowFinished(bool completed);

  AssistantController* const assistant_controller_;  // Owned by Shell.

  base::WeakPtrFactory<AssistantSetupController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantSetupController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_SETUP_CONTROLLER_H_
