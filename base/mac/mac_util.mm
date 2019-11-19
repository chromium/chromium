// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/mac_util.h"

#import <Cocoa/Cocoa.h>
#import <IOKit/IOKitLib.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/utsname.h>
#include <sys/xattr.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/sys_string_conversions.h"

namespace base {
namespace mac {

namespace {

// The current count of outstanding requests for full screen mode from browser
// windows, plugins, etc.
int g_full_screen_requests[kNumFullScreenModes] = { 0 };

// Sets the appropriate application presentation option based on the current
// full screen requests.  Since only one presentation option can be active at a
// given time, full screen requests are ordered by priority.  If there are no
// outstanding full screen requests, reverts to normal mode.  If the correct
// presentation option is already set, does nothing.
void SetUIMode() {
  NSApplicationPresentationOptions current_options =
      [NSApp presentationOptions];

  // Determine which mode should be active, based on which requests are
  // currently outstanding.  More permissive requests take precedence.  For
  // example, plugins request |kFullScreenModeAutoHideAll|, while browser
  // windows request |kFullScreenModeHideDock| when the fullscreen overlay is
  // down.  Precedence goes to plugins in this case, so AutoHideAll wins over
  // HideDock.
  NSApplicationPresentationOptions desired_options =
      NSApplicationPresentationDefault;
  if (g_full_screen_requests[kFullScreenModeAutoHideAll] > 0) {
    desired_options = NSApplicationPresentationHideDock |
                      NSApplicationPresentationAutoHideMenuBar;
  } else if (g_full_screen_requests[kFullScreenModeHideDock] > 0) {
    desired_options = NSApplicationPresentationHideDock;
  } else if (g_full_screen_requests[kFullScreenModeHideAll] > 0) {
    desired_options = NSApplicationPresentationHideDock |
                      NSApplicationPresentationHideMenuBar;
  }

  // Mac OS X bug: if the window is fullscreened (Lion-style) and
  // NSApplicationPresentationDefault is requested, the result is that the menu
  // bar doesn't auto-hide. rdar://13576498 http://www.openradar.me/13576498
  //
  // As a workaround, in that case, explicitly set the presentation options to
  // the ones that are set by the system as it fullscreens a window.
  if (desired_options == NSApplicationPresentationDefault &&
      current_options & NSApplicationPresentationFullScreen) {
    desired_options |= NSApplicationPresentationFullScreen |
                       NSApplicationPresentationAutoHideMenuBar |
                       NSApplicationPresentationAutoHideDock;
  }

  if (current_options != desired_options)
    [NSApp setPresentationOptions:desired_options];
}

// Looks into Shared File Lists corresponding to Login Items for the item
// representing the current application.  If such an item is found, returns a
// retained reference to it. Caller is responsible for releasing the reference.
LSSharedFileListItemRef GetLoginItemForApp() {
  ScopedCFTypeRef<LSSharedFileListRef> login_items(LSSharedFileListCreate(
      NULL, kLSSharedFileListSessionLoginItems, NULL));

  if (!login_items.get()) {
    DLOG(ERROR) << "Couldn't get a Login Items list.";
    return NULL;
  }

  base::scoped_nsobject<NSArray> login_items_array(
      CFToNSCast(LSSharedFileListCopySnapshot(login_items, NULL)));

  NSURL* url = [NSURL fileURLWithPath:[base::mac::MainBundle() bundlePath]];

  for(NSUInteger i = 0; i < [login_items_array count]; ++i) {
    LSSharedFileListItemRef item =
        reinterpret_cast<LSSharedFileListItemRef>(login_items_array[i]);
    base::ScopedCFTypeRef<CFErrorRef> error;
    CFURLRef item_url_ref =
        LSSharedFileListItemCopyResolvedURL(item, 0, error.InitializeInto());

    // This function previously used LSSharedFileListItemResolve(), which could
    // return a NULL URL even when returning no error. This caused
    // <https://crbug.com/760989>. It's not clear one way or the other whether
    // LSSharedFileListItemCopyResolvedURL() shares this behavior, so this check
    // remains in place.
    if (!error && item_url_ref) {
      ScopedCFTypeRef<CFURLRef> item_url(item_url_ref);
      if (CFEqual(item_url, url)) {
        CFRetain(item);
        return item;
      }
    }
  }

  return NULL;
}

bool IsHiddenLoginItem(LSSharedFileListItemRef item) {
  ScopedCFTypeRef<CFBooleanRef> hidden(reinterpret_cast<CFBooleanRef>(
      LSSharedFileListItemCopyProperty(item,
          reinterpret_cast<CFStringRef>(kLSSharedFileListLoginItemHidden))));

  return hidden && hidden == kCFBooleanTrue;
}

}  // namespace

CGColorSpaceRef GetGenericRGBColorSpace() {
  // Leaked. That's OK, it's scoped to the lifetime of the application.
  static CGColorSpaceRef g_color_space_generic_rgb(
      CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));
  DLOG_IF(ERROR, !g_color_space_generic_rgb) <<
      "Couldn't get the generic RGB color space";
  return g_color_space_generic_rgb;
}

CGColorSpaceRef GetSRGBColorSpace() {
  // Leaked.  That's OK, it's scoped to the lifetime of the application.
  static CGColorSpaceRef g_color_space_sRGB =
      CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  DLOG_IF(ERROR, !g_color_space_sRGB) << "Couldn't get the sRGB color space";
  return g_color_space_sRGB;
}

CGColorSpaceRef GetSystemColorSpace() {
  // Leaked.  That's OK, it's scoped to the lifetime of the application.
  // Try to get the main display's color space.
  static CGColorSpaceRef g_system_color_space =
      CGDisplayCopyColorSpace(CGMainDisplayID());

  if (!g_system_color_space) {
    // Use a generic RGB color space.  This is better than nothing.
    g_system_color_space = CGColorSpaceCreateDeviceRGB();

    if (g_system_color_space) {
      DLOG(WARNING) <<
          "Couldn't get the main display's color space, using generic";
    } else {
      DLOG(ERROR) << "Couldn't get any color space";
    }
  }

  return g_system_color_space;
}

// Add a request for full screen mode.  Must be called on the main thread.
void RequestFullScreen(FullScreenMode mode) {
  DCHECK_LT(mode, kNumFullScreenModes);
  if (mode >= kNumFullScreenModes)
    return;

  DCHECK_GE(g_full_screen_requests[mode], 0);
  if (mode < 0)
    return;

  g_full_screen_requests[mode] = std::max(g_full_screen_requests[mode] + 1, 1);
  SetUIMode();
}

// Release a request for full screen mode.  Must be called on the main thread.
void ReleaseFullScreen(FullScreenMode mode) {
  DCHECK_LT(mode, kNumFullScreenModes);
  if (mode >= kNumFullScreenModes)
    return;

  DCHECK_GE(g_full_screen_requests[mode], 0);
  if (mode < 0)
    return;

  g_full_screen_requests[mode] = std::max(g_full_screen_requests[mode] - 1, 0);
  SetUIMode();
}

// Switches full screen modes.  Releases a request for |from_mode| and adds a
// new request for |to_mode|.  Must be called on the main thread.
void SwitchFullScreenModes(FullScreenMode from_mode, FullScreenMode to_mode) {
  DCHECK_LT(from_mode, kNumFullScreenModes);
  DCHECK_LT(to_mode, kNumFullScreenModes);
  if (from_mode >= kNumFullScreenModes || to_mode >= kNumFullScreenModes)
    return;

  DCHECK_GT(g_full_screen_requests[from_mode], 0);
  DCHECK_GE(g_full_screen_requests[to_mode], 0);
  g_full_screen_requests[from_mode] =
      std::max(g_full_screen_requests[from_mode] - 1, 0);
  g_full_screen_requests[to_mode] =
      std::max(g_full_screen_requests[to_mode] + 1, 1);
  SetUIMode();
}

bool GetFileBackupExclusion(const FilePath& file_path) {
  return CSBackupIsItemExcluded(FilePathToCFURL(file_path), nullptr);
}

bool SetFileBackupExclusion(const FilePath& file_path) {
  // When excludeByPath is true the application must be running with root
  // privileges (admin for 10.6 and earlier) but the URL does not have to
  // already exist. When excludeByPath is false the URL must already exist but
  // can be used in non-root (or admin as above) mode. We use false so that
  // non-root (or admin) users don't get their TimeMachine drive filled up with
  // unnecessary backups.
  OSStatus os_err = CSBackupSetItemExcluded(FilePathToCFURL(file_path),
                                            /*exclude=*/TRUE,
                                            /*excludeByPath=*/FALSE);
  if (os_err != noErr) {
    OSSTATUS_DLOG(WARNING, os_err)
        << "Failed to set backup exclusion for file '"
        << file_path.value().c_str() << "'";
  }
  return os_err == noErr;
}

bool CheckLoginItemStatus(bool* is_hidden) {
  ScopedCFTypeRef<LSSharedFileListItemRef> item(GetLoginItemForApp());
  if (!item.get())
    return false;

  if (is_hidden)
    *is_hidden = IsHiddenLoginItem(item);

  return true;
}

void AddToLoginItems(bool hide_on_startup) {
  ScopedCFTypeRef<LSSharedFileListItemRef> item(GetLoginItemForApp());
  if (item.get() && (IsHiddenLoginItem(item) == hide_on_startup)) {
    return;  // Already is a login item with required hide flag.
  }

  ScopedCFTypeRef<LSSharedFileListRef> login_items(LSSharedFileListCreate(
      NULL, kLSSharedFileListSessionLoginItems, NULL));

  if (!login_items.get()) {
    DLOG(ERROR) << "Couldn't get a Login Items list.";
    return;
  }

  // Remove the old item, it has wrong hide flag, we'll create a new one.
  if (item.get()) {
    LSSharedFileListItemRemove(login_items, item);
  }

  NSURL* url = [NSURL fileURLWithPath:[base::mac::MainBundle() bundlePath]];

  BOOL hide = hide_on_startup ? YES : NO;
  NSDictionary* properties =
      @{(NSString*)kLSSharedFileListLoginItemHidden : @(hide) };

  ScopedCFTypeRef<LSSharedFileListItemRef> new_item;
  new_item.reset(LSSharedFileListInsertItemURL(
      login_items, kLSSharedFileListItemLast, NULL, NULL,
      reinterpret_cast<CFURLRef>(url),
      reinterpret_cast<CFDictionaryRef>(properties), NULL));

  if (!new_item.get()) {
    DLOG(ERROR) << "Couldn't insert current app into Login Items list.";
  }
}

void RemoveFromLoginItems() {
  ScopedCFTypeRef<LSSharedFileListItemRef> item(GetLoginItemForApp());
  if (!item.get())
    return;

  ScopedCFTypeRef<LSSharedFileListRef> login_items(LSSharedFileListCreate(
      NULL, kLSSharedFileListSessionLoginItems, NULL));

  if (!login_items.get()) {
    DLOG(ERROR) << "Couldn't get a Login Items list.";
    return;
  }

  LSSharedFileListItemRemove(login_items, item);
}

bool WasLaunchedAsLoginOrResumeItem() {
  ProcessSerialNumber psn = { 0, kCurrentProcess };
  ProcessInfoRec info = {};
  info.processInfoLength = sizeof(info);

// GetProcessInformation has been deprecated since macOS 10.9, but there is no
// replacement that provides the information we need. See
// https://crbug.com/650854.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  if (GetProcessInformation(&psn, &info) == noErr) {
#pragma clang diagnostic pop
    ProcessInfoRec parent_info = {};
    parent_info.processInfoLength = sizeof(parent_info);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (GetProcessInformation(&info.processLauncher, &parent_info) == noErr) {
#pragma clang diagnostic pop
      return parent_info.processSignature == 'lgnw';
    }
  }
  return false;
}

