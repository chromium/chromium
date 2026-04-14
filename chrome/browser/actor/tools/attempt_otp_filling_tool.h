// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_OTP_FILLING_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_OTP_FILLING_TOOL_H_

#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/common/actor/task_id.h"

namespace actor {

// A tool that attempts to retrieve a one-time password (OTP) and fill it into a
// specified field (or fields) on the page.
class AttemptOtpFillingTool : public Tool {
 public:
  AttemptOtpFillingTool(TaskId task_id, ToolDelegate& tool_delegate);
  ~AttemptOtpFillingTool() override;

  // Tool:
  void Validate(ToolCallback callback) override;
  void Invoke(ToolCallback callback) override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  tabs::TabHandle GetTargetTab() const override;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_OTP_FILLING_TOOL_H_
