// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_SHIM_APP_SHIM_DELEGATE_H_
#define CHROME_APP_SHIM_APP_SHIM_DELEGATE_H_

#import <Cocoa/Cocoa.h>
#include <vector>

#include "base/files/file_path.h"

class AppShimController;

// An application delegate to catch user interactions and send the appropriate
// IPC messages to Chrome.
@interface AppShimDelegate
    : NSObject<NSApplicationDelegate, NSUserInterfaceValidations> {
 @private
  AppShimController* appShimController_;  // Weak, initially NULL.
  BOOL terminateNow_;
  BOOL terminateRequested_;
  std::vector<base::FilePath> filesToOpenAtStartup_;
}

// The controller is initially NULL. Setting it indicates to the delegate that
// the controller has finished initialization.
- (void)setController:(AppShimController*)controller;

// Gets files that were queued because the controller was not ready.
// Returns whether any FilePaths were added to |out|.
- (BOOL)getFilesToOpenAtStartup:(std::vector<base::FilePath>*)out;

// If the controller is ready, this sends a FocusApp with the files to open.
// Otherwise, this adds the files to |filesToOpenAtStartup_|.
// Takes an array of NSString*.
- (void)openFiles:(NSArray*)filename;

// Terminate immediately. This is necessary as we override terminate: to send
// a QuitApp message.
- (void)terminateNow;

@end

#endif  // CHROME_APP_SHIM_APP_SHIM_DELEGATE_H_
