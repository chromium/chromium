// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tab_management_tool.h"

#include "base/notimplemented.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page_navigator.h"
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
  PostResponseTask(std::move(callback), MakeOkResult());
}

void TabManagementTool::Invoke(ToolCallback callback) {
  callback_ = std::move(callback);

  // TODO(crbug.com/445993857): Only the create action is hooked up and
  // implemented.
  switch (action_) {
    case kCreate: {
      CHECK(window_id_.has_value());
      CHECK(create_disposition_.has_value());
      BrowserWindowInterface* browser_window_interface =
          BrowserWindowInterface::FromSessionID(
              SessionID::FromSerializedValue(window_id_.value()));
      if (!browser_window_interface) {
        PostResponseTask(std::move(callback_),
                         MakeResult(mojom::ActionResultCode::kWindowWentAway));
        return;
      }

      // The observer is removed in the TabStripModelObserver's destructor.
      browser_window_interface->GetTabStripModel()->AddObserver(this);

      // Watch for the window going away as well so we don't wait indefinitely.
      browser_did_close_subscription_ =
          browser_window_interface->RegisterBrowserDidClose(base::BindRepeating(
              &TabManagementTool::OnBrowserDidClose, base::Unretained(this)));

      // Open a blank tab.
      browser_window_interface->OpenGURL(GURL(url::kAboutBlankURL),
                                         create_disposition_.value());
      break;
    }
    case kActivate:
    case kClose:
      CHECK(target_tab_.has_value());
      NOTIMPLEMENTED() << "ActivateTab and CloseTab not yet implemented";
      PostResponseTask(std::move(callback_),
                       MakeResult(mojom::ActionResultCode::kError));
      return;
  }
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

void TabManagementTool::UpdateTaskAfterInvoke(ActorTask& task,
                                              mojom::ActionResultPtr result,
                                              ToolCallback callback) const {
  if (action_ == kCreate && target_tab_) {
    task.AddTab(*target_tab_, std::move(callback));
  } else {
    std::move(callback).Run(std::move(result));
  }
}

tabs::TabHandle TabManagementTool::GetTargetTab() const {
  return target_tab_.value_or(tabs::TabHandle::Null());
}

void TabManagementTool::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (action_ != kCreate) {
    return;
  }

  if (change.type() == TabStripModelChange::kInserted) {
    if (callback_) {
      CHECK_GT(change.GetInsert()->contents.size(), 0ul);
      target_tab_ = change.GetInsert()->contents[0].tab->GetHandle();
      PostResponseTask(std::move(callback_), MakeOkResult());
    }
  }
}

void TabManagementTool::OnBrowserDidClose(BrowserWindowInterface* browser) {
  // If the window is destroyed in the interval after a create tab has been
  // invoked but before the tab's been added, this ensures we don't hang waiting
  // for the new tab.
  CHECK(window_id_);
  if (action_ == kCreate && callback_) {
    PostResponseTask(std::move(callback_),
                     MakeResult(mojom::ActionResultCode::kWindowWentAway));
  }
}

}  // namespace actor
