// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the penultimate pieces of the Mac shutdown puzzle. For
// an in-depth overview of the Mac shutdown path, see the comment above
// -[BrowserCrApplication terminate:].

#include "chrome/browser/lifetime/application_lifetime.h"

#include "base/check.h"
#import "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "ui/views/widget/widget.h"

namespace chrome {

// At this point, the user is trying to quit (or the system is forcing the
// application to quit) and all browsers have been successfully closed. The
// final step in shutdown is to post the NSApplicationWillTerminateNotification
// to end the -[NSApplication run] event loop.
void HandleAppExitingForPlatform() {
  static bool kill_me_now = false;
  CHECK(!kill_me_now);
  kill_me_now = true;

  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSApplicationWillTerminateNotification
                    object:NSApp];

  // Views Widgets host ui::Compositors that talk to the GPU process, whose host
  // complains if it is destroyed while in-use. By this point, all browser
  // windows are closed, which tear down any Widgets parented to them. This will
  // additionally close any unparented, non-Browser Widgets.
  views::Widget::CloseAllSecondaryWidgets();
}

}  // namespace chrome
