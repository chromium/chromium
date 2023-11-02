// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_ANDROID_WEBSITE_PARENT_APPROVAL_H_
#define CHROME_BROWSER_SUPERVISED_USER_ANDROID_WEBSITE_PARENT_APPROVAL_H_

#include "base/callback_forward.h"
#include "chrome/browser/supervised_user/web_approvals_manager.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

// The glue for Java-side implementation of WebsiteParentApproval.
class WebsiteParentApproval {
 public:
  // Returns whether the local approval flow is supported or not.
  static bool IsLocalApprovalSupported();

  // Request local approval from the parent.
  //
  // The provided callback will be called when the local approval flow is no
  // longer active (whether that's because the parent explicitly completed the
  // flow and approved or denied, or for example because the parent exited
  // before completing the auth flow).
  static void RequestLocalApproval(
      content::WebContents* web_contents,
      const GURL& url,
      base::OnceCallback<void(AndroidLocalWebApprovalFlowOutcome)> callback);

  WebsiteParentApproval() = delete;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_ANDROID_WEBSITE_PARENT_APPROVAL_H_
