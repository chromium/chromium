// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLATFORM_UTIL_H_
#define CHROME_BROWSER_PLATFORM_UTIL_H_

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class GURL;
class Profile;

namespace base {
class FilePath;
}

namespace platform_util {

// Result of calling OpenFile() or OpenFolder() passed into OpenOperationResult.
enum OpenOperationResult {
  OPEN_SUCCEEDED,
  OPEN_FAILED_PATH_NOT_FOUND,  // Specified path does not exist.
  OPEN_FAILED_INVALID_TYPE,  // Type of object found at path did not match what
                             // was expected. I.e. OpenFile was called on a
                             // folder or OpenFolder called on a file.
  OPEN_FAILED_NO_HANLDER_FOR_FILE_TYPE,  // There was no file handler capable of
                                         // opening file. Only returned on
                                         // ChromeOS.
  OPEN_FAILED_FILE_ERROR,  // Open operation failed due to some other file
                           // error.
};

// Type of item that is the target of the OpenItem() call.
enum OpenItemType {
  OPEN_FILE,
  OPEN_FOLDER,
};

// Callback used with OpenFile and OpenFolder.
typedef base::OnceCallback<void(OpenOperationResult)> OpenOperationCallback;

// Opens the item specified by |full_path|, which is expected to be the type
// indicated by |item_type| in the desktop's default manner.
// |callback| will be invoked on the UI thread with the result of the open
// operation.
//
// It is an error if the object at |full_path| does not match the intended type
// specified in |item_type|. This error will be reported to |callback|.
//
// Note: On all platforms, the user may be shown additional UI if there is no
// suitable handler for |full_path|. On Chrome OS, all errors will result in
// visible error messages iff |callback| is not specified.
// Must be called on the UI thread.
void OpenItem(Profile* profile,
              const base::FilePath& full_path,
              OpenItemType item_type,
              OpenOperationCallback callback);

// Opens the folder containing the item specified by |full_path| in the
// desktop's default manner. If possible, the item will be selected. The
// |profile| is used to determine the running profile of file manager app in
// Chrome OS only. |profile| is not used in platforms other than Chrome OS. Must
// be called on the UI thread.
void ShowItemInFolder(Profile* profile, const base::FilePath& full_path);

// Open the given external protocol URL in the desktop's default manner.
// (For example, mailto: URLs in the default mail user agent.)
// Must be called from the UI thread.
#if BUILDFLAG(IS_CHROMEOS_ASH)
void OpenExternal(Profile* profile, const GURL& url);
#else
void OpenExternal(const GURL& url);
#endif

// Get the top level window for the native view. This can return NULL.
gfx::NativeWindow GetTopLevel(gfx::NativeView view);

// Returns a NativeView handle for parenting dialogs off |window|. This can be
// used to position a dialog using a NativeWindow, when a NativeView (e.g.
// browser tab) isn't available.
gfx::NativeView GetViewForWindow(gfx::NativeWindow window);

// Get the direct parent of |view|, may return NULL.
gfx::NativeView GetParent(gfx::NativeView view);

// Returns true if |window| is the foreground top level window.
bool IsWindowActive(gfx::NativeWindow window);

// Activate the window, bringing it to the foreground top level.
void ActivateWindow(gfx::NativeWindow window);

// Returns true if the view is visible. The exact definition of this is
// platform-specific, but it is generally not "visible to the user", rather
// whether the view has the visible attribute set.
bool IsVisible(gfx::NativeView view);

#if BUILDFLAG(IS_MAC)
// On the Mac, back and forward swipe gestures can be triggered using a scroll
// gesture, if enabled in System Preferences. This function returns true if the
// feature is enabled, and false otherwise.
bool IsSwipeTrackingFromScrollEventsEnabled();

// Returns the active window which accepts keyboard inputs.
NSWindow* GetActiveWindow();
#endif

// Returns true if the given browser window is in locked fullscreen mode
// (a special type of fullscreen where the user is locked into one browser
// window).
bool IsBrowserLockedFullscreen(const Browser* browser);

}  // namespace platform_util

#endif  // CHROME_BROWSER_PLATFORM_UTIL_H_
