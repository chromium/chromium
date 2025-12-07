// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_ANDROID_EXTENSION_PARENT_APPROVAL_H_
#define CHROME_BROWSER_SUPERVISED_USER_ANDROID_EXTENSION_PARENT_APPROVAL_H_

#include "base/functional/callback_forward.h"
#include "extensions/browser/supervised_extension_approval_result.h"

namespace content {
class WebContents;
}  // namespace content

// The glue for Java-side implementation of ExtensionParentApproval.
class ExtensionParentApproval {
 public:
  // Returns whether the extension approval flow is supported or not.
  static bool IsExtensionApprovalSupported();

  // Request extension approval from the parent.
  // The provided callback will be used when the extension approval flow is no
  // longer active (whether that's because the parent explicitly completed the
  // flow and approved or denied, or for example because the parent exited
  // before completing the auth flow).
  static void RequestExtensionApproval(
      content::WebContents* web_contents,
      base::OnceCallback<void(extensions::SupervisedExtensionApprovalResult)>
          callback);

  ExtensionParentApproval() = delete;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_ANDROID_EXTENSION_PARENT_APPROVAL_H_
