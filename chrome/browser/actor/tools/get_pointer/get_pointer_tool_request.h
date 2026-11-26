// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_GET_POINTER_GET_POINTER_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_GET_POINTER_GET_POINTER_TOOL_REQUEST_H_

#include <optional>
#include <string_view>
#include <vector>

#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/get_pointer/get_pointer_result_builder.h"
#include "chrome/browser/actor/tools/tool_request.h"

namespace actor {

class ToolRequestVisitorFunctor;

// Asks what the user pointed at in a tab. See `BuildGetPointerResult()` for
// the answer.
class GetPointerToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "GetPointer";
  static constexpr char kModelFacingName[] = "get_pointer";

  // `locations` are the places the user pointed at, most recent first, in the
  // DIP space of the tab's viewport. They come from the caller, which tracks
  // the pointer, and not from the model: the tool takes no arguments.
  GetPointerToolRequest(ActorSurfaceHandle actor_surface_handle,
                        std::vector<PointerLocation> locations,
                        GetPointerResultOptions options);
  GetPointerToolRequest(const GetPointerToolRequest&);
  GetPointerToolRequest& operator=(const GetPointerToolRequest&);
  ~GetPointerToolRequest() override;

  // Returns the `ToolId::kGetPointer` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  // ToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;

  const std::vector<PointerLocation>& locations() const { return locations_; }
  const GetPointerResultOptions& options() const { return options_; }

 private:
  std::vector<PointerLocation> locations_;
  GetPointerResultOptions options_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_GET_POINTER_GET_POINTER_TOOL_REQUEST_H_
