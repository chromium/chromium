// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/mac_util.h"

#import <Cocoa/Cocoa.h>
#include <CoreServices/CoreServices.h>
#import <IOKit/IOKitLib.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/xattr.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/scoped_aedesc.h"
#include "base/mac/scoped_ioobject.h"
#include "base/posix/sysctl.h"
#include "base/strings/string_number_conversions.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"

namespace base::mac {

namespace {

class LoginItemsFileList {
 public:
  LoginItemsFileList() = default;
  LoginItemsFileList(const LoginItemsFileList&) = delete;
  LoginItemsFileList& operator=(const LoginItemsFileList&) = delete;
  ~LoginItemsFileList() = default;

  [[nodiscard]] bool Initialize() {
    DCHECK(!login_items_) << __func__ << " called more than once.";
    // The LSSharedFileList suite of functions has been deprecated. Instead,
    // a LoginItems helper should be registered with SMLoginItemSetEnabled()
    // https://crbug.com/1154377.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    login_items_.reset(LSSharedFileListCreate(
        nullptr, kLSSharedFileListSessionLoginItems, nullptr));
#pragma clang diagnostic pop
    DLOG_IF(ERROR, !login_items_.get()) << "Couldn't get a Login Items list.";
    return login_items_.get();
  }

  LSSharedFileListRef GetLoginFileList() {
    DCHECK(login_items_) << "Initialize() failed or not called.";
    return login_items_.get();
  }

  // Looks into Shared File Lists corresponding to Login Items for the item
  // representing the specified bundle.  If such an item is found, returns a
  // retained reference to it. Caller is responsible for releasing the
  // reference.
  apple::ScopedCFTypeRef<LSSharedFileListItemRef> GetLoginItemForApp(
      NSURL* url) {
    DCHECK(login_items_) << "Initialize() failed or not called.";

#pragma clang diagnostic push  // https://crbug.com/1154377
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    apple::ScopedCFTypeRef<CFArrayRef> login_items_array(
        LSSharedFileListCopySnapshot(login_items_.get(), /*inList=*/nullptr));
#pragma clang diagnostic pop

    for (CFIndex i = 0; i < CFArrayGetCount(login_items_array.get()); ++i) {
      LSSharedFileListItemRef item =
          (LSSharedFileListItemRef)CFArrayGetValueAtIndex(
              login_items_array.get(), i);
#pragma clang diagnostic push  // https://crbug.com/1154377
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
      // kLSSharedFileListDoNotMountVolumes is used so that we don't trigger
      // mounting when it's not expected by a user. Just listing the login
      // items should not cause any side-effects.
      NSURL* item_url =
          apple::CFToNSOwnershipCast(LSSharedFileListItemCopyResolvedURL(
              item, kLSSharedFileListDoNotMountVolumes, /*outError=*/nullptr));
#pragma clang diagnostic pop

      if (item_url && [item_url isEqual:url]) {
        return apple::ScopedCFTypeRef<LSSharedFileListItemRef>(
            item, base::scoped_policy::RETAIN);
      }
    }

    return apple::ScopedCFTypeRef<LSSharedFileListItemRef>();
  }

  apple::ScopedCFTypeRef<LSSharedFileListItemRef> GetLoginItemForMainApp() {
    NSURL* url = [NSURL fileURLWithPath:base::apple::MainBundle().bundlePath];
    return GetLoginItemForApp(url);
  }

 private:
  apple::ScopedCFTypeRef<LSSharedFileListRef> login_items_;
};

bool IsHiddenLoginItem(LSSharedFileListItemRef item) {
#pragma clang diagnostic push  // https://crbug.com/1154377
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  apple::ScopedCFTypeRef<CFBooleanRef> hidden(
      reinterpret_cast<CFBooleanRef>(LSSharedFileListItemCopyProperty(
          item, kLSSharedFileListLoginItemHidden)));
#pragma clang diagnostic pop

  return hidden && hidden.get() == kCFBooleanTrue;
}

}  // namespace

CGColorSpaceRef GetSRGBColorSpace() {
  // Leaked.  That's OK, it's scoped to the lifetime of the application.
  static CGColorSpaceRef g_color_space_sRGB =
      CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  DLOG_IF(ERROR, !g_color_space_sRGB) << "Couldn't get the sRGB color space";
  return g_color_space_sRGB;
}

