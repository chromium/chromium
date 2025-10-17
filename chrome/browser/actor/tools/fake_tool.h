// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_FAKE_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_FAKE_TOOL_H_

#include "chrome/browser/actor/tools/tool.h"
#include "chrome/common/actor/action_result.h"

namespace actor {

// Fake tool that allows test to control the timing of callback.
class FakeTool : public Tool {
 public:
  FakeTool(TaskId task_id,
           ToolDelegate& tool_delegate,
           base::OnceClosure on_invoke,
           base::OnceClosure on_destroy);

  ~FakeTool() override;

  void Validate(ValidateCallback callback) override;

  void Invoke(InvokeCallback callback) override;

  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      std::optional<ObservationDelayController::PageStabilityConfig>
          page_stability_config) override;

  tabs::TabHandle GetTargetTab() const override;

 private:
  base::OnceClosure on_invoke_;
  base::OnceClosure on_destroy_;
  InvokeCallback invoke_callback_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_FAKE_TOOL_H_