bool WasLaunchedAsLoginItemRestoreState() {
  // "Reopen windows..." option was added for Lion.  Prior OS versions should
  // not have this behavior.
  if (!WasLaunchedAsLoginOrResumeItem())
    return false;

  CFStringRef app = CFSTR("com.apple.loginwindow");
  CFStringRef save_state = CFSTR("TALLogoutSavesState");
  ScopedCFTypeRef<CFPropertyListRef> plist(
      CFPreferencesCopyAppValue(save_state, app));
  // According to documentation, com.apple.loginwindow.plist does not exist on a
  // fresh installation until the user changes a login window setting.  The
  // "reopen windows" option is checked by default, so the plist would exist had
  // the user unchecked it.
  // https://developer.apple.com/library/mac/documentation/macosx/conceptual/bpsystemstartup/chapters/CustomLogin.html
  if (!plist)
    return true;

  if (CFBooleanRef restore_state = base::mac::CFCast<CFBooleanRef>(plist))
    return CFBooleanGetValue(restore_state);

  return false;
}

bool WasLaunchedAsHiddenLoginItem() {
  if (!WasLaunchedAsLoginOrResumeItem())
    return false;

  ScopedCFTypeRef<LSSharedFileListItemRef> item(GetLoginItemForApp());
  if (!item.get()) {
    // OS X can launch items for the resume feature.
    return false;
  }
  return IsHiddenLoginItem(item);
}

