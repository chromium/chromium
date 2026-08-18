// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TRANSLATE_PAGE_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TRANSLATE_PAGE_TOOL_REQUEST_H_

#include <string>
#include <string_view>

#include "chrome/browser/actor/tools/tool_request.h"

namespace actor {

// A tool request for translating the page in a specific tab.
class TranslatePageToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "TranslatePage";

  // `target_language`: The ISO 639 language code (e.g., "en", "es", "fr",
  // "zh-CN") to translate the page to. If empty (default), Chrome translates
  // the page to the user's preferred target language as determined by
  // language settings.
  explicit TranslatePageToolRequest(tabs::TabHandle tab_handle,
                                    std::string_view target_language = "");
  ~TranslatePageToolRequest() override;

  std::string_view target_language() const { return target_language_; }

  // TabToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;
  std::string JournalEvent() const override;
  bool RequiresUrlCheckInCurrentTab() const override;

 private:
  std::string target_language_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TRANSLATE_PAGE_TOOL_REQUEST_H_
