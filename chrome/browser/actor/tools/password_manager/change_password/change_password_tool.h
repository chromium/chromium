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
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"

namespace password_manager {
class ContentPasswordManagerDriver;
}  // namespace password_manager

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

  void Validate(ToolCallback callback) override;
  mojom::ActionResultPtr TimeOfUseValidation(
      const optimization_guide::proto::AnnotatedPageContent* last_observation)
      override;
  void Invoke(ToolCallback callback) override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  ActorSurfaceHandle GetTargetActorSurface() const override;

 private:
  mojom::ActionResultPtr ExtractFieldIds(
      const optimization_guide::proto::AnnotatedPageContent* last_observation,
      autofill::LocalFrameToken& target_frame_token);
  void OnChangePasswordFormFilled(
      ToolCallback callback,
      const std::optional<autofill::FormData>& form_data);

  ActorSurfaceHandle actor_surface_handle_;
  std::optional<PageTarget> current_password_field_;
  std::optional<PageTarget> new_password_field_;
  std::optional<PageTarget> confirm_new_password_field_;
  std::optional<PageTarget> confirm_button_;

  base::WeakPtr<password_manager::ContentPasswordManagerDriver> driver_;
  autofill::FieldGlobalId current_password_field_id_;
  autofill::FieldGlobalId new_password_field_id_;
  autofill::FieldGlobalId confirm_new_password_field_id_;

  base::WeakPtrFactory<ChangePasswordTool> weak_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_PASSWORD_MANAGER_CHANGE_PASSWORD_CHANGE_PASSWORD_TOOL_H_
