// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/history_tool.h"

#include "base/time/time.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace actor {

namespace {

// The polling interval used to update the pending_navigations_ list.
constexpr base::TimeDelta kPendingNavigationPollingInterval =
    base::Milliseconds(100);

mojom::ActionResultPtr UrlCheckToActionResult(MayActOnUrlBlockReason reason) {
  return reason == MayActOnUrlBlockReason::kAllowed
             ? MakeOkResult()
             : MakeResult(mojom::ActionResultCode::kUrlBlocked);
}

}  // namespace

using ::content::NavigationController;
using ::content::NavigationHandle;
using ::tabs::TabHandle;
using ::tabs::TabInterface;

HistoryTool::HistoryTool(TaskId task_id,
                         ToolDelegate& tool_delegate,
                         TabInterface& tab,
                         HistoryToolRequest::Direction direction)
    : Tool(task_id, tool_delegate),
      WebContentsObserver(tab.GetContents()),
      direction_(direction),
      tab_handle_(tab.GetHandle()) {}

HistoryTool::~HistoryTool() = default;

void HistoryTool::Validate(ToolCallback callback) {
  // Get the navigation entry that would be navigated to.
  int offset = direction_ == HistoryToolRequest::Direction::kBack ? -1 : 1;
  content::NavigationEntry* entry =
      web_contents()->GetController().GetEntryAtOffset(offset);

  // If there is no entry, the navigation will fail at the time of use, so
  // we can pass validation for now.
  if (!entry) {
    PostResponseTask(std::move(callback), MakeOkResult());
    return;
  }

  validated_entry_id_ = entry->GetUniqueID();

  // Check if the destination URL is blocked.
  tool_delegate().IsAcceptableNavigationDestination(
      entry->GetURL(),
      base::BindOnce(&UrlCheckToActionResult).Then(std::move(callback)));
}

mojom::ActionResultPtr HistoryTool::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation) {
  NavigationController& controller = web_contents()->GetController();
  mojom::ActionResultPtr result;

  if (direction_ == HistoryToolRequest::Direction::kBack &&
      !controller.CanGoBack()) {
    result = MakeResult(mojom::ActionResultCode::kHistoryNoBackEntries);
  } else if (direction_ == HistoryToolRequest::Direction::kForward &&
             !controller.CanGoForward()) {
    result = MakeResult(mojom::ActionResultCode::kHistoryNoForwardEntries);
  } else {
    // Ensure the entry being navigated to is the same as when it was
    // validated.
    int offset = direction_ == HistoryToolRequest::Direction::kBack ? -1 : 1;
    content::NavigationEntry* entry =
        web_contents()->GetController().GetEntryAtOffset(offset);
    if (!entry || entry->GetUniqueID() != validated_entry_id_) {
      result =
          MakeResult(mojom::ActionResultCode::kHistoryNavigationEntryChanged);
    } else {
      result = MakeOkResult();
    }
  }

  return result;
}

void HistoryTool::Invoke(ToolCallback callback) {
  CHECK(web_contents());
  CHECK(!IsInvokeInProgress());
  CHECK(pending_navigations_.empty());

  invoke_callback_ = std::move(callback);

  CHECK(IsInvokeInProgress());

  // TODO(crbug.com/417521502): A navigation may need to send a BeforeUnload
  // event which could result in a modal dialog being presented the the user and
  // the navigation is deferred until this dialog is confirmed (navigation
  // proceeds) or canceled. The current approach here will wait until the dialog
  // is manually dismissed by the user but we may want to provide automatic
  // resolution here.

  if (direction_ == HistoryToolRequest::Direction::kBack) {
    pending_navigations_ = web_contents()->GetController().GoBack();
  } else {
    CHECK_EQ(direction_, HistoryToolRequest::Direction::kForward);
    pending_navigations_ = web_contents()->GetController().GoForward();
  }

  if (pending_navigations_.empty()) {
    PostResponseTask(
        std::move(invoke_callback_),
        MakeResult(mojom::ActionResultCode::kHistoryNoNavigationsCreated));
    return;
  }

  // Ensure navigations that were started synchronously are moved to the
  // in-flight list and start polling for navigation cancellation.
  PurgePendingNavigations();
}

std::string HistoryTool::DebugString() const {
  return absl::StrFormat("HistoryTool[%s]", JournalEvent());
}

std::string HistoryTool::JournalEvent() const {
  return direction_ == HistoryToolRequest::Direction::kBack ? "Back"
                                                            : "Forward";
}

