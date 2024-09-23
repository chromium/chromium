// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#import <Cocoa/Cocoa.h>

#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/platform_util_internal.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "net/base/apple/url_conversions.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace platform_util {

// Returns true if revealing file paths in the Finder should be skipped
// because it's not needed while running a test.
bool WorkspacePathRevealDisabledForTest() {
  // Note: the kTestType switch is only added on browser tests, but not unit
  // tests. Unit tests need to add the switch manually:
  //
  //   base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  //   command_line->AppendSwitch(switches::kTestType);
  //
  return base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType);
}

void ShowItemInFolder(Profile* profile, const base::FilePath& full_path) {
  DCHECK([NSThread isMainThread]);

  // The Finder creates a new window on each `full_path` reveal. Skip
  // revealing the path during testing to avoid an avalanche of new
  // Finder windows.
  if (WorkspacePathRevealDisabledForTest() ||
      !internal::AreShellOperationsAllowed()) {
    return;
  }

  NSURL* url = base::apple::FilePathToNSURL(full_path);
  [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[ url ]];
}

void OpenFileOnMainThread(const base::FilePath& full_path) {
  DCHECK([NSThread isMainThread]);
  NSURL* url = base::apple::FilePathToNSURL(full_path);
  if (!url)
    return;

  [[NSWorkspace sharedWorkspace]
                openURL:url
          configuration:[NSWorkspaceOpenConfiguration configuration]
      completionHandler:nil];
}

namespace internal {

void PlatformOpenVerifiedItem(const base::FilePath& path, OpenItemType type) {
  switch (type) {
    case OPEN_FILE:
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&OpenFileOnMainThread, path));
      return;
    case OPEN_FOLDER:
      NSURL* url = base::apple::FilePathToNSURL(path);
      if (!url)
        return;

      // Note that there exists a TOCTOU race between the time that |path| was
      // verified as being a directory and when NSWorkspace invokes Finder (or
      // alternative) to open |path_string|.
      [[NSWorkspace sharedWorkspace] openURL:url];
      return;
  }
}

}  // namespace internal

void OpenExternal(const GURL& url) {
  DCHECK([NSThread isMainThread]);
  NSURL* ns_url = net::NSURLWithGURL(url);

  if (!ns_url || ![[NSWorkspace sharedWorkspace] openURL:ns_url]) {
    LOG(WARNING) << "NSWorkspace failed to open URL " << url;
  }
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
  return NSEvent.swipeTrackingFromScrollEventsEnabled;
}

NSWindow* GetActiveWindow() {
  return [NSApp keyWindow];
}

}  // namespace platform_util
