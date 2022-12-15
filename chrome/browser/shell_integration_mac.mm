// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/version_info.h"
#import "net/base/mac/url_conversions.h"
#import "third_party/mozilla/NSWorkspace+Utils.h"

namespace shell_integration {

namespace {

// Returns true if |identifier| is the bundle id of the default client
// application for the given protocol.
bool IsIdentifierDefaultProtocolClient(NSString* identifier,
                                       NSString* protocol) {
  base::ScopedCFTypeRef<CFStringRef> default_client(
      LSCopyDefaultHandlerForURLScheme(base::mac::NSToCFCast(protocol)));
  if (!default_client)
    return false;

  // Do the comparison case-insensitively as LS smashes the case.
  NSComparisonResult result =
      [base::mac::CFToNSCast(default_client) caseInsensitiveCompare:identifier];
  return result == NSOrderedSame;
}

}  // namespace

// Sets Chromium as default browser to be used by the operating system. This
// applies only for the current user. Returns false if this cannot be done, or
// if the operation fails.
bool SetAsDefaultBrowser() {
  // We really do want the outer bundle here, not the main bundle since setting
  // a shortcut to Chrome as the default browser doesn't make sense.
  NSString* identifier = [base::mac::OuterBundle() bundleIdentifier];
  if (!identifier)
    return false;

  [[NSWorkspace sharedWorkspace] setDefaultBrowserWithIdentifier:identifier];

  // The CoreServicesUIAgent presents a dialog asking the user to confirm their
  // new default browser choice, but the agent sometimes orders the dialog
  // behind the Chrome window. The user never sees the dialog, and therefore
  // never confirms the change. Make the CoreServicesUIAgent active so the
  // confirmation dialog comes to the front.
  NSString* const kCoreServicesUIAgentBundleID =
      @"com.apple.coreservices.uiagent";

  for (NSRunningApplication* application in
       [[NSWorkspace sharedWorkspace] runningApplications]) {
    if ([[application bundleIdentifier]
            isEqualToString:kCoreServicesUIAgentBundleID]) {
      [application activateWithOptions:NSApplicationActivateAllWindows];
      break;
    }
  }

  return true;
}

// Sets Chromium as the default application to be used by the operating system
// for the given protocol. This applies only for the current user. Returns false
// if this cannot be done, or if the operation fails.
bool SetAsDefaultProtocolClient(const std::string& protocol) {
  if (protocol.empty())
    return false;

  if (GetDefaultWebClientSetPermission() != SET_DEFAULT_UNATTENDED)
    return false;

  // We really do want the main bundle here since it makes sense to set an
  // app shortcut as a default protocol handler.
  NSString* identifier = [base::mac::MainBundle() bundleIdentifier];
  if (!identifier)
    return false;

  NSString* protocol_ns = base::SysUTF8ToNSString(protocol);
  OSStatus return_code =
      LSSetDefaultHandlerForURLScheme(base::mac::NSToCFCast(protocol_ns),
                                      base::mac::NSToCFCast(identifier));
  return return_code == noErr;
}

DefaultWebClientSetPermission
GetPlatformSpecificDefaultWebClientSetPermission() {
  return SET_DEFAULT_UNATTENDED;
}

std::u16string GetApplicationNameForProtocol(const GURL& url) {
  NSURL* ns_url = [NSURL URLWithString:
      base::SysUTF8ToNSString(url.possibly_invalid_spec())];
  base::ScopedCFTypeRef<CFErrorRef> out_err;
  base::ScopedCFTypeRef<CFURLRef> openingApp(LSCopyDefaultApplicationURLForURL(
      (CFURLRef)ns_url, kLSRolesAll, out_err.InitializeInto()));
  if (out_err) {
    // likely kLSApplicationNotFoundErr
    return std::u16string();
  }
  NSString* appPath = [base::mac::CFToNSCast(openingApp.get()) path];
  NSString* appDisplayName =
      [[NSFileManager defaultManager] displayNameAtPath:appPath];
  return base::SysNSStringToUTF16(appDisplayName);
}

std::vector<base::FilePath> GetAllApplicationPathsForURL(const GURL& url) {
  NSURL* ns_url = net::NSURLWithGURL(url);
  NSArray* app_urls = nil;
  if (@available(macos 12.0, *)) {
    app_urls =
        [[NSWorkspace sharedWorkspace] URLsForApplicationsToOpenURL:ns_url];
  } else {
    CFArrayRef urls =
        LSCopyApplicationURLsForURL(base::mac::NSToCFCast(ns_url), kLSRolesAll);
    app_urls = [base::mac::CFToNSCast(urls) autorelease];
  }
  std::vector<base::FilePath> app_paths;
  if ([app_urls count] == 0)
    return app_paths;
  app_paths.reserve([app_urls count]);
  for (NSURL* app_url in app_urls) {
    app_paths.push_back(base::mac::NSURLToFilePath(app_url));
  }
  return app_paths;
}

bool CanApplicationHandleURL(const base::FilePath& app_path, const GURL& url) {
  NSURL* ns_item_url = net::NSURLWithGURL(url);
  NSURL* ns_app_url = base::mac::FilePathToNSURL(app_path);
  Boolean result = FALSE;
  LSCanURLAcceptURL(base::mac::NSToCFCast(ns_item_url),
                    base::mac::NSToCFCast(ns_app_url), kLSRolesAll,
                    kLSAcceptDefault, &result);
  return result;
}

// Attempt to determine if this instance of Chrome is the default browser and
// return the appropriate state. (Defined as being the handler for HTTP/HTTPS
// protocols; we don't want to report "no" here if the user has simply chosen
// to open HTML files in a text editor and FTP links with an FTP client.)
DefaultWebClientState GetDefaultBrowser() {
  // We really do want the outer bundle here, since this we want to know the
  // status of the main Chrome bundle and not a shortcut.
  NSString* my_identifier = [base::mac::OuterBundle() bundleIdentifier];
  if (!my_identifier)
    return UNKNOWN_DEFAULT;

  base::ScopedCFTypeRef<CFStringRef> default_browser_cf(
      LSCopyDefaultHandlerForURLScheme(CFSTR("http")));
  if (!default_browser_cf)
    return NOT_DEFAULT;
  NSString* default_browser = base::mac::CFToNSCast(default_browser_cf);

  // Do the comparison case-insensitively as LS smashes the case.
  if ([default_browser caseInsensitiveCompare:my_identifier] == NSOrderedSame)
    return IS_DEFAULT;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Flavors of Chrome are of the constructions "com.google.Chrome" and
  // "com.google.Chrome.beta". If the first three components match, then these
  // are variant flavors.
  auto three_components_only_lopper = [](NSString* bundle_id) {
    NSMutableArray<NSString*>* parts =
        [[bundle_id componentsSeparatedByString:@"."] mutableCopy];
    while ([parts count] > 3)
      [parts removeLastObject];
    return [parts componentsJoinedByString:@"."];
  };

  NSString* my_identifier_lopped = three_components_only_lopper(my_identifier);
  NSString* default_browser_lopped =
      three_components_only_lopper(default_browser);

  // Do the comparisons case-insensitively as LS smashes the case.
  if ([my_identifier_lopped caseInsensitiveCompare:default_browser_lopped] ==
      NSOrderedSame) {
    return OTHER_MODE_IS_DEFAULT;
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return NOT_DEFAULT;
}

// Returns true if Firefox is the default browser for the current user.
bool IsFirefoxDefaultBrowser() {
  base::ScopedCFTypeRef<CFStringRef> default_browser(
      LSCopyDefaultHandlerForURLScheme(CFSTR("http")));
  if (!default_browser)
    return false;

  // Do the comparison case-insensitively as LS smashes the case.
  return CFStringCompare(default_browser, CFSTR("org.mozilla.firefox"),
                         kCFCompareCaseInsensitive) == kCFCompareEqualTo;
}

// Attempt to determine if this instance of Chrome is the default client
// application for the given protocol and return the appropriate state.
DefaultWebClientState IsDefaultProtocolClient(const std::string& protocol) {
  if (protocol.empty())
    return UNKNOWN_DEFAULT;

  // We really do want the main bundle here since it makes sense to set an
  // app shortcut as a default protocol handler.
  NSString* my_identifier = [base::mac::MainBundle() bundleIdentifier];
  if (!my_identifier)
    return UNKNOWN_DEFAULT;

  return IsIdentifierDefaultProtocolClient(my_identifier,
                                           base::SysUTF8ToNSString(protocol))
             ? IS_DEFAULT
             : NOT_DEFAULT;
}

}  // namespace shell_integration
