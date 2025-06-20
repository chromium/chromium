// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_SELECT_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_SELECT_TOOL_REQUEST_H_

#include <memory>
#include <string>
#include <string_view>

#include "chrome/browser/actor/tools/page_tool_request.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor {

// Chooses an option in a <select> box on the page based on the value attribute
// of the <option> children.
class SelectToolRequest : public PageToolRequest {
 public:
  SelectToolRequest(tabs::TabHandle tab_handle,
                    const Target& target,
                    std::string_view value);
  ~SelectToolRequest() override;

  // ToolRequest
  std::string JournalEvent() const override;

  // PageToolRequest
  mojom::ToolActionPtr ToMojoToolAction() const override;
  std::unique_ptr<PageToolRequest> Clone() const override;

 private:
  // The <option> whose value attribute matches this parameter will be selected.
  std::string value_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_SELECT_TOOL_REQUEST_H_
