// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/load_and_extract_content_tool.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/validate_url_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace actor {

LoadAndExtractContentTool::LoadAndExtractContentTool(
    TaskId task_id,
    ToolDelegate& tool_delegate,
    SessionID window_id,
    std::vector<GURL> urls)
    : Tool(task_id, tool_delegate),
      urls_(std::move(urls)),
      window_id_(window_id) {
  per_url_completion_callback_ = base::BarrierCallback<mojom::ActionResultPtr>(
      urls_.size(),
      base::BindOnce(&LoadAndExtractContentTool::OnAllUrlsCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

LoadAndExtractContentTool::~LoadAndExtractContentTool() = default;

void LoadAndExtractContentTool::Validate(ToolCallback callback) {
  // TODO(b/478281513): Validate the URLs, using the same logic as the
  // NavigateTool.
  PostResponseTask(std::move(callback), MakeOkResult());
}

void LoadAndExtractContentTool::Invoke(ToolCallback callback) {
  CHECK(!invoke_callback_);
  invoke_callback_ = std::move(callback);

  BrowserWindowInterface* browser_window_interface =
      BrowserWindowInterface::FromSessionID(window_id_);

  if (!browser_window_interface) {
    PostResponseTask(std::move(invoke_callback_),
                     MakeResult(mojom::ActionResultCode::kWindowWentAway));
    return;
  }

  // Watch for the window going away as well so we don't wait indefinitely.
  browser_did_close_subscription_ =
      browser_window_interface->RegisterBrowserDidClose(
          base::BindRepeating(&LoadAndExtractContentTool::OnBrowserDidClose,
                              weak_ptr_factory_.GetWeakPtr()));

  for (const GURL& url : urls_) {
    constexpr int kIndexAppendToEnd = -1;
    content::WebContents* web_contents = chrome::AddAndReturnTabAt(
        browser_window_interface->GetBrowserForMigrationOnly(), url,
        kIndexAppendToEnd, /*foreground=*/false);
    if (!web_contents) {
      per_url_completion_callback_.Run(
          MakeResult(mojom::ActionResultCode::kNewTabCreationFailed));
      continue;
    }
    tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
    tabs_created_.push_back(tab->GetHandle());

    // TODO(b/478282022): Implement waiting for the tab navigation and
    // collecting the APC. Temporarily, this implementation just closes the tab.
    PostResponseTask(
        base::BindOnce(&LoadAndExtractContentTool::OnTabReadyToClose,
                       weak_ptr_factory_.GetWeakPtr(),
                       tabs_created_.size() - 1),
        MakeOkResult());
  }
}

void LoadAndExtractContentTool::OnTabReadyToClose(
    size_t index,
    mojom::ActionResultPtr result) {
  tabs::TabHandle tab_handle = tabs_created_[index];

  if (tab_handle == tabs::TabHandle::Null()) {
    // TODO(b/478282022): Don't throw this error if APC collection successfully
    // occurred already.
    result = MakeResult(mojom::ActionResultCode::kTabWentAway);

    per_url_completion_callback_.Run(std::move(result));
    return;
  }

  tab_handle.Get()->Close();

  per_url_completion_callback_.Run(std::move(result));
}

void LoadAndExtractContentTool::OnAllUrlsCompleted(
    std::vector<mojom::ActionResultPtr> results) {
  if (!invoke_callback_) {
    return;
  }

  for (mojom::ActionResultPtr& result : results) {
    // In the case of multiple errors, we arbitrarily return the first.
    if (!IsOk(result->code)) {
      PostResponseTask(std::move(invoke_callback_), std::move(result));
      return;
    }
  }

  // TODO(b/478282022): Plumb the APC results back to the caller.
  PostResponseTask(std::move(invoke_callback_), MakeOkResult());
}

void LoadAndExtractContentTool::OnBrowserDidClose(
    BrowserWindowInterface* browser) {
  // If the window is destroyed while the action is ongoing, this ensures we
  // don't hang waiting for the action.
  if (invoke_callback_) {
    PostResponseTask(std::move(invoke_callback_),
                     MakeResult(mojom::ActionResultCode::kWindowWentAway));
  }
}

std::string LoadAndExtractContentTool::DebugString() const {
  return "LoadAndExtractContentTool";
}

std::string LoadAndExtractContentTool::JournalEvent() const {
  return "LoadAndExtractContent";
}

std::unique_ptr<ObservationDelayController>
LoadAndExtractContentTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  // TODO(b/478281781): Create an observation delayer that waits for the pages
  // to load.
  return nullptr;
}

tabs::TabHandle LoadAndExtractContentTool::GetTargetTab() const {
  // This tool can operate on multiple tabs, so there's no single target.
  return tabs::TabHandle::Null();
}

}  // namespace actor
