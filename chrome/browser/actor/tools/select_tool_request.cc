// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/select_tool_request.h"

#include <optional>
#include <string_view>

#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"

namespace actor {

namespace {

// Default description for the `select_option` tool.
constexpr std::string_view kSelectOptionToolDescription =
    "Selects an option from a dropdown (<select>) element on the active "
    "webpage.";

// Description for the `dom_node_id` parameter of the `select_option` tool.
constexpr std::string_view kDomNodeIdParamDescription =
    "The numeric DOM node ID of the target dropdown element (e.g. 101).";

// Description for the `value` parameter of the `select_option` tool.
constexpr std::string_view kValueParamDescription =
    "The value of the <option> element to select.";

}  // namespace

using ::tabs::TabHandle;

SelectToolRequest::SelectToolRequest(TabHandle tab_handle,
                                     const PageTarget& target,
                                     std::string_view value)
    : PageToolRequest(tab_handle, target), value_(value) {}

SelectToolRequest::~SelectToolRequest() = default;

// static
std::optional<ToolDefinition> SelectToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kSelectOption, kModelFacingName,
                               kSelectOptionToolDescription)
      .SetToolParameterSchema(
          ToolSchemaBuilder()
              .AddIntegerProperty(kDomNodeIdParam, kDomNodeIdParamDescription)
              .AddStringProperty(kValueParam, kValueParamDescription))
      .Build();
}

void SelectToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view SelectToolRequest::Name() const {
  return kName;
}

mojom::ToolActionPtr SelectToolRequest::ToMojoToolAction(
    content::RenderFrameHost& frame) const {
  auto select = mojom::SelectAction::New();

  select->value = value_;

  return mojom::ToolAction::NewSelect(std::move(select));
}

std::unique_ptr<PageToolRequest> SelectToolRequest::Clone() const {
  return std::make_unique<SelectToolRequest>(*this);
}

}  // namespace actor