void AddToLoginItems(const FilePath& app_bundle_file_path,
                     bool hide_on_startup) {
  LoginItemsFileList login_items;
  if (!login_items.Initialize()) {
    return;
  }

  NSURL* app_bundle_url = base::apple::FilePathToNSURL(app_bundle_file_path);
  apple::ScopedCFTypeRef<LSSharedFileListItemRef> item =
      login_items.GetLoginItemForApp(app_bundle_url);

  if (item.get() && (IsHiddenLoginItem(item.get()) == hide_on_startup)) {
    return;  // There already is a login item with required hide flag.
  }

  // Remove the old item, it has wrong hide flag, we'll create a new one.
  if (item.get()) {
#pragma clang diagnostic push  // https://crbug.com/1154377
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    LSSharedFileListItemRemove(login_items.GetLoginFileList(), item.get());
#pragma clang diagnostic pop
  }

#pragma clang diagnostic push  // https://crbug.com/1154377
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  BOOL hide = hide_on_startup ? YES : NO;
  NSDictionary* properties =
      @{apple::CFToNSPtrCast(kLSSharedFileListLoginItemHidden) : @(hide)};

  apple::ScopedCFTypeRef<LSSharedFileListItemRef> new_item(
      LSSharedFileListInsertItemURL(
          login_items.GetLoginFileList(), kLSSharedFileListItemLast,
          /*inDisplayName=*/nullptr,
          /*inIconRef=*/nullptr, apple::NSToCFPtrCast(app_bundle_url),
          apple::NSToCFPtrCast(properties), /*inPropertiesToClear=*/nullptr));
#pragma clang diagnostic pop

  if (!new_item.get()) {
    DLOG(ERROR) << "Couldn't insert current app into Login Items list.";
  }
}

void RemoveFromLoginItems(const FilePath& app_bundle_file_path) {
  LoginItemsFileList login_items;
  if (!login_items.Initialize()) {
    return;
  }

  NSURL* app_bundle_url = base::apple::FilePathToNSURL(app_bundle_file_path);
  apple::ScopedCFTypeRef<LSSharedFileListItemRef> item =
      login_items.GetLoginItemForApp(app_bundle_url);
  if (!item.get()) {
    return;
  }

#pragma clang diagnostic push  // https://crbug.com/1154377
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  LSSharedFileListItemRemove(login_items.GetLoginFileList(), item.get());
#pragma clang diagnostic pop
}

bool WasLaunchedAsLoginOrResumeItem() {
  ProcessSerialNumber psn = {0, kCurrentProcess};
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
  // "Reopen windows..." option was added for 10.7.  Prior OS versions should
  // not have this behavior.
  if (!WasLaunchedAsLoginOrResumeItem()) {
    return false;
  }

  CFStringRef app = CFSTR("com.apple.loginwindow");
  CFStringRef save_state = CFSTR("TALLogoutSavesState");
  apple::ScopedCFTypeRef<CFPropertyListRef> plist(
      CFPreferencesCopyAppValue(save_state, app));
  // According to documentation, com.apple.loginwindow.plist does not exist on a
  // fresh installation until the user changes a login window setting.  The
  // "reopen windows" option is checked by default, so the plist would exist had
  // the user unchecked it.
  // https://developer.apple.com/library/mac/documentation/macosx/conceptual/bpsystemstartup/chapters/CustomLogin.html
  if (!plist) {
    return true;
  }

  if (CFBooleanRef restore_state =
          base::apple::CFCast<CFBooleanRef>(plist.get())) {
    return CFBooleanGetValue(restore_state);
  }

  return false;
}

bool WasLaunchedAsHiddenLoginItem() {
  if (!WasLaunchedAsLoginOrResumeItem()) {
    return false;
  }

  LoginItemsFileList login_items;
  if (!login_items.Initialize()) {
    return false;
  }

  apple::ScopedCFTypeRef<LSSharedFileListItemRef> item(
      login_items.GetLoginItemForMainApp());
  if (!item.get()) {
    // The OS itself can launch items, usually for the resume feature.
    return false;
  }
  return IsHiddenLoginItem(item.get());
}

