// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_LOAD_AND_EXTRACT_CONTENT_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_LOAD_AND_EXTRACT_CONTENT_TOOL_REQUEST_H_

#include <memory>
#include <string_view>
#include <vector>

#include "chrome/browser/actor/tools/tool_request.h"

class GURL;

namespace actor {

// A request to load a set of URLs and extract their content.
// TODO(b/443954134): Consider adding a parameter for which window to use.
class LoadAndExtractContentToolRequest : public ToolRequest {
 public:
  static constexpr char kName[] = "LoadAndExtractContent";

  explicit LoadAndExtractContentToolRequest(std::vector<GURL> urls);
  ~LoadAndExtractContentToolRequest() override;

  LoadAndExtractContentToolRequest(const LoadAndExtractContentToolRequest&);
  LoadAndExtractContentToolRequest& operator=(
      const LoadAndExtractContentToolRequest&);

  // ToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;

 private:
  std::vector<GURL> urls_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_LOAD_AND_EXTRACT_CONTENT_TOOL_REQUEST_H_
