// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/translate_page_tool_request.h"

#include <utility>

#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/browser/actor/tools/translate_page_tool.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace actor {

TranslatePageToolRequest::TranslatePageToolRequest(
    tabs::TabHandle tab_handle,
    std::string_view target_language)
    : TabToolRequest(tab_handle), target_language_(target_language) {}

TranslatePageToolRequest::~TranslatePageToolRequest() = default;

ToolRequest::CreateToolResult TranslatePageToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  tabs::TabInterface* tab = GetTabHandle().Get();
  if (!tab) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }
  return {std::make_unique<TranslatePageTool>(task_id, tool_delegate, *tab,
                                              target_language_),
          MakeOkResult()};
}

void TranslatePageToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view TranslatePageToolRequest::Name() const {
  return kName;
}

std::string TranslatePageToolRequest::JournalEvent() const {
  if (target_language_.empty()) {
    return std::string(Name());
  }
  return absl::StrFormat("%s[%s]", Name(), target_language_);
}

bool TranslatePageToolRequest::RequiresUrlCheckInCurrentTab() const {
  return true;
}

}  // namespace actor
