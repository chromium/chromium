// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_EVENT_PAGE_REQUEST_MANAGER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_EVENT_PAGE_REQUEST_MANAGER_H_

#include <string>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_metrics.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {
class ProcessManager;
}

namespace media_router {

// A class that is responsible for running closures that are requests to the
// Media Router component extension while the extension is awake, or queueing
// requests and waking the extension up if it's suspended.
class EventPageRequestManager : public KeyedService {
 public:
  ~EventPageRequestManager() override;

  // KeyedService:
  void Shutdown() override;

  // Sets the ID of the component extension.
  virtual void SetExtensionId(const std::string& extension_id);

  // Runs a closure if the Mojo connections to the extensions are valid, or
  // defers the execution until the connections have been established. If this
  // call resulted in waking the extension, |wake_reason| is recorded as the
  // reason.
  virtual void RunOrDefer(base::OnceClosure request,
                          MediaRouteProviderWakeReason wake_reason);

  // Executes the Mojo requests queued in |pending_requests_|.
  virtual void OnMojoConnectionsReady();

  // Attempts to wake the component extension again if there are pending
  // requests. Otherwise the extension will be woken up the next time a request
  // is received.
  virtual void OnMojoConnectionError();

  content::WebContents* GetEventPageWebContents();

  const std::string& media_route_provider_extension_id() const {
    return media_route_provider_extension_id_;
  }

  bool mojo_connections_ready() const { return mojo_connections_ready_; }

  void set_mojo_connections_ready_for_test(bool ready) {
    mojo_connections_ready_ = ready;
  }

 protected:
  explicit EventPageRequestManager(content::BrowserContext* context);

 private:
  friend class EventPageRequestManagerFactory;
  friend class EventPageRequestManagerTest;
  FRIEND_TEST_ALL_PREFIXES(EventPageRequestManagerTest,
                           DropOldestPendingRequest);
  FRIEND_TEST_ALL_PREFIXES(EventPageRequestManagerTest,
                           TooManyWakeupAttemptsDrainsQueue);

  // Max consecutive attempts to wake up the component extension before
  // giving up and draining the pending request queue.
  static const int kMaxWakeupAttemptCount = 3;

  // The max number of pending requests allowed. When number of pending requests
  // exceeds this number, the oldest request will be dropped.
  static const int kMaxPendingRequests = 30;

  // Enqueues a request to be executed when the Mojo connections to the
  // component extension are ready.
  void EnqueueRequest(base::OnceClosure request);

  // Drops all pending requests. Called when we have a connection error to
  // component extension and further reattempts are unlikely to help.
  void DrainPendingRequests();

  // Sets the reason why we are attempting to wake the extension.  Since
  // multiple tasks may be enqueued for execution each time the extension runs,
  // we record the first such reason.
  virtual void SetWakeReason(MediaRouteProviderWakeReason reason);

  // Whether the component extension event page is suspended.
  bool IsEventPageSuspended() const;

  // Calls to |event_page_tracker_| to wake the component extension.
  // |media_route_provider_extension_id_| must not be empty and the extension
  // should be currently suspended. If there have already been too many wakeup
  // attempts, give up and drain the pending request queue.
  void AttemptWakeEventPage();

  // Callback invoked by |event_page_tracker_| after an attempt to wake the
  // component extension. If |success| is false, the pending request queue is
  // drained.
  void OnWakeComplete(bool success);

  // Clears the wake reason after the extension has been awoken.
  void ClearWakeReason();

  // ID of the component extension. Used for managing its suspend/wake state
  // via |event_page_tracker_|.
  std::string media_route_provider_extension_id_;

  // Pending requests queued to be executed once component extension
  // becomes ready.
  base::circular_deque<base::OnceClosure> pending_requests_;

  // Allows the extension to be monitored for suspend, and woken.
  // This is a reference to a BrowserContext keyed service that outlives this
  // instance.
  extensions::ProcessManager* extension_process_manager_;

  int wakeup_attempt_count_ = 0;

  bool mojo_connections_ready_ = false;

  // Records the current reason the extension is being woken up.  Is set to
  // MediaRouteProviderWakeReason::TOTAL_COUNT if there is no pending reason.
  MediaRouteProviderWakeReason current_wake_reason_ =
      MediaRouteProviderWakeReason::TOTAL_COUNT;

  base::WeakPtrFactory<EventPageRequestManager> weak_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_EVENT_PAGE_REQUEST_MANAGER_H_
