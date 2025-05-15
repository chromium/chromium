// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/history_tool.h"

#include "base/time/time.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace {

// The polling interval used to update the pending_navigations_ list.
constexpr base::TimeDelta kPendingNavigationPollingInterval =
    base::Milliseconds(100);

}  // namespace

namespace actor {

using ::content::NavigationController;
using ::content::NavigationHandle;
using ::tabs::TabInterface;

HistoryTool::HistoryTool(TabInterface& tab, Direction direction)
    : WebContentsObserver(tab.GetContents()), direction_(direction) {
  CHECK(tab.GetContents());
}

HistoryTool::~HistoryTool() = default;

void HistoryTool::Validate(ValidateCallback callback) {
  NavigationController& controller = web_contents()->GetController();
  mojom::ActionResultPtr result;

  if (direction_ == kBack && !controller.CanGoBack()) {
    result = MakeResult(mojom::ActionResultCode::kHistoryNoBackEntries);
  } else if (direction_ == kForward && !controller.CanGoForward()) {
    result = MakeResult(mojom::ActionResultCode::kHistoryNoForwardEntries);
  } else {
    result = MakeOkResult();
  }

  // TODO(crbug.com/402731599): Additional validation here (e.g. is URL in
  // allowlist).

  PostResponseTask(std::move(callback), std::move(result));
}

void HistoryTool::Invoke(InvokeCallback callback) {
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

  if (direction_ == kBack) {
    pending_navigations_ = web_contents()->GetController().GoBack();
  } else {
    CHECK_EQ(direction_, kForward);
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
  return absl::StrFormat("HistoryTool[%s]",
                         direction_ == kBack ? "Back" : "Forward");
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
                          details_msg(navigation_handle));
    } else if (navigation_handle->IsErrorPage()) {
      result = MakeResult(mojom::ActionResultCode::kHistoryErrorPage,
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
