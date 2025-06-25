// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TAB_MANAGEMENT_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TAB_MANAGEMENT_TOOL_REQUEST_H_

#include <string>

#include "chrome/browser/actor/tools/tool_request.h"
#include "ui/base/window_open_disposition.h"

namespace actor {

// Creates a new blank tab in the specified window.
class CreateTabToolRequest : public ToolRequest {
 public:
  enum class Disposition { kForeground, kBackground };

  CreateTabToolRequest(int32_t window_id, WindowOpenDisposition disposition);
  ~CreateTabToolRequest() override;

  CreateToolResult CreateTool(TaskId task_id,
                              AggregatedJournal& journal) const override;
  std::string JournalEvent() const override;

 private:
  int32_t window_id_;
  WindowOpenDisposition disposition_;
};

// Brings the specified tab to the foreground.
class ActivateTabToolRequest : public TabToolRequest {
 public:
  explicit ActivateTabToolRequest(tabs::TabHandle tab);
  ~ActivateTabToolRequest() override;
  CreateToolResult CreateTool(TaskId task_id,
                              AggregatedJournal& journal) const override;
  std::string JournalEvent() const override;
};

// Closes the specified tab to the foreground.
class CloseTabToolRequest : public TabToolRequest {
 public:
  explicit CloseTabToolRequest(tabs::TabHandle tab);
  ~CloseTabToolRequest() override;
  CreateToolResult CreateTool(TaskId task_id,
                              AggregatedJournal& journal) const override;
  std::string JournalEvent() const override;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TAB_MANAGEMENT_TOOL_REQUEST_H_
