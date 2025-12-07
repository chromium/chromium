// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/ui_event_debugstring.h"

#include <string>
#include <variant>

#include "base/strings/strcat.h"
#include "chrome/browser/actor/shared_types.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace actor::ui {
namespace {

// TODO(crbug.com/388070065): Replace with std::optional::transform when C++23
// is available.
template <typename In, typename TransformT>
std::optional<std::invoke_result_t<TransformT, In>> transform(
    std::optional<In> opt,
    TransformT&& fn) {
  if (!opt.has_value()) {
    return std::nullopt;
  }
  return std::invoke(std::forward<TransformT>(fn), opt.value());
}

constexpr absl::Overload UiEventToDebugStringFn{
    [](const StartTask& e) -> std::string {
      return absl::StrFormat("StartTask[id=%d]", e.task_id.value());
    },
    [](const TaskStateChanged& e) -> std::string {
      return absl::StrFormat("TaskStateChanged[task_id=%d, state=%s]",
                             e.task_id.value(), ToString(e.state));
    },
    [](const StartingToActOnTab& e) -> std::string {
      return absl::StrFormat("StartingToActOnTab[task_id=%d, tab=%d]",
                             e.task_id.value(), e.tab_handle.raw_value());
    },
    [](const StoppedActingOnTab& e) -> std::string {
      return absl::StrFormat("StoppedActingOnTab[tab=%d]",
                             e.tab_handle.raw_value());
    },
    [](const MouseClick& e) -> std::string {
      return absl::StrFormat("MouseClick[type=%s, count=%s]",
                             actor::DebugString(e.click_type),
                             actor::DebugString(e.click_count));
    },
    [](const MouseMove& e) -> std::string {
      return absl::StrFormat(
          "MouseMove[target=%s target_source=%s]",
          transform(e.target, &gfx::Point::ToString).value_or("null"),
          DebugString(e.target_source));
    },
};
}  // namespace

std::string DebugString(const UiEvent& event) {
  return std::visit(UiEventToDebugStringFn, event);
}

std::string DebugString(const AsyncUiEvent& event) {
  return std::visit(UiEventToDebugStringFn, event);
}

std::string DebugString(const SyncUiEvent& event) {
  return std::visit(UiEventToDebugStringFn, event);
}

std::string DebugString(TargetSource source) {
  switch (source) {
    case TargetSource::kUnresolvableInApc:
      return "UnresolvableInApc";
    case TargetSource::kToolRequest:
      return "ToolRequest";
    case TargetSource::kDerivedFromApc:
      return "DerivedFromApc";
  }
}

}  // namespace actor::ui
