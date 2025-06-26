// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/event_dispatcher.h"

#include <variant>

#include "base/logging.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/helpers.h"
#include "chrome/browser/actor/ui/tool_request_variant.h"
#include "chrome/common/actor/action_result.h"

namespace actor::ui {

// TODO(crbug.com/425784083): This is just a placeholder (that's redundant with
// tool_request.JournalEvent()).  Remove it and replace with functors that
// perform type translation for OnPreTool and OnPostTool.
constexpr Visitor RequestTypeNameFn{
    [](const ClickToolRequest& tr) { return "ClickToolRequest"; },
    [](const ActivateTabToolRequest& tr) { return "ActivateTabToolRequest"; },
    [](const CloseTabToolRequest& tr) { return "CloseTabToolRequest"; },
    [](const CreateTabToolRequest& tr) { return "CreateTabToolRequest"; },
    [](const DragAndReleaseToolRequest& tr) {
      return "DragAndReleaseToolRequest";
    },
    [](const HistoryToolRequest& tr) { return "HistoryToolRequest"; },
    [](const MoveMouseToolRequest& tr) { return "MoveMouseToolRequest"; },
    [](const NavigateToolRequest& tr) { return "NavigateToolRequest"; },
    [](const ScrollToolRequest& tr) { return "ScrollToolRequest"; },
    [](const SelectToolRequest& tr) { return "SelectToolRequest"; },
    [](const TypeToolRequest& tr) { return "TypeToolRequest"; },
    [](const WaitToolRequest& tr) { return "WaitToolRequest"; }};

namespace {

class UiEventDispatcherImpl : public UiEventDispatcher {
 public:
  UiEventDispatcherImpl() = default;
  ~UiEventDispatcherImpl() override = default;

  void OnPreTool(Profile* profile,
                 const ToolRequest& tool_request,
                 UiCompleteCallback callback) override {
    // TODO(crbug.com/425784083): Translate ToolRequest into a sequence of
    // UiEvent objects and call into ActorUiStateManager.
    ConvertToVariantFn fn;
    tool_request.Apply(fn);
    VLOG(4) << "OnPreTool(ToolRequest): type="
            << std::visit(RequestTypeNameFn, fn.GetVariant().value());

    std::move(callback).Run(MakeOkResult());
  }

  void OnPostTool(Profile* profile,
                  const ToolRequest& tool_request,
                  UiCompleteCallback callback) override {
    // TODO(crbug.com/425784083): Translate ToolRequest into a sequence of
    // UiEvent objects and call into ActorUiStateManager.
    ConvertToVariantFn fn;
    tool_request.Apply(fn);
    VLOG(4) << "OnPostTool(ToolRequest): type="
            << std::visit(RequestTypeNameFn, fn.GetVariant().value());

    std::move(callback).Run(MakeOkResult());
  }
};
}  // namespace

std::unique_ptr<UiEventDispatcher> NewUiEventDispatcher() {
  return std::make_unique<UiEventDispatcherImpl>();
}

}  // namespace actor::ui
