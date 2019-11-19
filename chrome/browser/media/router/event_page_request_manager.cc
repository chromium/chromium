// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/event_page_request_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/logging.h"
#include "base/macros.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"

namespace media_router {

EventPageRequestManager::~EventPageRequestManager() = default;

void EventPageRequestManager::Shutdown() {
  extension_process_manager_ = nullptr;
}

void EventPageRequestManager::SetExtensionId(const std::string& extension_id) {
  media_route_provider_extension_id_ = extension_id;
}

void EventPageRequestManager::RunOrDefer(
    base::OnceClosure request,
    MediaRouteProviderWakeReason wake_reason) {
  if (mojo_connections_ready_) {
    DCHECK(!media_route_provider_extension_id_.empty());
    std::move(request).Run();
  } else {
    EnqueueRequest(std::move(request));
    if (IsEventPageSuspended()) {
      SetWakeReason(wake_reason);
      AttemptWakeEventPage();
    }
  }
}

void EventPageRequestManager::OnMojoConnectionsReady() {
  if (IsEventPageSuspended()) {
    DVLOG(1)
        << "OnMojoConnectionsReady was called while extension is suspended.";
    SetWakeReason(MediaRouteProviderWakeReason::REGISTER_MEDIA_ROUTE_PROVIDER);
    AttemptWakeEventPage();
    return;
  }

  mojo_connections_ready_ = true;

  base::circular_deque<base::OnceClosure> requests;
  requests.swap(pending_requests_);
  for (base::OnceClosure& request : requests) {
    DCHECK(mojo_connections_ready_);
    // The requests should not queue additional requests when executed.
    std::move(request).Run();
  }
  DCHECK(pending_requests_.empty());
  wakeup_attempt_count_ = 0;
}

void EventPageRequestManager::OnMojoConnectionError() {
  mojo_connections_ready_ = false;

  // If this method is invoked while there are pending requests, then
  // it means we tried to wake the extension, but weren't able to complete the
  // connection to media route provider. Since we do not know whether the error
  // is transient, reattempt the wakeup.
  if (!pending_requests_.empty()) {
    DLOG(ERROR) << "A connection error while there are pending requests.";
    SetWakeReason(MediaRouteProviderWakeReason::CONNECTION_ERROR);
    AttemptWakeEventPage();
  }
}

content::WebContents* EventPageRequestManager::GetEventPageWebContents() {
  if (!extension_process_manager_)
    return nullptr;

  extensions::ExtensionHost* extension_host =
      extension_process_manager_->GetBackgroundHostForExtension(
          media_route_provider_extension_id_);
  if (!extension_host)
    return nullptr;

  return extension_host->host_contents();
}

EventPageRequestManager::EventPageRequestManager(
    content::BrowserContext* context)
    : extension_process_manager_(extensions::ProcessManager::Get(context)) {}

void EventPageRequestManager::EnqueueRequest(base::OnceClosure request) {
  pending_requests_.push_back(std::move(request));
  if (pending_requests_.size() > kMaxPendingRequests) {
    DLOG(ERROR) << "Reached max queue size. Dropping oldest request.";
    pending_requests_.pop_front();
  }
  DVLOG(2) << "EnqueueRequest (queue-length=" << pending_requests_.size()
           << ")";
}

void EventPageRequestManager::DrainPendingRequests() {
  DLOG(ERROR) << "Draining request queue. (queue-length="
              << pending_requests_.size() << ")";
  pending_requests_.clear();
}

void EventPageRequestManager::SetWakeReason(
    MediaRouteProviderWakeReason reason) {
  DCHECK(reason != MediaRouteProviderWakeReason::TOTAL_COUNT);
  if (current_wake_reason_ == MediaRouteProviderWakeReason::TOTAL_COUNT)
    current_wake_reason_ = reason;
}

bool EventPageRequestManager::IsEventPageSuspended() const {
  return !extension_process_manager_ ||
         extension_process_manager_->IsEventPageSuspended(
             media_route_provider_extension_id_);
}

void EventPageRequestManager::AttemptWakeEventPage() {
  ++wakeup_attempt_count_;
  if (wakeup_attempt_count_ > kMaxWakeupAttemptCount) {
    DLOG(ERROR) << "Attempted too many times to wake up event page.";
    DrainPendingRequests();
    wakeup_attempt_count_ = 0;
    MediaRouterMojoMetrics::RecordMediaRouteProviderWakeup(
        MediaRouteProviderWakeup::ERROR_TOO_MANY_RETRIES);
    return;
  }

  DVLOG(1) << "Attempting to wake up event page: attempt "
           << wakeup_attempt_count_;
  if (!extension_process_manager_) {
    DLOG(ERROR) << "Attempted to wake up event page without a valid event page"
                   "tracker";
    return;
  }

  // This return false if the extension is already awake.
  // Callback is bound using WeakPtr because |extension_process_manager_|
  // outlives |this|.
  if (!extension_process_manager_->WakeEventPage(
          media_route_provider_extension_id_,
          base::BindOnce(&EventPageRequestManager::OnWakeComplete,
                         weak_factory_.GetWeakPtr()))) {
    DLOG(ERROR) << "Failed to schedule a wakeup for event page.";
  }
}

void EventPageRequestManager::OnWakeComplete(bool success) {
  // If there are multiple overlapping WakeEventPage requests, ensure the
  // metrics are only recorded once.
  if (current_wake_reason_ == MediaRouteProviderWakeReason::TOTAL_COUNT)
    return;

  if (success) {
    MediaRouterMojoMetrics::RecordMediaRouteProviderWakeReason(
        current_wake_reason_);
    ClearWakeReason();
    MediaRouterMojoMetrics::RecordMediaRouteProviderWakeup(
        MediaRouteProviderWakeup::SUCCESS);
    return;
  }

  // This is likely an non-retriable error. Drop the pending requests.
  DLOG(ERROR) << "An error encountered while waking the event page.";
  ClearWakeReason();
  DrainPendingRequests();
  MediaRouterMojoMetrics::RecordMediaRouteProviderWakeup(
      MediaRouteProviderWakeup::ERROR_UNKNOWN);
}

void EventPageRequestManager::ClearWakeReason() {
  DCHECK(current_wake_reason_ != MediaRouteProviderWakeReason::TOTAL_COUNT);
  current_wake_reason_ = MediaRouteProviderWakeReason::TOTAL_COUNT;
}

}  // namespace media_router
