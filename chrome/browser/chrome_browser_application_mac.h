// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_APPLICATION_MAC_H_
#define CHROME_BROWSER_CHROME_BROWSER_APPLICATION_MAC_H_

#ifdef __OBJC__

#import <AppKit/AppKit.h>
#include <stddef.h>

#import "base/mac/scoped_sending_event.h"
#import "base/message_loop/message_pump_mac.h"

@interface BrowserCrApplication : NSApplication<CrAppProtocol,
                                                CrAppControlProtocol> {
 @private
  BOOL handlingSendEvent_;
}

// Our implementation of |-terminate:| only attempts to terminate the
// application, i.e., begins a process which may lead to termination. This
// method cancels that process.
- (void)cancelTerminate:(id)sender;
@end

#endif  // __OBJC__

namespace chrome_browser_application_mac {

// To be used to instantiate BrowserCrApplication from C++ code.
void RegisterBrowserCrApp();

// Calls -[NSApp terminate:].
void Terminate();

// Cancels a termination started by |Terminate()|.
void CancelTerminate();

}  // namespace chrome_browser_application_mac

#endif  // CHROME_BROWSER_CHROME_BROWSER_APPLICATION_MAC_H_