std::unique_ptr<ObservationDelayController> HistoryTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  return std::make_unique<ObservationDelayController>(
      *web_contents()->GetPrimaryMainFrame(), task_id(), journal(),
      page_stability_config);
}

void HistoryTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                         ToolCallback callback) const {
  task.AddTab(tab_handle_, std::move(callback));
}

tabs::TabHandle HistoryTool::GetTargetTab() const {
  return tab_handle_;
}

void HistoryTool::DidStartNavigation(NavigationHandle* navigation_handle) {
  if (!IsInvokeInProgress() || !navigation_handle->IsHistory()) {
    return;
  }

  size_t matching_navigations = std::erase_if(
      pending_navigations_,
      [navigation_handle](const base::WeakPtr<NavigationHandle>& other) {
        return other &&
               navigation_handle->GetNavigationId() == other->GetNavigationId();
      });
  CHECK_LE(matching_navigations, 1ul);

  // Navigations can sometimes be started synchronously from GoBack/GoForward
  // which means this point will be reached before pending_navigations_ is
  // written (since it's written when GoBack/GoForward return) so add them
  // unconditionally in that case. Invoke calls PurgePendingNavigations which
  // will clear these entries from `pending_navigations_`. This only catches
  // synchronously started navigations since Invoke will return failure
  // immediately if no navigations were created.
  if (pending_navigations_.empty() || matching_navigations > 0) {
    in_flight_navigation_ids_.insert(navigation_handle->GetNavigationId());
  }
}

void HistoryTool::DidFinishNavigation(NavigationHandle* navigation_handle) {
  if (!IsInvokeInProgress()) {
    return;
  }

  if (in_flight_navigation_ids_.erase(navigation_handle->GetNavigationId())) {
    mojom::ActionResultPtr result;
    auto details_msg = [](NavigationHandle* handle) {
      std::string msg;
      if (handle->GetNavigationDiscardReason()) {
        msg = absl::StrFormat("DiscardReason[%d] ",
                              handle->GetNavigationDiscardReason().value());
      }
      if (handle->GetNetErrorCode() != net::OK) {
        msg +=
            absl::StrFormat("ErrorCode[%s]",
                            net::ErrorToShortString(handle->GetNetErrorCode()));
      }
      return msg;
    };

    if (!navigation_handle->HasCommitted()) {
      result = MakeResult(mojom::ActionResultCode::kHistoryFailedBeforeCommit,
                          /*requires_page_stabilization=*/false,
                          details_msg(navigation_handle));
    } else if (navigation_handle->IsErrorPage()) {
      result = MakeResult(mojom::ActionResultCode::kHistoryErrorPage,
                          /*requires_page_stabilization=*/false,
                          details_msg(navigation_handle));
    } else {
      result = MakeOkResult();
    }
    FinishToolInvocationIfNeeded(std::move(result));
  }
}

void HistoryTool::FinishToolInvocationIfNeeded(mojom::ActionResultPtr result) {
  CHECK(IsInvokeInProgress());

  // This responds with failure if any navigations fails.
  if ((in_flight_navigation_ids_.empty() && pending_navigations_.empty()) ||
      !IsOk(*result)) {
    PostResponseTask(std::move(invoke_callback_), std::move(result));
  }
}

void HistoryTool::PurgePendingNavigations() {
  if (!IsInvokeInProgress()) {
    return;
  }

  std::erase_if(
      pending_navigations_, [this](base::WeakPtr<NavigationHandle>& handle) {
        // Also remove navigations that have been started. This typically
        // happens in DidStartNavigation but navigations started synchronously
        // will happen before this list is populated.
        return !handle ||
               in_flight_navigation_ids_.contains(handle->GetNavigationId());
      });

  if (pending_navigations_.empty() && in_flight_navigation_ids_.empty()) {
    // If no navigations were started and all handles were destroyed, the tool
    // has completed without navigating.
    FinishToolInvocationIfNeeded(
        MakeResult(mojom::ActionResultCode::kHistoryCancelledBeforeStart));
  } else if (!pending_navigations_.empty()) {
    // If there are still unstarted navigations, poll this method again.
    // TODO(crbug.com/417756996): Ideally the content API would have a signal
    // for when a navigation was canceled before starting so we wouldn't have to
    // poll.
    content::GetUIThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&HistoryTool::PurgePendingNavigations,
                       weak_ptr_factory_.GetWeakPtr()),
        kPendingNavigationPollingInterval);
  }
}

bool HistoryTool::IsInvokeInProgress() const {
  return !invoke_callback_.is_null();
}

}  // namespace actor
