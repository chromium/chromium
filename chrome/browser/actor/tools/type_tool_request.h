// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TYPE_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TYPE_TOOL_REQUEST_H_

#include <memory>
#include <string>

#include "chrome/browser/actor/tools/page_tool_request.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor {

class TypeToolRequest : public PageToolRequest {
 public:
  enum class Mode {
    // Replace all existing text in the editing context.
    kReplace,

    // Insert text before any existing text in the editing context.
    kPrepend,

    // Insert text after any existing text in the editing context.
    kAppend
  };

  TypeToolRequest(tabs::TabHandle tab_handle,
                  const Target& target,
                  std::string_view text,
                  bool follow_by_enter,
                  Mode mode);
  ~TypeToolRequest() override;

  // ToolRequest
  std::string JournalEvent() const override;

  // PageToolRequest
  mojom::ToolActionPtr ToMojoToolAction() const override;
  std::unique_ptr<PageToolRequest> Clone() const override;

  // Text to type.
  std::string text;

  // Whether to inject an enter/return key after typing.
  bool follow_by_enter;

  // Behavior with respect to existing text.
  Mode mode;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TYPE_TOOL_REQUEST_H_
