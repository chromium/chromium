// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOL_REQUEST_H_

#include <memory>
#include <optional>
#include <string_view>
#include <variant>

#include "base/types/expected.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/task_id.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace actor {

class Tool;
class ToolDelegate;
class ToolRequestVisitorFunctor;

// Base class for all tool requests. For tools scoped to a tab (e.g. History
// traversal, Navigate) derive from TabToolRequest. For tools operating in a web
// contents, implemented in the renderer, derive from PageToolRequest. Tools not
// scoped to either can derive directly from this class.
class ToolRequest {
 public:
  ToolRequest();
  virtual ~ToolRequest();
  ToolRequest(const ToolRequest& other);
  ToolRequest& operator=(const ToolRequest& other);

  bool IsTabScoped() const;

  // Returns true if this action will add a tab to the set of observed tasks.
  virtual bool AddsTabToObservationSet() const;

  // Returns the URL to record in the journal when recording entries for this
  // request. This may be empty for requests that aren't tied to a frame/tab or
  // if the scoped object no longer exists.
  virtual GURL GetURLForJournal() const;

  // Returns a handle to the tab being targeted by this request. The default
  // (non-tab, non-page scoped tool requests) returns a null handle.
  virtual tabs::TabHandle GetTabHandle() const;

  // Returns true if this tool takes action within the page of the current tab
  // and thus requires checking the current tab's URL for safety checks.
  // Typically, most tab scoped tools will return true here but, for example, a
  // navigate tool is tab scoped but navigates *away* from the current URL.
  virtual bool RequiresUrlCheckInCurrentTab() const;

  // Returns the name to use for the journal when recording entries for this
  // request. This should only be overridden if Name() isn't descriptive enough.
  virtual std::string JournalEvent() const;

  // Returns the name of the ToolRequest.
  // NOTE: This value is persisted to UMA logs so do not change after a
  // ToolRequest is added.
  virtual std::string_view Name() const = 0;

  // Used by ConvertToVariantFn to convert a polymorphic ToolRequest object into
  // the proper ToolRequestVariant type.
  virtual void Apply(ToolRequestVisitorFunctor&) const = 0;

  struct CreateToolResult {
    CreateToolResult(std::unique_ptr<Tool> tool, mojom::ActionResultPtr result);
    ~CreateToolResult();
    std::unique_ptr<Tool> tool;
    mojom::ActionResultPtr result;
  };

  // Instantiates the tool requested by this object.
  virtual CreateToolResult CreateTool(TaskId task_id,
                                      ToolDelegate& tool_delegate) const = 0;

  // Gets origin associated with the tool request, if one exists. Right now only
  // navigate requests have this origin. When origin gating is enabled, these
  // origins are cached and the browser may navigate the browser via link or
  // other means to this origin without prompting the user. Since this is a
  // security feature, new tool requests should not use this method unless it is
  // safe to use their origins as a trust signal.
  virtual std::optional<url::Origin> AssociatedOriginGrant() const;

  // Gets configuration for general page stability on observation.
  virtual ObservationDelayController::PageStabilityConfig
  GetObservationPageStabilityConfig() const;
};

// Tool requests targeting a specific, existing tab should inherit from this
// subclass.
class TabToolRequest : public ToolRequest {
 public:
  explicit TabToolRequest(const tabs::TabHandle tab_handle);
  ~TabToolRequest() override;
  TabToolRequest(const TabToolRequest& other);
  TabToolRequest& operator=(const TabToolRequest& other);

  // ToolRequest
  GURL GetURLForJournal() const override;

  // Returns a handle to the tab being targeted by this request. For tab scoped
  // requests this handle will never be a null value but it may point to a tab
  // that is no longer available.
  tabs::TabHandle GetTabHandle() const override;

 private:
  tabs::TabHandle tab_handle_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOL_REQUEST_H_
