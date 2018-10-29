// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_MANAGER_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_MANAGER_H_

#include <unordered_map>

#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/permission_bubble/permission_prompt.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

enum class PermissionAction;
class PermissionRequest;

namespace test {
class PermissionRequestManagerTestApi;
}

// Provides access to permissions bubbles. Allows clients to add a request
// callback interface to the existing permission bubble configuration.
// Depending on the situation and policy, that may add new UI to an existing
// permission bubble, create and show a new permission bubble, or provide no
// visible UI action at all. (In that case, the request will be immediately
// informed that the permission request failed.)
//
// A PermissionRequestManager is associated with a particular WebContents.
// Requests attached to a particular WebContents' PBM must outlive it.
//
// The PermissionRequestManager should be addressed on the UI thread.
class PermissionRequestManager
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PermissionRequestManager>,
      public PermissionPrompt::Delegate {
 public:
  class Observer {
   public:
    virtual void OnBubbleAdded() {}
    virtual void OnBubbleRemoved() {}

   protected:
    virtual ~Observer() = default;
  };

  enum AutoResponseType {
    NONE,
    ACCEPT_ALL,
    DENY_ALL,
    DISMISS
  };

  ~PermissionRequestManager() override;

  // Adds a new request to the permission bubble. Ownership of the request
  // remains with the caller. The caller must arrange for the request to
  // outlive the PermissionRequestManager. If a bubble is visible when this
  // call is made, the request will be queued up and shown after the current
  // bubble closes. A request with message text identical to an outstanding
  // request will be merged with the outstanding request, and will have the same
  // callbacks called as the outstanding request.
  void AddRequest(PermissionRequest* request);

  // Will reposition the bubble (may change parent if necessary).
  void UpdateAnchorPosition();

  // True if a permission bubble is currently visible.
  // TODO(hcarmona): Remove this as part of the bubble API work.
  bool IsBubbleVisible();

  // Get the native window of the bubble.
  // TODO(hcarmona): Remove this as part of the bubble API work.
  gfx::NativeWindow GetBubbleWindow();

  // For observing the status of the permission bubble manager.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Do NOT use this methods in production code. Use this methods in browser
  // tests that need to accept or deny permissions when requested in
  // JavaScript. Your test needs to set this appropriately, and then the bubble
  // will proceed as desired as soon as Show() is called.
  void set_auto_response_for_test(AutoResponseType response) {
    auto_response_for_test_ = response;
  }

 private:
  friend class test::PermissionRequestManagerTestApi;

  // TODO(felt): Update testing to use the TestApi so that it doesn't involve a
  // lot of friends.
  friend class GeolocationBrowserTest;
  friend class GeolocationPermissionContextTests;
  friend class MockPermissionPromptFactory;
  friend class PermissionContextBaseTests;
  friend class PermissionRequestManagerTest;
  friend class content::WebContentsUserData<PermissionRequestManager>;
  FRIEND_TEST_ALL_PREFIXES(DownloadTest, TestMultipleDownloadsBubble);

  explicit PermissionRequestManager(content::WebContents* web_contents);

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentOnLoadCompletedInMainFrame() override;
  void DocumentLoadedInFrame(
      content::RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // PermissionPrompt::Delegate:
  const std::vector<PermissionRequest*>& Requests() override;
  PermissionPrompt::DisplayNameOrOrigin GetDisplayNameOrOrigin() override;
  void Accept() override;
  void Deny() override;
  void Closing() override;

  // Posts a task which will allow the bubble to become visible if it is needed.
  void ScheduleShowBubble();

  // If we aren't already showing a bubble, dequeue and show a pending request.
  void DequeueRequestsAndShowBubble();

  // Shows the bubble for a request that has just been dequeued, or re-show a
  // bubble after switching tabs away and back.
  void ShowBubble(bool is_reshow);

  // Delete the view object
  void DeleteBubble();

  // Delete the view object, finalize requests, asynchronously show a queued
  // request if present.
  void FinalizeBubble(PermissionAction permission_action);

  // Cancel all pending or active requests and destroy the PermissionPrompt if
  // one exists. This is called if the WebContents is destroyed or navigates its
  // main frame.
  void CleanUpRequests();

  // Searches |requests_|, |queued_requests_| and |queued_frame_requests_| - but
  // *not* |duplicate_requests_| - for a request matching |request|, and returns
  // the matching request, or |nullptr| if no match. Note that the matching
  // request may or may not be the same object as |request|.
  PermissionRequest* GetExistingRequest(PermissionRequest* request);

  // Calls PermissionGranted on a request and all its duplicates.
  void PermissionGrantedIncludingDuplicates(PermissionRequest* request);
  // Calls PermissionDenied on a request and all its duplicates.
  void PermissionDeniedIncludingDuplicates(PermissionRequest* request);
  // Calls Cancelled on a request and all its duplicates.
  void CancelledIncludingDuplicates(PermissionRequest* request);
  // Calls RequestFinished on a request and all its duplicates.
  void RequestFinishedIncludingDuplicates(PermissionRequest* request);

  void NotifyBubbleAdded();
  void NotifyBubbleRemoved();

  void DoAutoResponseForTesting();

  // Factory to be used to create views when needed.
  PermissionPrompt::Factory view_factory_;

  // The UI surface for an active permission prompt if we're displaying one.
  // On Desktop, we destroy this upon tab switching, while on Android we keep
  // the object alive. The infobar system hides the actual infobar UI and modals
  // prevent tab switching.
  std::unique_ptr<PermissionPrompt> view_;
  // We only show new prompts when both of these are true.
  bool main_frame_has_fully_loaded_;
  bool tab_is_hidden_;

  std::vector<PermissionRequest*> requests_;
  base::circular_deque<PermissionRequest*> queued_requests_;
  // Maps from the first request of a kind to subsequent requests that were
  // duped against it.
  std::unordered_multimap<PermissionRequest*, PermissionRequest*>
      duplicate_requests_;

  base::ObserverList<Observer>::Unchecked observer_list_;
  AutoResponseType auto_response_for_test_;

  base::WeakPtrFactory<PermissionRequestManager> weak_factory_;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_MANAGER_H_
