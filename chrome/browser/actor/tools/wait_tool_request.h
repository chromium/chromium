// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_WAIT_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_WAIT_TOOL_REQUEST_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor {

class WaitToolRequest : public ToolRequest {
 public:
  explicit WaitToolRequest(base::TimeDelta wait_duration);
  ~WaitToolRequest() override;

  // ToolRequest
  CreateToolResult CreateTool(TaskId task_id,
                              AggregatedJournal& journal) const override;
  std::string JournalEvent() const override;

 private:
  base::TimeDelta wait_duration_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_WAIT_TOOL_REQUEST_H_
