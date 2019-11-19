// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_PERMISSION_REQUEST_CREATOR_MOCK_H_
#define CHROME_BROWSER_SUPERVISED_USER_PERMISSION_REQUEST_CREATOR_MOCK_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/supervised_user/permission_request_creator.h"
#include "url/gurl.h"

class Profile;

class PermissionRequestCreatorMock : public PermissionRequestCreator {
 public:
  explicit PermissionRequestCreatorMock(Profile* profile);
  ~PermissionRequestCreatorMock() override;

  // PermissionRequestCreator:
  bool IsEnabled() const override;
  void CreateURLAccessRequest(const GURL& url_requested,
                              SuccessCallback callback) override;

  // Sets whether the next call to create requests will succeed.
  void SetPermissionResult(bool result);
  void SetEnabled();

  // Delays approvals for incoming requests until
  // |PermissionRequestCreatorMock::HandleDelayedRequests| is called.
  void DelayHandlingForNextRequests();
  void HandleDelayedRequests();

  // Getter methods.
  const std::vector<GURL>& url_requests() const { return url_requests_; }

 private:
  void CreateURLAccessRequestImpl(const GURL& url_requested);

  bool result_ = false;
  bool enabled_ = false;
  bool delay_handling_ = false;
  int last_url_request_handled_index_ = 0;

  Profile* profile_ = nullptr;

  std::vector<GURL> url_requests_;

  DISALLOW_COPY_AND_ASSIGN(PermissionRequestCreatorMock);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_PERMISSION_REQUEST_CREATOR_MOCK_H_
