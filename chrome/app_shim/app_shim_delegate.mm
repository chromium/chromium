// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app_shim/app_shim_delegate.h"

#include "base/apple/foundation_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app_shim/app_shim_controller.h"
#include "net/base/mac/url_conversions.h"

@implementation AppShimDelegate {
  raw_ptr<AppShimController> _appShimController;  // Weak, owns |this|
}

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
      base::apple::NSStringToFilePath(filename)};
  _appShimController->OpenFiles(filePaths);
  return YES;
}

- (void)application:(NSApplication*)app openFiles:(NSArray*)filenames {
  std::vector<base::FilePath> filePaths;
  for (NSString* filename in filenames)
    filePaths.push_back(base::apple::NSStringToFilePath(filename));
  _appShimController->OpenFiles(filePaths);
  [app replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
}

- (void)application:(NSApplication*)app openURLs:(NSArray<NSURL*>*)urls {
  std::vector<GURL> urls_to_open;
  for (NSURL* url in urls)
    urls_to_open.push_back(net::GURLWithNSURL(url));
  _appShimController->OpenUrls(urls_to_open);
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
  SEL action = [item action];
  if (action == @selector(commandDispatch:)) {
    NSInteger tag = [item tag];
    if (tag == IDC_WEB_APP_SETTINGS || tag == IDC_NEW_WINDOW) {
      return YES;
    }
  }
  return NO;
}

- (void)commandDispatch:(id)sender {
  NSInteger tag = [sender tag];
  NSLog(@"Dispatching: %d", int(tag));
  _appShimController->CommandDispatch(tag);
}

- (NSMenu*)applicationDockMenu:(NSApplication*)sender {
  return _appShimController->GetApplicationDockMenu();
}

// Called when the app is shutting down. Used to persist the current state of
// the app.
- (void)applicationWillTerminate:(NSNotification*)aNotification {
  _appShimController->ApplicationWillTerminate();
}

@end
