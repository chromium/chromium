// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/chrome_browser_cloud_management_browsertest_mac_util.h"

#import <Cocoa/Cocoa.h>

namespace policy {

void PostAppControllerNSNotifications() {
  // Simulate the user clicking a window other than the dialog.
  // The Profile is not ready when the dialog is displayed, so it can't be
  // accessed.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidResignMainNotification
                    object:nil];

  // Simulate the user hiding Chrome via Cmd+h when the dialog is displayed.
  // The ExtensionAppShimHandler hasn't been created when the dialog is
  // displayed, so it must be skipped.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSApplicationWillHideNotification
                    object:nil];
}

}  // namespace
