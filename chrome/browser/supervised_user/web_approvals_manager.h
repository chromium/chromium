// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_WEB_APPROVALS_MANAGER_H_
#define CHROME_BROWSER_SUPERVISED_USER_WEB_APPROVALS_MANAGER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"

class GURL;
class PermissionRequestCreator;

// Manages remote and local web approval requests from Family Link users.
//
// Remote requests are forwarded to the guardian and processed asynchronously.
// The result of the remote approval syncs as a new web rule to the client and
// is not handled in this class.
// Local request opens OS specific local approval flow. The result of the local
// approval is not handled in this class.

class WebApprovalsManager {
 public:
  // Callback indicating whether the URL access request was initiated
  // successfully.
  using ApprovalRequestInitiatedCallback = base::OnceCallback<void(bool)>;

  WebApprovalsManager();

  WebApprovalsManager(const WebApprovalsManager&) = delete;
  WebApprovalsManager& operator=(const WebApprovalsManager&) = delete;

  ~WebApprovalsManager();

  // Requests a local approval flow for the `url`.
  // Runs the `callback` to inform the caller whether the flow initiation was
  // successful.
  void RequestLocalApproval(const GURL& url,
                            ApprovalRequestInitiatedCallback callback);

  // Adds a remote approval request for the `url`.
  // The `callback` is run when the request was sent or sending of the request
  // failed.
  void RequestRemoteApproval(const GURL& url,
                             ApprovalRequestInitiatedCallback callback);

  // Returns whether remote approval requests are enabled.
  bool AreRemoteApprovalRequestsEnabled() const;

  // Adds remote approval request `creator` to handle remote approval requests.
  void AddRemoteApprovalRequestCreator(
      std::unique_ptr<PermissionRequestCreator> creator);

  // Clears all remote approval requests creators.
  void ClearRemoteApprovalRequestsCreators();

 private:
  using CreateRemoteApprovalRequestCallback =
      base::RepeatingCallback<void(PermissionRequestCreator*,
                                   ApprovalRequestInitiatedCallback)>;

  size_t FindEnabledRemoteApprovalRequestCreator(size_t start) const;

  void AddRemoteApprovalRequestInternal(
      const CreateRemoteApprovalRequestCallback& create_request,
      ApprovalRequestInitiatedCallback callback,
      size_t index);

  void OnRemoteApprovalRequestIssued(
      const CreateRemoteApprovalRequestCallback& create_request,
      ApprovalRequestInitiatedCallback callback,
      size_t index,
      bool success);

  // Stores remote approval request creators.
  // The creators are cleared during shutdown.
  std::vector<std::unique_ptr<PermissionRequestCreator>>
      remote_approval_request_creators_;

  base::WeakPtrFactory<WebApprovalsManager> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_WEB_APPROVALS_MANAGER_H_
