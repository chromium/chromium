// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_CLICK_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_CLICK_TOOL_REQUEST_H_

#include <memory>
#include <string>

#include "chrome/browser/actor/tools/page_tool_request.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor {

class ClickToolRequest : public PageToolRequest {
 public:
  enum class ClickType { kLeft, kRight };
  enum class ClickCount { kSingle, kDouble };

  ClickToolRequest(tabs::TabHandle tab_handle,
                   const Target& target,
                   ClickType type,
                   ClickCount count);
  ~ClickToolRequest() override;

  // ToolRequest
  std::string JournalEvent() const override;

  // PageToolRequest
  mojom::ToolActionPtr ToMojoToolAction() const override;
  std::unique_ptr<PageToolRequest> Clone() const override;

 private:
  ClickType click_type_;
  ClickCount click_count_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_CLICK_TOOL_REQUEST_H_
