// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/click_tool_request.h"

#include "chrome/common/actor.mojom.h"

namespace actor {

using ::tabs::TabHandle;

ClickToolRequest::ClickToolRequest(TabHandle tab_handle,
                                   const Target& target,
                                   ClickType type,
                                   ClickCount count)
    : PageToolRequest(tab_handle, target),
      click_type_(type),
      click_count_(count) {}

ClickToolRequest::~ClickToolRequest() = default;

std::string ClickToolRequest::JournalEvent() const {
  return "Click";
}

mojom::ToolActionPtr ClickToolRequest::ToMojoToolAction() const {
  auto click = mojom::ClickAction::New();

  click->target = PageToolRequest::ToMojoToolTarget(GetTarget());

  switch (click_type_) {
    case ClickType::kLeft:
      click->type = actor::mojom::ClickAction::Type::kLeft;
      break;
    case ClickType::kRight:
      click->type = actor::mojom::ClickAction::Type::kRight;
      break;
  }

  switch (click_count_) {
    case ClickCount::kSingle:
      click->count = actor::mojom::ClickAction::Count::kSingle;
      break;
    case ClickCount::kDouble:
      click->count = actor::mojom::ClickAction::Count::kDouble;
      break;
  }

  return mojom::ToolAction::NewClick(std::move(click));
}

std::unique_ptr<PageToolRequest> ClickToolRequest::Clone() const {
  return std::make_unique<ClickToolRequest>(*this);
}

}  // namespace actor
