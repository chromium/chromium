// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app_shim/app_shim_delegate.h"

#include "base/mac/foundation_util.h"
#include "chrome/app_shim/app_shim_controller.h"

@implementation AppShimDelegate

- (BOOL)getFilesToOpenAtStartup:(std::vector<base::FilePath>*)out {
  if (filesToOpenAtStartup_.empty())
    return NO;

  out->insert(out->end(), filesToOpenAtStartup_.begin(),
              filesToOpenAtStartup_.end());
  filesToOpenAtStartup_.clear();
  return YES;
}

- (void)setController:(AppShimController*)controller {
  appShimController_ = controller;
}

- (void)openFiles:(NSArray*)filenames {
  std::vector<base::FilePath> filePaths;
  for (NSString* filename in filenames)
    filePaths.push_back(base::mac::NSStringToFilePath(filename));

  // If the AppShimController is ready, try to send a FocusApp. If that fails,
  // (e.g. if launching has not finished), enqueue the files.
  if (appShimController_ && appShimController_->SendFocusApp(
                                apps::APP_SHIM_FOCUS_OPEN_FILES, filePaths)) {
    return;
  }

  filesToOpenAtStartup_.insert(filesToOpenAtStartup_.end(), filePaths.begin(),
                               filePaths.end());
}

- (BOOL)application:(NSApplication*)app openFile:(NSString*)filename {
  [self openFiles:@[ filename ]];
  return YES;
}

- (void)application:(NSApplication*)app openFiles:(NSArray*)filenames {
  [self openFiles:filenames];
  [app replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
}

- (BOOL)applicationOpenUntitledFile:(NSApplication*)app {
  if (appShimController_) {
    return appShimController_->SendFocusApp(apps::APP_SHIM_FOCUS_REOPEN,
                                            std::vector<base::FilePath>());
  }

  return NO;
}

- (void)applicationWillBecomeActive:(NSNotification*)notification {
  if (appShimController_) {
    appShimController_->SendFocusApp(apps::APP_SHIM_FOCUS_NORMAL,
                                     std::vector<base::FilePath>());
  }
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  return NO;
}

@end
