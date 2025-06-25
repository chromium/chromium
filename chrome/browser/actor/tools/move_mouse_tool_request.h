// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_MOVE_MOUSE_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_MOVE_MOUSE_TOOL_REQUEST_H_

#include <memory>
#include <string>

#include "chrome/browser/actor/tools/page_tool_request.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor {

// Injects a mouse move event at the given target.
class MoveMouseToolRequest : public PageToolRequest {
 public:
  MoveMouseToolRequest(tabs::TabHandle tab_handle, const Target& target);
  ~MoveMouseToolRequest() override;

  // ToolRequest
  std::string JournalEvent() const override;

  // PageToolRequest
  mojom::ToolActionPtr ToMojoToolAction() const override;
  std::unique_ptr<PageToolRequest> Clone() const override;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_MOVE_MOUSE_TOOL_REQUEST_H_