bool RemoveQuarantineAttribute(const FilePath& file_path) {
  const char kQuarantineAttrName[] = "com.apple.quarantine";
  int status = removexattr(file_path.value().c_str(), kQuarantineAttrName, 0);
  return status == 0 || errno == ENOATTR;
}

namespace {

// Returns the running system's Darwin major version. Don't call this, it's
// an implementation detail and its result is meant to be cached by
// MacOSXMinorVersion.
int DarwinMajorVersionInternal() {
  // base::OperatingSystemVersionNumbers calls Gestalt, which is a
  // higher-level operation than is needed. It might perform unnecessary
  // operations. On 10.6, it was observed to be able to spawn threads (see
  // http://crbug.com/53200). It might also read files or perform other
  // blocking operations. Actually, nobody really knows for sure just what
  // Gestalt might do, or what it might be taught to do in the future.
  //
  // uname, on the other hand, is implemented as a simple series of sysctl
  // system calls to obtain the relevant data from the kernel. The data is
  // compiled right into the kernel, so no threads or blocking or other
  // funny business is necessary.

  struct utsname uname_info;
  if (uname(&uname_info) != 0) {
    DPLOG(ERROR) << "uname";
    return 0;
  }

  if (strcmp(uname_info.sysname, "Darwin") != 0) {
    DLOG(ERROR) << "unexpected uname sysname " << uname_info.sysname;
    return 0;
  }

  int darwin_major_version = 0;
  char* dot = strchr(uname_info.release, '.');
  if (dot) {
    if (!base::StringToInt(base::StringPiece(uname_info.release,
                                             dot - uname_info.release),
                           &darwin_major_version)) {
      dot = NULL;
    }
  }

  if (!dot) {
    DLOG(ERROR) << "could not parse uname release " << uname_info.release;
    return 0;
  }

  return darwin_major_version;
}

// Returns the running system's Mac OS X minor version. This is the |y| value
// in 10.y or 10.y.z. Don't call this, it's an implementation detail and the
// result is meant to be cached by MacOSXMinorVersion.
int MacOSXMinorVersionInternal() {
  int darwin_major_version = DarwinMajorVersionInternal();

  // The Darwin major version is always 4 greater than the Mac OS X minor
  // version for Darwin versions beginning with 6, corresponding to Mac OS X
  // 10.2. Since this correspondence may change in the future, warn when
  // encountering a version higher than anything seen before. Older Darwin
  // versions, or versions that can't be determined, result in immediate death.
  CHECK(darwin_major_version >= 6);
  int mac_os_x_minor_version = darwin_major_version - 4;
  DLOG_IF(WARNING, darwin_major_version > 19)
      << "Assuming Darwin " << base::NumberToString(darwin_major_version)
      << " is macOS 10." << base::NumberToString(mac_os_x_minor_version);

  return mac_os_x_minor_version;
}

}  // namespace

