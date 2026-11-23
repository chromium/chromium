// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_PASSWORD_MANAGER_CHANGE_PASSWORD_CHANGE_PASSWORD_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_PASSWORD_MANAGER_CHANGE_PASSWORD_CHANGE_PASSWORD_TOOL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor.mojom-forward.h"
#include "components/actor/core/shared_types.h"
#include "components/password_manager/core/browser/password_change/change_password_filling_helper.h"

namespace actor {

class ActorSurface;

class ChangePasswordTool : public Tool {
 public:
  ChangePasswordTool(TaskId task_id,
                     ToolDelegate& tool_delegate,
                     ActorSurface& actor_surface,
                     std::optional<PageTarget> current_password_field,
                     std::optional<PageTarget> new_password_field,
                     std::optional<PageTarget> confirm_new_password_field,
                     std::optional<PageTarget> confirm_button);
  ~ChangePasswordTool() override;

  void Validate(DeprecatedToolCallback callback) override;
  mojom::ActionResultPtr TimeOfUseValidation(
      const optimization_guide::proto::AnnotatedPageContent* last_observation)
      override;
  void Invoke(DeprecatedToolCallback callback) override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  ActorSurfaceHandle GetTargetActorSurface() const override;

 private:
  void OnChangePasswordFormFilled(
      DeprecatedToolCallback callback,
      password_manager::ChangePasswordActionResult result);

  ActorSurfaceHandle actor_surface_handle_;
  std::optional<PageTarget> current_password_field_;
  std::optional<PageTarget> new_password_field_;
  std::optional<PageTarget> confirm_new_password_field_;
  std::optional<PageTarget> confirm_button_;

  password_manager::ChangePasswordFillingHelper filling_helper_;

  base::WeakPtrFactory<ChangePasswordTool> weak_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_PASSWORD_MANAGER_CHANGE_PASSWORD_CHANGE_PASSWORD_TOOL_H_
