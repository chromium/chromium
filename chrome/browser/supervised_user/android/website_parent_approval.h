// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_ANDROID_WEBSITE_PARENT_APPROVAL_H_
#define CHROME_BROWSER_SUPERVISED_USER_ANDROID_WEBSITE_PARENT_APPROVAL_H_

// The glue for Java-side implementation of WebsiteParentApproval.
class WebsiteParentApproval {
 public:
  // Returns whether the local approval flow is supported or not.
  static bool IsLocalApprovalSupported();

  // Request local approval from the parent.
  //
  // TODO(crbug.com/1272462): pass URL, favicon, callback.
  static void RequestLocalApproval();

  WebsiteParentApproval() = delete;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_ANDROID_WEBSITE_PARENT_APPROVAL_H_
