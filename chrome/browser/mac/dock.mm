// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/mac/dock.h"

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <signal.h>

#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/launchd.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/strings/sys_string_conversions.h"
#include "build/branding_buildflags.h"

extern "C" {

// Undocumented private internal CFURL functions. The Dock uses these to
// serialize and deserialize CFURLs for use in its plist's file-data keys. See
// 10.5.8 CF-476.19 and 10.7.2 CF-635.15's CFPriv.h and CFURL.c. The property
// list representation will contain, at the very least, the _CFURLStringType
// and _CFURLString keys. _CFURLStringType is a number that defines the
// interpretation of the _CFURLString. It may be a CFURLPathStyle value, or
// the CFURL-internal FULL_URL_REPRESENTATION value (15). Prior to Mac OS X
// 10.7.2, the Dock plist always used kCFURLPOSIXPathStyle (0), formatting
// _CFURLString as a POSIX path. In Mac OS X 10.7.2 (CF-635.15), it uses
// FULL_URL_REPRESENTATION along with a file:/// URL. This is due to a change
// in _CFURLInit.

CFPropertyListRef _CFURLCopyPropertyListRepresentation(CFURLRef url);
CFURLRef _CFURLCreateFromPropertyListRepresentation(
    CFAllocatorRef allocator, CFPropertyListRef property_list_representation);

}  // extern "C"

