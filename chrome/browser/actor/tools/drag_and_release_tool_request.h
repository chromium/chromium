// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_DRAG_AND_RELEASE_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_DRAG_AND_RELEASE_TOOL_REQUEST_H_

#include <memory>
#include <string>

#include "chrome/browser/actor/tools/page_tool_request.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor {

// Simulates a mouse press, move, release sequence. As this is a PageTool, the
// sequence can only span a local subtree (i.e. cannot drag and drop between
// OOPIFs or RenderWidgetHosts).
class DragAndReleaseToolRequest : public PageToolRequest {
 public:
  DragAndReleaseToolRequest(tabs::TabHandle tab_handle,
                            const Target& from_target,
                            const Target& to_target);
  ~DragAndReleaseToolRequest() override;

  // ToolRequest
  std::string JournalEvent() const override;

  // PageToolRequest
  mojom::ToolActionPtr ToMojoToolAction() const override;
  std::unique_ptr<PageToolRequest> Clone() const override;

 private:
  Target from_target_;
  Target to_target_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_DRAG_AND_RELEASE_TOOL_REQUEST_H_
