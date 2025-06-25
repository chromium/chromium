// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOL_REQUEST_H_

#include <memory>
#include <string_view>
#include <variant>

#include "base/types/expected.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/common/actor.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"

namespace actor {

class AggregatedJournal;
class Tool;

// Base class for all tool requests. For tools scoped to a tab (e.g. History
// traversal, Navigate) derive from TabToolRequest. For tools operating in a web
// contents, implemented in the renderer, derive from PageToolRequest. Tools not
// scoped to either can derive directly from this class.
class ToolRequest {
 public:
  ToolRequest();
  virtual ~ToolRequest();

  // Returns the URL to record in the journal when recording entries for this
  // request. This may be empty for requests that aren't tied to a frame/tab or
  // if the scoped object no longer exists.
  virtual GURL GetURLForJournal() const;

  // Returns the name to use for the journal when recording entries for this
  // request.
  virtual std::string JournalEvent() const = 0;

  struct CreateToolResult {
    CreateToolResult(std::unique_ptr<Tool> tool, mojom::ActionResultPtr result);
    ~CreateToolResult();
    std::unique_ptr<Tool> tool;
    mojom::ActionResultPtr result;
  };

  // Instantiates the tool requested by this object.
  virtual CreateToolResult CreateTool(TaskId task_id,
                                      AggregatedJournal& journal) const = 0;
};

// Tool requests targeting a specific, existing tab should inherit from this
// subclass.
class TabToolRequest : public ToolRequest {
 public:
  explicit TabToolRequest(const tabs::TabInterface::Handle tab_handle);
  ~TabToolRequest() override;

  // ToolRequest
  GURL GetURLForJournal() const override;

  // Returns a handle to the tab being targeted by this request. This handle
  // should never be null but it may be for a tab that is no longer available.
  tabs::TabInterface::Handle GetTabHandle() const;

 private:
  tabs::TabInterface::Handle tab_handle_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOL_REQUEST_H_
