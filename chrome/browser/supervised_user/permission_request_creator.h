// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_PERMISSION_REQUEST_CREATOR_H_
#define CHROME_BROWSER_SUPERVISED_USER_PERMISSION_REQUEST_CREATOR_H_

#include "base/callback_forward.h"

class GURL;

class PermissionRequestCreator {
 public:
  typedef base::OnceCallback<void(bool)> SuccessCallback;

  virtual ~PermissionRequestCreator() {}

  // Returns false if creating a permission request is expected to fail.
  // If this method returns true, it doesn't necessary mean that creating the
  // permission request will succeed, just that it's not known in advance
  // to fail.
  virtual bool IsEnabled() const = 0;

  // Creates a permission request for |url_requested| and calls |callback| with
  // the result (whether creating the permission request was successful).
  virtual void CreateURLAccessRequest(const GURL& url_requested,
                                      SuccessCallback callback) = 0;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_PERMISSION_REQUEST_CREATOR_H_
