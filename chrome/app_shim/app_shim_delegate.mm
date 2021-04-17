// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app_shim/app_shim_delegate.h"

#include "base/mac/foundation_util.h"
#include "chrome/app_shim/app_shim_controller.h"

@implementation AppShimDelegate

- (instancetype)initWithController:(AppShimController*)controller {
  if (self = [super init])
    _appShimController = controller;
  return self;
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
  _appShimController->OnAppFinishedLaunching();
}

- (BOOL)application:(NSApplication*)app openFile:(NSString*)filename {
  std::vector<base::FilePath> filePaths = {
      base::mac::NSStringToFilePath(filename)};
  _appShimController->OpenFiles(filePaths);
  return YES;
}

- (void)application:(NSApplication*)app openFiles:(NSArray*)filenames {
  std::vector<base::FilePath> filePaths;
  for (NSString* filename in filenames)
    filePaths.push_back(base::mac::NSStringToFilePath(filename));
  _appShimController->OpenFiles(filePaths);
  [app replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
}

- (void)applicationWillBecomeActive:(NSNotification*)notification {
  return _appShimController->host()->FocusApp();
}

- (BOOL)applicationShouldHandleReopen:(NSApplication*)sender
                    hasVisibleWindows:(BOOL)flag {
  _appShimController->host()->ReopenApp();
  return YES;
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  return NO;
}

@end
