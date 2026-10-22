// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tab_management_tool.h"

#include "base/notimplemented.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace actor {

using ::tabs::TabHandle;

TabManagementTool::TabManagementTool(TaskId task_id,
                                     ToolDelegate& tool_delegate,
                                     int32_t window_id,
                                     WindowOpenDisposition create_disposition)
    : Tool(task_id, tool_delegate),
      action_(Action::kCreate),
      create_disposition_(create_disposition),
      window_id_(window_id) {}

TabManagementTool::TabManagementTool(TaskId task_id,
                                     ToolDelegate& tool_delegate,
                                     Action action,
                                     TabHandle tab_handle)
    : Tool(task_id, tool_delegate), action_(action), target_tab_(tab_handle) {}

TabManagementTool::~TabManagementTool() = default;

void TabManagementTool::Validate(ToolCallback callback) {
  if (action_ == kCreate) {
    if (window_id_.has_value()) {
      PostResponseTask(
          std::move(callback),
          ValidateWindowId(SessionID::FromSerializedValue(window_id_.value())));
      return;
    }
  }
  PostResponseTask(std::move(callback), MakeOkResult());
}

void TabManagementTool::Invoke(ToolCallback callback) {
  callback_ = std::move(callback);
  CHECK(window_id_.has_value() || target_tab_.has_value());

  BrowserWindowInterface* browser_window_interface =
      window_id_.has_value()
          ? FindBrowserWindow(
                SessionID::FromSerializedValue(window_id_.value()))
          : target_tab_.value().Get()->GetBrowserWindowInterface();

  mojom::ActionResultPtr window_result =
      ValidateBrowserWindow(browser_window_interface);
  if (!IsOk(*window_result)) {
    PostResponseTask(std::move(callback_), std::move(window_result));
    return;
  }

  TabListInterface* tab_list = TabListInterface::From(browser_window_interface);
  if (!tab_list) {
    PostResponseTask(std::move(callback_),
                     MakeResult(mojom::ActionResultCode::kWindowWentAway));
    return;
  }

  tab_list_observation_.Observe(tab_list);

  // Watch for the window going away as well so we don't wait indefinitely.
  browser_did_close_subscription_ =
      browser_window_interface->RegisterBrowserDidClose(base::BindRepeating(
          &TabManagementTool::OnBrowserDidClose, base::Unretained(this)));

  // Each of these functions result in a `TabListInterfaceObserver` call if they
  // complete successfully. Otherwise, they will fire `callback_` early and
  // return.
  switch (action_) {
    case kCreate:
      CreateTab(tab_list);
      break;
    case kActivate:
      ActivateTab(tab_list);
      break;
    case kClose:
      CloseTab();
      break;
  }
}

void TabManagementTool::CreateTab(TabListInterface* tab_list) {
  CHECK(window_id_.has_value());
  CHECK(create_disposition_.has_value());
  // Open a blank tab.
  tab_list->OpenTab(
      GURL(url::kAboutBlankURL), /*index=*/-1,
      create_disposition_.value() == WindowOpenDisposition::kNewForegroundTab);
}

void TabManagementTool::ActivateTab(TabListInterface* tab_list) {
  CHECK(target_tab_.has_value());
  // The tab might already be activated. In which case, treat that as a
  // successful tool invocation.
  if (target_tab_->Get()->IsActivated()) {
    PostResponseTask(std::move(callback_), MakeOkResult());
    return;
  }

  tab_list->ActivateTab(*target_tab_);
}

void TabManagementTool::CloseTab() {
  CHECK(target_tab_.has_value());
  target_tab_->Get()->Close();
}

std::string TabManagementTool::DebugString() const {
  return absl::StrFormat("TabManagementTool:%s", JournalEvent().c_str());
}

std::string TabManagementTool::JournalEvent() const {
  switch (action_) {
    case kCreate:
      return "CreateTab";
    case kActivate:
      return "ActivateTab";
    case kClose:
      return "CloseTab";
  }
}

std::unique_ptr<ObservationDelayController>
TabManagementTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  if (action_ != kCreate) {
    return nullptr;
  }

  return std::make_unique<ObservationDelayController>(task_id(), journal());
}

void TabManagementTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                               ToolCallback callback) const {
  if (action_ == kClose) {
    CHECK(target_tab_.has_value());
    task.RemoveTab(*target_tab_);
  }
  std::move(callback).Run(MakeOkResult());
}

void TabManagementTool::UpdateTaskAfterInvoke(ActorTask& task,
                                              mojom::ActionResultPtr result,
                                              ToolCallback callback) const {
  if (action_ == kCreate && target_tab_) {
    task.AddTab(*target_tab_, /*stop_task_on_detach=*/true,
                std::move(callback));
  } else {
    std::move(callback).Run(std::move(result));
  }
}

ActorSurfaceHandle TabManagementTool::GetTargetActorSurface() const {
  return target_tab_.has_value() ? ActorSurfaceHandle::From(*target_tab_)
                                 : ActorSurfaceHandle::Null();
}

void TabManagementTool::OnTabAdded(TabListInterface& tab_list,
                                   tabs::TabInterface* tab,
                                   int index) {
  if (!callback_ || action_ != kCreate || !tab) {
    return;
  }

  target_tab_ = tab->GetHandle();
  PostResponseTask(std::move(callback_), MakeOkResult());
}

void TabManagementTool::OnActiveTabChanged(TabListInterface& tab_list,
                                           tabs::TabInterface* tab) {
  if (!callback_ || action_ != kActivate || !tab) {
    return;
  }

  if (*target_tab_ == tab->GetHandle()) {
    PostResponseTask(std::move(callback_), MakeOkResult());
  }
}

void TabManagementTool::OnTabRemoved(TabListInterface& tab_list,
                                     tabs::TabInterface* tab,
                                     TabRemovedReason removed_reason) {
  // This fires once per removed tab, so a single handle comparison covers
  // every case that matters here: a removal of some other tab (e.g. the user
  // closing an unrelated tab while we wait for `target_tab_` to close or
  // activate) is ignored, and `target_tab_` is unset for kCreate until
  // OnTabAdded() runs, so removals during tab creation are ignored too.
  if (!callback_ || !tab || !target_tab_.has_value() ||
      *target_tab_ != tab->GetHandle()) {
    return;
  }

  // The tab we are waiting to activate closed unexpectedly.
  if (action_ == kActivate) {
    PostResponseTask(std::move(callback_),
                     MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  // Handle tab removal when closing a tab.
  if (action_ == kClose) {
    // Our single tab should have been deleted, rather than moved elsewhere.
    if (!TabRemoveReasonUtils::WillDeleteTab(removed_reason)) {
      PostResponseTask(std::move(callback_),
                       MakeResult(mojom::ActionResultCode::kTabWentAway));
      return;
    }

    PostResponseTask(std::move(callback_), MakeOkResult());
    return;
  }
}

void TabManagementTool::OnTabListDestroyed(TabListInterface& tab_list) {
  tab_list_observation_.Reset();
}

void TabManagementTool::OnBrowserDidClose(BrowserWindowInterface* browser) {
  // If the window is destroyed in the interval after a tab action has been
  // invoked but before the action is completed, this ensures we don't hang
  // waiting for the action.
  if (callback_) {
    PostResponseTask(std::move(callback_),
                     MakeResult(mojom::ActionResultCode::kWindowWentAway));
  }
}

}  // namespace actor
