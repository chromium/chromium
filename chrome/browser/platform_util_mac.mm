// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#import <Cocoa/Cocoa.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/platform_util_internal.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace platform_util {

void ShowItemInFolder(Profile* profile, const base::FilePath& full_path) {
  DCHECK([NSThread isMainThread]);
  NSString* path_string = base::SysUTF8ToNSString(full_path.value());
  if (!path_string || ![[NSWorkspace sharedWorkspace] selectFile:path_string
                                        inFileViewerRootedAtPath:@""])
    LOG(WARNING) << "NSWorkspace failed to select file " << full_path.value();
}

void OpenFileOnMainThread(const base::FilePath& full_path) {
  DCHECK([NSThread isMainThread]);
  NSString* path_string = base::SysUTF8ToNSString(full_path.value());
  if (!path_string)
    return;

  // On Mavericks or later, NSWorkspaceLaunchWithErrorPresentation will
  // properly handle Finder activation for quarantined files
  // (http://crbug.com/32921) and unassociated file types
  // (http://crbug.com/50263).
  NSURL* url = [NSURL fileURLWithPath:path_string];
  if (!url)
    return;

  const NSWorkspaceLaunchOptions launch_options =
      NSWorkspaceLaunchAsync | NSWorkspaceLaunchWithErrorPresentation;
  [[NSWorkspace sharedWorkspace] openURLs:@[ url ]
                  withAppBundleIdentifier:nil
                                  options:launch_options
           additionalEventParamDescriptor:nil
                        launchIdentifiers:NULL];
}

namespace internal {

void PlatformOpenVerifiedItem(const base::FilePath& path, OpenItemType type) {
  switch (type) {
    case OPEN_FILE:
      base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                     base::BindOnce(&OpenFileOnMainThread, path));
      return;
    case OPEN_FOLDER:
      NSString* path_string = base::SysUTF8ToNSString(path.value());
      if (!path_string)
        return;
      // Note that there exists a TOCTOU race between the time that |path| was
      // verified as being a directory and when NSWorkspace invokes Finder (or
      // alternative) to open |path_string|.
      [[NSWorkspace sharedWorkspace] openFile:path_string];
      return;
  }
}

}  // namespace internal

void OpenExternal(Profile* profile, const GURL& url) {
  DCHECK([NSThread isMainThread]);
  NSString* url_string = base::SysUTF8ToNSString(url.spec());
  NSURL* ns_url = [NSURL URLWithString:url_string];
  if (!ns_url || ![[NSWorkspace sharedWorkspace] openURL:ns_url])
    LOG(WARNING) << "NSWorkspace failed to open URL " << url;
}

gfx::NativeWindow GetTopLevel(gfx::NativeView view) {
  return gfx::NativeWindow([view.GetNativeNSView() window]);
}

gfx::NativeView GetViewForWindow(gfx::NativeWindow native_window) {
  NSWindow* window = native_window.GetNativeNSWindow();
  DCHECK(window);
  DCHECK([window contentView]);
  return gfx::NativeView([window contentView]);
}

gfx::NativeView GetParent(gfx::NativeView view) {
  return gfx::NativeView(nil);
}

bool IsWindowActive(gfx::NativeWindow native_window) {
  // If |window| is a doppelganger NSWindow being used to track an NSWindow that
  // is being hosted in another process, then use the views::Widget interface to
  // interact with it.
  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(native_window);
  if (widget)
    return widget->IsActive();

  NSWindow* window = native_window.GetNativeNSWindow();
  return [window isKeyWindow] || [window isMainWindow];
}

void ActivateWindow(gfx::NativeWindow native_window) {
  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(native_window);
  if (widget)
    return widget->Activate();

  NSWindow* window = native_window.GetNativeNSWindow();
  [window makeKeyAndOrderFront:nil];
}

bool IsVisible(gfx::NativeView native_view) {
  views::Widget* widget = views::Widget::GetWidgetForNativeView(native_view);
  if (widget)
    return widget->IsVisible();

  // A reasonable approximation of how you'd expect this to behave.
  NSView* view = native_view.GetNativeNSView();
  return (view &&
          ![view isHiddenOrHasHiddenAncestor] &&
          [view window] &&
          [[view window] isVisible]);
}

bool IsSwipeTrackingFromScrollEventsEnabled() {
  SEL selector = @selector(isSwipeTrackingFromScrollEventsEnabled);
  return [NSEvent respondsToSelector:selector]
      && [NSEvent performSelector:selector];
}

}  // namespace platform_util
