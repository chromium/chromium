// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/ui_event_debugstring.h"

#include <string>
#include <variant>

#include "base/strings/strcat.h"
#include "chrome/browser/actor/ui/variant_visitor.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace actor::ui {

namespace {
std::string_view DebugString(const MouseClickType& t) {
  switch (t) {
    case MouseClickType::kLeft:
      return "LeftClick";
    case MouseClickType::kRight:
      return "RightClick";
    default:
      NOTREACHED();
  }
}

std::string_view DebugString(const MouseClickCount& c) {
  switch (c) {
    case MouseClickCount::kSingle:
      return "SingleClick";
    case MouseClickCount::kDouble:
      return "DoubleClick";
    default:
      NOTREACHED();
  }
}

std::string DebugString(const PageTarget& t) {
  if (std::holds_alternative<gfx::Point>(t)) {
    return std::get<gfx::Point>(t).ToString();
  } else if (std::holds_alternative<DomNode>(t)) {
    const DomNode& d = std::get<DomNode>(t);
    return absl::StrFormat("DomNode[id=%d doc_id=%s]", d.node_id,
                           d.document_identifier);
  }
  NOTREACHED();
}

constexpr Visitor UiEventToDebugStringFn{
    [](const StartTask& e) -> std::string {
      return absl::StrFormat("StartTask[id=%d]", e.task_id.value());
    },
    [](const StartingToActOnTab& e) -> std::string {
      return absl::StrFormat("StartingToActOnTab[task_id=%d, tab=%d]",
                             e.task_id.value(), e.tab_handle.raw_value());
    },
    [](const MouseClick& e) -> std::string {
      return absl::StrFormat("MouseClick[type=%s, count=%s]",
                             DebugString(e.click_type),
                             DebugString(e.click_count));
    },
    [](const MouseMove& e) -> std::string {
      return absl::StrFormat("MouseMove[target=%s]", DebugString(e.target));
    },
};
}  // namespace

std::string DebugString(const UiEvent& event) {
  return std::visit(UiEventToDebugStringFn, event);
}

}  // namespace actor::ui
