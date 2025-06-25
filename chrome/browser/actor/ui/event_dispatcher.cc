// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/event_dispatcher.h"

#include "base/logging.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/common/actor/action_result.h"

namespace actor {
namespace {
class UiEventDispatcherImpl : public UiEventDispatcher {
 public:
  UiEventDispatcherImpl() = default;
  ~UiEventDispatcherImpl() override = default;

  void OnPreTool(Profile* profile,
                 const ToolRequest& tool_request,
                 UiCompleteCallback callback) override {
    VLOG(1) << "PreTool :: " << tool_request.JournalEvent();
    // TODO(crbug.com/425784083): Translate ToolRequest into a sequence of
    // UiEvent objects and call into ActorUiStateManager.
    std::move(callback).Run(MakeOkResult());
  }

  void OnPostTool(Profile* profile,
                  const ToolRequest& tool_request,
                  UiCompleteCallback callback) override {
    VLOG(1) << "PostTool :: " << tool_request.JournalEvent();
    // TODO(crbug.com/425784083): Translate ToolRequest into a sequence of
    // UiEvent objects and call into ActorUiStateManager.
    std::move(callback).Run(MakeOkResult());
  }
};
}  // namespace

std::unique_ptr<UiEventDispatcher> NewUiEventDispatcher() {
  return std::make_unique<UiEventDispatcherImpl>();
}

}  // namespace actor
