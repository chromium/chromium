// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOL_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOL_CONTROLLER_H_

#include "chrome/browser/actor/tools/tool_invocation.h"

namespace actor {

// Entry point into actor tool usage. This class routes a tool use request to
// the appropriate browser tool or to a corresponding executor in the renderer
// for page-level tools.
class ToolController {
 public:
  ToolController();
  ~ToolController();
  ToolController(const ToolController&) = delete;
  ToolController& operator=(const ToolController&) = delete;

  // Invokes a tool action.
  void Invoke(const ToolInvocation& action,
              ToolInvocation::ResultCallback result_callback);
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOL_CONTROLLER_H_
