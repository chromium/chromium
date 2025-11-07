// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_form_filling_tool_request.h"

#include <ostream>
#include <variant>

#include "chrome/browser/actor/tools/attempt_form_filling_tool.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/gfx/geometry/point.h"

namespace actor {

AttemptFormFillingToolRequest::FormFillingRequest::FormFillingRequest() =
    default;
AttemptFormFillingToolRequest::FormFillingRequest::~FormFillingRequest() =
    default;
AttemptFormFillingToolRequest::FormFillingRequest::FormFillingRequest(
    const FormFillingRequest&) = default;
AttemptFormFillingToolRequest::FormFillingRequest&
AttemptFormFillingToolRequest::FormFillingRequest::operator=(
    const FormFillingRequest&) = default;
AttemptFormFillingToolRequest::FormFillingRequest::FormFillingRequest(
    FormFillingRequest&&) = default;
AttemptFormFillingToolRequest::FormFillingRequest&
AttemptFormFillingToolRequest::FormFillingRequest::operator=(
    FormFillingRequest&&) = default;

AttemptFormFillingToolRequest::AttemptFormFillingToolRequest(
    tabs::TabHandle tab_handle,
    std::vector<FormFillingRequest> requests)
    : TabToolRequest(tab_handle), requests_(std::move(requests)) {}

AttemptFormFillingToolRequest::AttemptFormFillingToolRequest(
    const AttemptFormFillingToolRequest&) = default;
AttemptFormFillingToolRequest& AttemptFormFillingToolRequest::operator=(
    const AttemptFormFillingToolRequest&) = default;

AttemptFormFillingToolRequest::~AttemptFormFillingToolRequest() = default;

ToolRequest::CreateToolResult AttemptFormFillingToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  tabs::TabInterface* tab = GetTabHandle().Get();
  if (!tab) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }

  return {std::make_unique<AttemptFormFillingTool>(task_id, tool_delegate, *tab,
                                                   std::move(requests_)),
          MakeOkResult()};
}

std::string_view AttemptFormFillingToolRequest::Name() const {
  return kName;
}

void AttemptFormFillingToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::ostream& operator<<(
    std::ostream& out,
    const AttemptFormFillingToolRequest::FormFillingRequest& request) {
  out << "Request(" << static_cast<int>(request.requested_data);
  for (const auto& field : request.trigger_fields) {
    if (std::holds_alternative<gfx::Point>(field)) {
      out << ", Point(" << field << ")";
    } else {
      out << ", " << field;
    }
  }
  out << ")";
  return out;
}

std::ostream& operator<<(
    std::ostream& out,
    const std::vector<AttemptFormFillingToolRequest::FormFillingRequest>&
        requests) {
  // base::ToString() provides a formatter for base::span()
  return out << base::ToString(base::span(requests));
}

}  // namespace actor
