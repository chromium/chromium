// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tab_management_tool.h"

#include "base/functional/callback_forward.h"
#include "base/notimplemented.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
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

void TabManagementTool::Validate(ValidateCallback callback) {
  PostResponseTask(std::move(callback), MakeOkResult());
}

void TabManagementTool::Invoke(InvokeCallback callback) {
  callback_ = std::move(callback);

  // TODO(crbug.com/411462297): Only the create action is hooked up and
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
      browser_list_observation_.Observe(BrowserList::GetInstance());

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
TabManagementTool::GetObservationDelayer() const {
  return nullptr;
}

void TabManagementTool::UpdateTaskAfterInvoke(ActorTask& task,
                                              InvokeCallback callback) const {
  if (action_ == kCreate && did_create_tab_handle_) {
    task.AddTab(*did_create_tab_handle_, std::move(callback));
  } else {
    std::move(callback).Run(MakeOkResult());
  }
}

void TabManagementTool::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted) {
    if (callback_) {
      CHECK_GT(change.GetInsert()->contents.size(), 0ul);
      did_create_tab_handle_ = change.GetInsert()->contents[0].tab->GetHandle();
      PostResponseTask(std::move(callback_), MakeOkResult());
    }
  }
}

void TabManagementTool::OnBrowserRemoved(Browser* browser) {
  // If the window is destroyed in the interval after a create tab has been
  // invoked but before the tab's been added, this ensures we don't hang waiting
  // for the new tab.
  if (action_ == kCreate) {
    CHECK(window_id_);
    if (callback_ && browser->GetSessionID().id() == window_id_.value()) {
      PostResponseTask(std::move(callback_),
                       MakeResult(mojom::ActionResultCode::kWindowWentAway));
    }
  }
}

}  // namespace actor
