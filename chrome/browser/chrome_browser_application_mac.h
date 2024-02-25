// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_APPLICATION_MAC_H_
#define CHROME_BROWSER_CHROME_BROWSER_APPLICATION_MAC_H_

#ifdef __OBJC__

#import <AppKit/AppKit.h>
#include <stddef.h>

#import "base/mac/scoped_sending_event.h"
#import "base/message_loop/message_pump_apple.h"

@interface BrowserCrApplication
    : NSApplication <CrAppProtocol, CrAppControlProtocol>

// Our implementation of |-terminate:| only attempts to terminate the
// application, i.e., begins a process which may lead to termination. This
// method cancels that process.
- (void)cancelTerminate:(id)sender;

- (BOOL)voiceOverStateForTesting;

@end

#endif  // __OBJC__

namespace chrome_browser_application_mac {

// To be used to instantiate BrowserCrApplication from C++ code.
void RegisterBrowserCrApp();

// Provide additional initialization for headless mode from C++ code.
void InitializeHeadlessMode();

// Calls -[NSApp terminate:].
void Terminate();

// Cancels a termination started by |Terminate()|.
void CancelTerminate();

}  // namespace chrome_browser_application_mac

#endif  // CHROME_BROWSER_CHROME_BROWSER_APPLICATION_MAC_H_