bool RemoveQuarantineAttribute(const FilePath& file_path) {
  const char kQuarantineAttrName[] = "com.apple.quarantine";
  int status = removexattr(file_path.value().c_str(), kQuarantineAttrName, 0);
  return status == 0 || errno == ENOATTR;
}

void SetFileTags(const FilePath& file_path,
                 const std::vector<std::string>& file_tags) {
  if (file_tags.empty()) {
    return;
  }

  NSMutableArray* tag_array = [NSMutableArray array];
  for (const auto& tag : file_tags) {
    [tag_array addObject:SysUTF8ToNSString(tag)];
  }

  NSURL* file_url = apple::FilePathToNSURL(file_path);
  [file_url setResourceValue:tag_array forKey:NSURLTagNamesKey error:nil];
}

namespace {

int ParseOSProductVersion(const std::string_view& version) {
  int macos_version = 0;

  // The number of parts that need to be a part of the return value
  // (major/minor/bugfix).
  int parts = 3;

  // When a Rapid Security Response is applied to a system, the UI will display
  // an additional letter (e.g. "13.4.1 (a)"). That extra letter should not be
  // present in `version_string`; in fact, the version string should not contain
  // any spaces. However, take the first space-delimited "word" for parsing.
  std::vector<std::string_view> words = base::SplitStringPiece(
      version, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  CHECK_GE(words.size(), 1u);

  // There are expected to be either two or three numbers separated by a dot.
  // Walk through them, and add them to the version string.
  for (const auto& value_str : base::SplitStringPiece(
           words[0], ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL)) {
    int value;
    bool success = base::StringToInt(value_str, &value);
    CHECK(success);
    macos_version *= 100;
    macos_version += value;
    if (--parts == 0) {
      break;
    }
  }

  // While historically the string has comprised exactly two or three numbers
  // separated by a dot, it's not inconceivable that it might one day be only
  // one number. Therefore, only check to see that at least one number was found
  // and processed.
  CHECK_LE(parts, 2);

  // Tack on as many '00 digits as needed to be sure that exactly three version
  // numbers are returned.
  for (int i = 0; i < parts; ++i) {
    macos_version *= 100;
  }

  // Checks that the value is within expected bounds corresponding to released
  // OS version numbers. The most important bit is making sure that the "10.16"
  // compatibility mode isn't engaged.
  CHECK(macos_version >= 10'00'00);
  CHECK(macos_version < 10'16'00 || macos_version >= 11'00'00);

  return macos_version;
}

}  // namespace

int ParseOSProductVersionForTesting(const std::string_view& version) {
  return ParseOSProductVersion(version);
}

int MacOSVersion() {
  static int macos_version = ParseOSProductVersion(
      StringSysctlByName("kern.osproductversion").value());

  return macos_version;
}

namespace {

#if defined(ARCH_CPU_X86_64)
// https://developer.apple.com/documentation/apple_silicon/about_the_rosetta_translation_environment#3616845
bool ProcessIsTranslated() {
  int ret = 0;
  size_t size = sizeof(ret);
  if (sysctlbyname("sysctl.proc_translated", &ret, &size, nullptr, 0) == -1) {
    return false;
  }
  return ret;
}
#endif  // ARCH_CPU_X86_64

}  // namespace

CPUType GetCPUType() {
#if defined(ARCH_CPU_ARM64)
  return CPUType::kArm;
#elif defined(ARCH_CPU_X86_64)
  return ProcessIsTranslated() ? CPUType::kTranslatedIntel : CPUType::kIntel;
#else
#error Time for another chip transition?
#endif  // ARCH_CPU_*
}

std::string GetOSDisplayName() {
  std::string version_string = base::SysNSStringToUTF8(
      NSProcessInfo.processInfo.operatingSystemVersionString);
  return "macOS " + version_string;
}

std::string GetPlatformSerialNumber() {
  base::mac::ScopedIOObject<io_service_t> expert_device(
      IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceMatching("IOPlatformExpertDevice")));
  if (!expert_device) {
    DLOG(ERROR) << "Error retrieving the machine serial number.";
    return std::string();
  }

  apple::ScopedCFTypeRef<CFTypeRef> serial_number(
      IORegistryEntryCreateCFProperty(expert_device.get(),
                                      CFSTR(kIOPlatformSerialNumberKey),
                                      kCFAllocatorDefault, 0));
  CFStringRef serial_number_cfstring =
      base::apple::CFCast<CFStringRef>(serial_number.get());
  if (!serial_number_cfstring) {
    DLOG(ERROR) << "Error retrieving the machine serial number.";
    return std::string();
  }