namespace internal {
int MacOSXMinorVersion() {
  static int mac_os_x_minor_version = MacOSXMinorVersionInternal();
  return mac_os_x_minor_version;
}
}  // namespace internal

std::string GetModelIdentifier() {
  std::string return_string;
  ScopedIOObject<io_service_t> platform_expert(
      IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceMatching("IOPlatformExpertDevice")));
  if (platform_expert) {
    ScopedCFTypeRef<CFDataRef> model_data(
        static_cast<CFDataRef>(IORegistryEntryCreateCFProperty(
            platform_expert,
            CFSTR("model"),
            kCFAllocatorDefault,
            0)));
    if (model_data) {
      return_string =
          reinterpret_cast<const char*>(CFDataGetBytePtr(model_data));
    }
  }
  return return_string;
}

bool ParseModelIdentifier(const std::string& ident,
                          std::string* type,
                          int32_t* major,
                          int32_t* minor) {
  size_t number_loc = ident.find_first_of("0123456789");
  if (number_loc == std::string::npos)
    return false;
  size_t comma_loc = ident.find(',', number_loc);
  if (comma_loc == std::string::npos)
    return false;
  int32_t major_tmp, minor_tmp;
  std::string::const_iterator begin = ident.begin();
  if (!StringToInt(
          StringPiece(begin + number_loc, begin + comma_loc), &major_tmp) ||
      !StringToInt(
          StringPiece(begin + comma_loc + 1, ident.end()), &minor_tmp))
    return false;
  *type = ident.substr(0, number_loc);
  *major = major_tmp;
  *minor = minor_tmp;
  return true;
}

std::string GetOSDisplayName() {
  std::string os_name;
  if (IsAtMostOS10_11())
    os_name = "OS X";
  else
    os_name = "macOS";
  std::string version_string = base::SysNSStringToUTF8(
      [[NSProcessInfo processInfo] operatingSystemVersionString]);
  return os_name + " " + version_string;
}

}  // namespace mac
}  // namespace base
