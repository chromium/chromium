// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/select_tool_request.h"

#include "chrome/common/actor.mojom.h"

namespace actor {

using ::tabs::TabHandle;

SelectToolRequest::SelectToolRequest(TabHandle tab_handle,
                                     const Target& target,
                                     std::string_view value)
    : PageToolRequest(tab_handle, target), value_(value) {}

SelectToolRequest::~SelectToolRequest() = default;

std::string SelectToolRequest::JournalEvent() const {
  return "Select";
}

mojom::ToolActionPtr SelectToolRequest::ToMojoToolAction() const {
  auto select = mojom::SelectAction::New();

  select->target = PageToolRequest::ToMojoToolTarget(GetTarget());
  select->value = value_;

  return mojom::ToolAction::NewSelect(std::move(select));
}

std::unique_ptr<PageToolRequest> SelectToolRequest::Clone() const {
  return std::make_unique<SelectToolRequest>(*this);
}

}  // namespace actor