namespace dock {
namespace {

NSString* const kDockTileDataKey = @"tile-data";
NSString* const kDockFileDataKey = @"file-data";
NSString* const kDockDomain = @"com.apple.dock";
NSString* const kDockPersistentAppsKey = @"persistent-apps";

// A wrapper around _CFURLCopyPropertyListRepresentation that operates on
// Foundation data types and returns an autoreleased NSDictionary.
NSDictionary* NSURLCopyDictionary(NSURL* url) {
  CFURLRef url_cf = base::mac::NSToCFCast(url);
  base::ScopedCFTypeRef<CFPropertyListRef> property_list(
      _CFURLCopyPropertyListRepresentation(url_cf));
  CFDictionaryRef dictionary_cf =
      base::mac::CFCast<CFDictionaryRef>(property_list);
  NSDictionary* dictionary = base::mac::CFToNSCast(dictionary_cf);

  if (!dictionary) {
    return nil;
  }

  NSMakeCollectable(property_list.release());
  return [dictionary autorelease];
}

// A wrapper around _CFURLCreateFromPropertyListRepresentation that operates
// on Foundation data types and returns an autoreleased NSURL.
NSURL* NSURLCreateFromDictionary(NSDictionary* dictionary) {
  CFDictionaryRef dictionary_cf = base::mac::NSToCFCast(dictionary);
  base::ScopedCFTypeRef<CFURLRef> url_cf(
      _CFURLCreateFromPropertyListRepresentation(NULL, dictionary_cf));
  NSURL* url = base::mac::CFToNSCast(url_cf);

  if (!url) {
    return nil;
  }

  NSMakeCollectable(url_cf.release());
  return [url autorelease];
}

// Returns an array parallel to |persistent_apps| containing only the
// pathnames of the Dock tiles contained therein. Returns nil on failure, such
// as when the structure of |persistent_apps| is not understood.
NSMutableArray* PersistentAppPaths(NSArray* persistent_apps) {
  if (!persistent_apps) {
    return nil;
  }

  NSMutableArray* app_paths =
      [NSMutableArray arrayWithCapacity:[persistent_apps count]];

  for (NSDictionary* app in persistent_apps) {
    if (![app isKindOfClass:[NSDictionary class]]) {
      LOG(ERROR) << "app not NSDictionary";
      return nil;
    }

    NSDictionary* tile_data = app[kDockTileDataKey];
    if (![tile_data isKindOfClass:[NSDictionary class]]) {
      LOG(ERROR) << "tile_data not NSDictionary";
      return nil;
    }

    NSDictionary* file_data = tile_data[kDockFileDataKey];
    if (![file_data isKindOfClass:[NSDictionary class]]) {
      // Some apps (e.g. Dashboard) have no file data, but instead have a
      // special value for the tile-type key. For these, add an empty string to
      // align indexes with the source array.
      [app_paths addObject:@""];
      continue;
    }

    NSURL* url = NSURLCreateFromDictionary(file_data);
    if (!url) {
      LOG(ERROR) << "no URL";
      return nil;
    }

    if (![url isFileURL]) {
      LOG(ERROR) << "non-file URL";
      return nil;
    }

    NSString* path = [url path];
    [app_paths addObject:path];
  }

  return app_paths;
}

// Restart the Dock process by sending it a SIGTERM.
void Restart() {
  // Doing this via launchd using the proper job label is the safest way to
  // handle the restart. Unlike "killall Dock", looking this up via launchd
  // guarantees that only the right process will be targeted.
  pid_t pid = base::mac::PIDForJob("com.apple.Dock.agent");
  if (pid <= 0) {
    return;
  }

  // Sending a SIGTERM to the Dock seems to be a more reliable way to get the
  // replacement Dock process to read the newly written plist than using the
  // equivalent of "launchctl stop" (even if followed by "launchctl start.")
  // Note that this is a potential race in that pid may no longer be valid or
  // may even have been reused.
  kill(pid, SIGTERM);
}

NSDictionary* DockPlistFromUserDefaults() {
  NSDictionary* dock_plist = [[NSUserDefaults standardUserDefaults]
      persistentDomainForName:kDockDomain];
  if (![dock_plist isKindOfClass:[NSDictionary class]]) {
    LOG(ERROR) << "dock_plist is not an NSDictionary";
    return nil;
  }
  return dock_plist;
}

NSArray* PersistentAppsFromDockPlist(NSDictionary* dock_plist) {
  if (!dock_plist) {
    return nil;
  }
  NSArray* persistent_apps = dock_plist[kDockPersistentAppsKey];
  if (![persistent_apps isKindOfClass:[NSArray class]]) {
    LOG(ERROR) << "persistent_apps is not an NSArray";
    return nil;
  }
  return persistent_apps;
}

}  // namespace

ChromeInDockStatus ChromeIsInTheDock() {
  NSDictionary* dock_plist = DockPlistFromUserDefaults();
  NSArray* persistent_apps = PersistentAppsFromDockPlist(dock_plist);

  if (!persistent_apps) {
    return ChromeInDockFailure;
  }

  NSString* launch_path = [base::mac::OuterBundle() bundlePath];

  return [PersistentAppPaths(persistent_apps) containsObject:launch_path]
             ? ChromeInDockTrue
             : ChromeInDockFalse;
}

AddIconStatus AddIcon(NSString* installed_path, NSString* dmg_app_path) {
  // ApplicationServices.framework/Frameworks/HIServices.framework contains an
  // undocumented function, CoreDockAddFileToDock, that is able to add items
  // to the Dock "live" without requiring a Dock restart. Under the hood, it
  // communicates with the Dock via Mach IPC. It is available as of Mac OS X
  // 10.6. AddIcon could call CoreDockAddFileToDock if available, but
  // CoreDockAddFileToDock seems to always to add the new Dock icon last,
  // where AddIcon takes care to position the icon appropriately. Based on
  // disassembly, the signature of the undocumented function appears to be
  //    extern "C" OSStatus CoreDockAddFileToDock(CFURLRef url, int);
  // The int argument doesn't appear to have any effect. It's not used as the
  // position to place the icon as hoped.

  // There's enough potential allocation in this function to justify a
  // distinct pool.
  base::mac::ScopedNSAutoreleasePool autorelease_pool;

  NSMutableDictionary* dock_plist = [NSMutableDictionary
      dictionaryWithDictionary:DockPlistFromUserDefaults()];
  NSMutableArray* persistent_apps =
      [NSMutableArray arrayWithArray:PersistentAppsFromDockPlist(dock_plist)];

  NSMutableArray* persistent_app_paths = PersistentAppPaths(persistent_apps);
  if (!persistent_app_paths) {
    return IconAddFailure;
  }

  NSUInteger already_installed_app_index = NSNotFound;
  NSUInteger app_index = NSNotFound;
  for (NSUInteger index = 0; index < [persistent_apps count]; ++index) {
    NSString* app_path = persistent_app_paths[index];
    if ([app_path isEqualToString:installed_path]) {
      // If the Dock already contains a reference to the newly installed
      // application, don't add another one.
      already_installed_app_index = index;
    } else if ([app_path isEqualToString:dmg_app_path]) {
      // If the Dock contains a reference to the application on the disk
      // image, replace it with a reference to the newly installed
      // application. However, if the Dock contains a reference to both the
      // application on the disk image and the newly installed application,
      // just remove the one referencing the disk image.
      //
      // This case is only encountered when the user drags the icon from the
      // disk image volume window in the Finder directly into the Dock.
      app_index = index;
    }
  }

  bool made_change = false;

  if (app_index != NSNotFound) {
    // Remove the Dock's reference to the application on the disk image.
    [persistent_apps removeObjectAtIndex:app_index];
    [persistent_app_paths removeObjectAtIndex:app_index];
    made_change = true;
  }

  if (already_installed_app_index == NSNotFound) {
    // The Dock doesn't yet have a reference to the icon at the
    // newly installed path. Figure out where to put the new icon.
    NSString* app_name = [installed_path lastPathComponent];

    if (app_index == NSNotFound) {
      // If an application with this name is already in the Dock, put the new
      // one right before it.
      for (NSUInteger index = 0; index < [persistent_apps count]; ++index) {
        NSString* dock_app_name =
            [persistent_app_paths[index] lastPathComponent];
        if ([dock_app_name isEqualToString:app_name]) {
          app_index = index;
          break;
        }
      }
    }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    if (app_index == NSNotFound) {
      // If this is an officially-branded Chrome (including Canary) and an
      // application matching the "other" flavor is already in the Dock, put
      // them next to each other. Google Chrome will precede Google Chrome
      // Canary in the Dock.
      NSString* chrome_name = @"Google Chrome.app";
      NSString* canary_name = @"Google Chrome Canary.app";
      for (NSUInteger index = 0; index < [persistent_apps count]; ++index) {
        NSString* dock_app_name =
            [[persistent_app_paths objectAtIndex:index] lastPathComponent];
        if ([dock_app_name isEqualToString:canary_name] &&
            [app_name isEqualToString:chrome_name]) {
          app_index = index;

          // Break: put Google Chrome.app before the first Google Chrome
          // Canary.app.
          break;
        } else if ([dock_app_name isEqualToString:chrome_name] &&
                   [app_name isEqualToString:canary_name]) {
          app_index = index + 1;

          // No break: put Google Chrome Canary.app after the last Google
          // Chrome.app.
        }
      }
    }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

    if (app_index == NSNotFound) {
      // Put the new application after the last browser application already
      // present in the Dock.
      NSArray* other_browser_app_names =
          [NSArray arrayWithObjects:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
                       @"Chromium.app",  // Unbranded Google Chrome
#else
                       @"Google Chrome.app", @"Google Chrome Canary.app",
#endif
                       @"Safari.app", @"Firefox.app", @"Camino.app",
                       @"Opera.app", @"OmniWeb.app",
                       @"WebKit.app",   // Safari nightly
                       @"Aurora.app",   // Firefox dev
                       @"Nightly.app",  // Firefox nightly
                       nil];
      for (NSUInteger index = 0; index < [persistent_apps count]; ++index) {
        NSString* dock_app_name =
            [persistent_app_paths[index] lastPathComponent];
        if ([other_browser_app_names containsObject:dock_app_name]) {
          app_index = index + 1;
        }
      }
    }

    if (app_index == NSNotFound) {
      // Put the new application last in the Dock.
      app_index = [persistent_apps count];
    }

    // Set up the new Dock tile.
    NSURL* url = [NSURL fileURLWithPath:installed_path isDirectory:YES];
    NSDictionary* url_dict = NSURLCopyDictionary(url);
    if (!url_dict) {
      LOG(ERROR) << "couldn't create url_dict";
      return IconAddFailure;
    }

    NSDictionary* new_tile_data = @{kDockFileDataKey : url_dict};
    NSDictionary* new_tile = @{kDockTileDataKey : new_tile_data};

    // Add the new tile to the Dock.
    [persistent_apps insertObject:new_tile atIndex:app_index];
    [persistent_app_paths insertObject:installed_path atIndex:app_index];
    made_change = true;
  }

  // Verify that the arrays are still parallel.
  DCHECK_EQ([persistent_apps count], [persistent_app_paths count]);

  if (!made_change) {
    // If no changes were made, there's no point in rewriting the Dock's
    // plist or restarting the Dock.
    return IconAlreadyPresent;
  }

  // Rewrite the plist.
  dock_plist[kDockPersistentAppsKey] = persistent_apps;
  [[NSUserDefaults standardUserDefaults] setPersistentDomain:dock_plist
                                                     forName:kDockDomain];

  Restart();
  return IconAddSuccess;
}

}  // namespace dock