  return base::SysCFStringRefToUTF8(serial_number_cfstring);
}

void OpenSystemSettingsPane(SystemSettingsPane pane,
                            const std::string& id_param) {
  NSString* url = nil;
  NSString* pane_file = nil;
  NSData* subpane_data = nil;
  // On macOS 13 and later, System Settings are implemented with app extensions
  // found at /System/Library/ExtensionKit/Extensions/. URLs to open them are
  // constructed with a scheme of "x-apple.systempreferences" and a body of the
  // the bundle ID of the app extension. (In the Info.plist there is an
  // EXAppExtensionAttributes dictionary with legacy identifiers, but given that
  // those are explicitly named "legacy", this code prefers to use the bundle
  // IDs for the URLs it uses.) It is not yet known how to definitively identify
  // the query string used to open sub-panes; the ones used below were
  // determined from historical usage, disassembly of related code, and
  // guessing. Clarity was requested from Apple in FB11753405. The current best
  // guess is to analyze the method named -revealElementForKey:, but because
  // the extensions are all written in Swift it's hard to confirm this is
  // correct or to use this knowledge.
  //
  // For macOS 12 and earlier, to determine the `subpane_data`, find a method
  // named -handleOpenParameter: which takes an AEDesc as a parameter.
  switch (pane) {
    case SystemSettingsPane::kAccessibility_Captions:
      if (MacOSMajorVersion() >= 13) {
        url = @"x-apple.systempreferences:com.apple.Accessibility-Settings."
              @"extension?Captioning";
      } else {
        url = @"x-apple.systempreferences:com.apple.preference.universalaccess?"
              @"Captioning";
      }
      break;
    case SystemSettingsPane::kDateTime:
      if (MacOSMajorVersion() >= 13) {
        url =
            @"x-apple.systempreferences:com.apple.Date-Time-Settings.extension";
      } else {
        pane_file = @"/System/Library/PreferencePanes/DateAndTime.prefPane";
      }
      break;
    case SystemSettingsPane::kNetwork_Proxies:
      if (MacOSMajorVersion() >= 13) {
        url = @"x-apple.systempreferences:com.apple.Network-Settings.extension?"
              @"Proxies";
      } else {
        pane_file = @"/System/Library/PreferencePanes/Network.prefPane";
        subpane_data = [@"Proxies" dataUsingEncoding:NSASCIIStringEncoding];
      }
      break;
    case SystemSettingsPane::kNotifications:
      if (MacOSMajorVersion() >= 13) {
        url = @"x-apple.systempreferences:com.apple.Notifications-Settings."
              @"extension";
        if (!id_param.empty()) {
          url = [url stringByAppendingFormat:@"?id=%s", id_param.c_str()];
        }
      } else {
        pane_file = @"/System/Library/PreferencePanes/Notifications.prefPane";
        NSDictionary* subpane_dict = @{
          @"command" : @"show",
          @"identifier" : SysUTF8ToNSString(id_param)
        };
        subpane_data = [NSPropertyListSerialization
            dataWithPropertyList:subpane_dict
                          format:NSPropertyListXMLFormat_v1_0
                         options:0
                           error:nil];
      }
      break;
    case SystemSettingsPane::kPrintersScanners:
      if (MacOSMajorVersion() >= 13) {
        url = @"x-apple.systempreferences:com.apple.Print-Scan-Settings."
              @"extension";
      } else {
        pane_file = @"/System/Library/PreferencePanes/PrintAndFax.prefPane";
      }
      break;
    case SystemSettingsPane::kPrivacySecurity:
      if (MacOSMajorVersion() >= 13) {
        url = @"x-apple.systempreferences:com.apple.settings.PrivacySecurity."
              @"extension?Privacy";
      } else {
        url = @"x-apple.systempreferences:com.apple.preference.security?"
              @"Privacy";
      }
      break;
    case SystemSettingsPane::kPrivacySecurity_Accessibility:
      if (MacOSMajorVersion() >= 13) {
        url = @"x-apple.systempreferences:com.apple.settings.PrivacySecurity."
              @"extension?Privacy_Accessibility";
      } else {
        url = @"x-apple.systempreferences:com.apple.preference.security?"
              @"Privacy_Accessibility";
      }
      break;
    case SystemSettingsPane::kPrivacySecurity_Bluetooth:
      if (MacOSMajorVersion() >= 13) {
        url = @"x-apple.systempreferences:com.apple.settings.PrivacySecurity."
              @"extension?Privacy_Bluetooth";
      } else {
        url = @"x-apple.systempreferences:com.apple.preference.security?"
              @"Privacy_Bluetooth";
      }
      break;
    case SystemSettingsPane::kPrivacySecurity_Camera:
      if (MacOSMajorVersion() >= 13) {
        url = @"x-apple.systempreferences:com.apple.settings.PrivacySecurity."
              @"extension?Privacy_Camera";
      } else {
        url = @"x-apple.systempreferences:com.apple.preference.security?"
              @"Privacy_Camera";
      }
      break;
    case SystemSettingsPane::kPrivacySecurity_Extensions_Sharing:
      if (MacOSMajorVersion() >= 13) {
        // See ShareKit, -[SHKSharingServicePicker openAppExtensionsPrefpane].
        url = @"x-apple.systempreferences:com.apple.ExtensionsPreferences?"
              @"Sharing";
      } else {
        // This is equivalent to the implementation of AppKit's
        // +[NSSharingServicePicker openAppExtensionsPrefPane].
        pane_file = @"/System/Library/PreferencePanes/Extensions.prefPane";
        NSDictionary* subpane_dict = @{
          @"action" : @"revealExtensionPoint",
          @"protocol" : @"com.apple.share-services"
        };
        subpane_data = [NSPropertyListSerialization
            dataWithPropertyList:subpane_dict
                          format:NSPropertyListXMLFormat_v1_0
                         options:0
                           error:nil];
      }
      break;
    case SystemSettingsPane::kPrivacySecurity_LocationServices:
      if (MacOSMajorVersion() >= 13) {
        url = @"x-apple.systempreferences:com.apple.settings.PrivacySecurity."
              @"extension?Privacy_LocationServices";
      } else {
        url = @"x-apple.systempreferences:com.apple.preference.security?"
              @"Privacy_LocationServices";
      }
      break;
    case SystemSettingsPane::kPrivacySecurity_Microphone:
      if (MacOSMajorVersion() >= 13) {
        url = @"x-apple.systempreferences:com.apple.settings.PrivacySecurity."
              @"extension?Privacy_Microphone";
      } else {
        url = @"x-apple.systempreferences:com.apple.preference.security?"
              @"Privacy_Microphone";
      }
      break;
    case SystemSettingsPane::kPrivacySecurity_ScreenRecording:
      if (MacOSMajorVersion() >= 13) {
        url = @"x-apple.systempreferences:com.apple.settings.PrivacySecurity."
              @"extension?Privacy_ScreenCapture";
      } else {
        url = @"x-apple.systempreferences:com.apple.preference.security?"
              @"Privacy_ScreenCapture";
      }
      break;
    case SystemSettingsPane::kTrackpad:
      if (MacOSMajorVersion() >= 13) {
        url = @"x-apple.systempreferences:com.apple.Trackpad-Settings."
              @"extension";
      } else {
        pane_file = @"/System/Library/PreferencePanes/Trackpad.prefPane";
      }
      break;
  }

  DCHECK(url != nil ^ pane_file != nil);

  if (url) {
    [NSWorkspace.sharedWorkspace openURL:[NSURL URLWithString:url]];
    return;
  }

  NSAppleEventDescriptor* subpane_descriptor;
  NSArray* pane_file_urls = @[ [NSURL fileURLWithPath:pane_file] ];

  LSLaunchURLSpec launchSpec = {0};
  launchSpec.itemURLs = apple::NSToCFPtrCast(pane_file_urls);
  if (subpane_data) {
    subpane_descriptor =
        [[NSAppleEventDescriptor alloc] initWithDescriptorType:'ptru'
                                                          data:subpane_data];
    launchSpec.passThruParams = subpane_descriptor.aeDesc;
  }
  launchSpec.launchFlags = kLSLaunchAsync | kLSLaunchDontAddToRecents;

  LSOpenFromURLSpec(&launchSpec, nullptr);
}

}  // namespace base::mac
