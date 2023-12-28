// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_UI_CONTROLLER_IMPL_H_
#define ASH_ASSISTANT_ASSISTANT_UI_CONTROLLER_IMPL_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"

class PrefRegistrySimple;

namespace chromeos {
namespace assistant {
namespace mojom {
class Assistant;
}  // namespace mojom
}  // namespace assistant
}  // namespace chromeos

namespace ash {

class AssistantControllerImpl;

class ASH_EXPORT AssistantUiControllerImpl
    : public AssistantUiController,
      public AssistantControllerObserver,
      public AssistantInteractionModelObserver,
      public AssistantUiModelObserver,
      public AssistantViewDelegateObserver,
      public OverviewObserver {
 public:
  explicit AssistantUiControllerImpl(
      AssistantControllerImpl* assistant_controller);

  AssistantUiControllerImpl(const AssistantUiControllerImpl&) = delete;
  AssistantUiControllerImpl& operator=(const AssistantUiControllerImpl&) =
      delete;

  ~AssistantUiControllerImpl() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Provides a pointer to the |assistant| owned by AssistantService.
  void SetAssistant(assistant::Assistant* assistant);

  // AssistantUiController:
  const AssistantUiModel* GetModel() const override;
  int GetNumberOfSessionsWhereOnboardingShown() const override;
  bool HasShownOnboarding() const override;
  void SetKeyboardTraversalMode(bool keyboard_traversal_mode) override;
  void ShowUi(AssistantEntryPoint entry_point) override;
  void ToggleUi(std::optional<AssistantEntryPoint> entry_point,
                std::optional<AssistantExitPoint> exit_point) override;
  std::optional<base::ScopedClosureRunner> CloseUi(
      AssistantExitPoint exit_point) override;
  void SetAppListBubbleWidth(int width) override;

  // AssistantInteractionModelObserver:
  void OnInteractionStateChanged(InteractionState interaction_state) override;

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;
  void OnOpeningUrl(const GURL& url,
                    bool in_background,
                    bool from_server) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      std::optional<AssistantEntryPoint> entry_point,
      std::optional<AssistantExitPoint> exit_point) override;

  // AssistantViewDelegateObserver:
  void OnOnboardingShown() override;

  // OverviewObserver:
  void OnOverviewModeWillStart() override;

  void ShowUnboundErrorToast();

 private:
  const raw_ptr<AssistantControllerImpl>
      assistant_controller_;  // Owned by Shell.
  AssistantUiModel model_;
  bool has_shown_onboarding_ = false;

  // Owned by AssistantService.
  raw_ptr<assistant::Assistant> assistant_ = nullptr;

  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      assistant_controller_observation_{this};

  base::ScopedObservation<OverviewController, OverviewObserver>
      overview_controller_observation_{this};

  base::WeakPtrFactory<AssistantUiControllerImpl>
      weak_factory_for_delayed_visibility_changes_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_UI_CONTROLLER_IMPL_H_
