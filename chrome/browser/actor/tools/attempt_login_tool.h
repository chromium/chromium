// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_LOGIN_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_LOGIN_TOOL_H_

#include "chrome/browser/actor/tools/tool.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

class AttemptLoginTool : public Tool {
 public:
  AttemptLoginTool(TaskId task_id,
                   AggregatedJournal& journal,
                   tabs::TabInterface& tab);
  ~AttemptLoginTool() override;

  // actor::Tool
  void Validate(ValidateCallback callback) override;
  void Invoke(InvokeCallback callback) override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer()
      const override;
  void UpdateTaskBeforeInvoke(ActorTask& task) const override;

 private:
  tabs::TabHandle tab_handle_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_LOGIN_TOOL_H_
