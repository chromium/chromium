// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/type_tool_request.h"

#include "chrome/common/actor.mojom.h"

namespace actor {

using ::tabs::TabHandle;

TypeToolRequest::TypeToolRequest(TabHandle tab_handle,
                                 const Target& target,
                                 std::string_view text,
                                 bool follow_by_enter,
                                 Mode mode)
    : PageToolRequest(tab_handle, target),
      text(text),
      follow_by_enter(follow_by_enter),
      mode(mode) {}

TypeToolRequest::~TypeToolRequest() = default;

std::string TypeToolRequest::JournalEvent() const {
  return "Type";
}

mojom::ToolActionPtr TypeToolRequest::ToMojoToolAction() const {
  auto type = mojom::TypeAction::New();

  type->target = PageToolRequest::ToMojoToolTarget(GetTarget());

  type->text = text;
  type->follow_by_enter = follow_by_enter;

  switch (mode) {
    case Mode::kReplace:
      type->mode = mojom::TypeAction::Mode::kDeleteExisting;
      break;
    case Mode::kPrepend:
      type->mode = mojom::TypeAction::Mode::kPrepend;
      break;
    case Mode::kAppend:
      type->mode = mojom::TypeAction::Mode::kAppend;
      break;
  }

  return mojom::ToolAction::NewType(std::move(type));
}

std::unique_ptr<PageToolRequest> TypeToolRequest::Clone() const {
  return std::make_unique<TypeToolRequest>(*this);
}

}  // namespace actor
