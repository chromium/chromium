// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_OTP_FILLING_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_OTP_FILLING_TOOL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/task_id.h"
#include "components/actor/core/shared_types.h"
#include "components/autofill/core/common/unique_ids.h"

namespace actor {

// A tool that attempts to retrieve a one-time password (OTP) and fill it into
// the specified fields on the page. (One field or many smaller ones.)
// If this is part of a sign-in flow, set `for_signin` to true.
class AttemptOtpFillingTool : public Tool {
 public:
  AttemptOtpFillingTool(TaskId task_id,
                        ToolDelegate& tool_delegate,
                        tabs::TabHandle tab_handle,
                        std::vector<PageTarget> trigger_fields,
                        bool for_signin);
  ~AttemptOtpFillingTool() override;

  // Tool:
  void Validate(ToolCallback callback) override;
  mojom::ActionResultPtr TimeOfUseValidation(
      const optimization_guide::proto::AnnotatedPageContent* last_observation)
      override;
  void Invoke(ToolCallback callback) override;

  void UpdateTaskBeforeInvoke(ActorTask& task,
                              ToolCallback callback) const override;

  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  tabs::TabHandle GetTargetTab() const override;

 private:
  void OnOtpRetrieved(ToolCallback callback, std::string otp);
  void OnOtpFilled(ToolCallback callback, bool success);

  tabs::TabHandle tab_handle_;
  std::vector<PageTarget> trigger_fields_;
  std::vector<autofill::FieldGlobalId> trigger_field_ids_;
  bool for_signin_;

  base::WeakPtrFactory<AttemptOtpFillingTool> weak_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_OTP_FILLING_TOOL_H_
