// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/mac/info_plist_data.h"

#import <Foundation/Foundation.h>
#include <stdint.h>

#include "base/apple/bridging.h"
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/containers/span.h"

extern "C" {
// Key used within CoreFoundation for loaded Info plists
extern const CFStringRef _kCFBundleNumericVersionKey;
}

namespace base::mac {

std::vector<uint8_t> OuterBundleCachedInfoPlistData() {
  // NSBundle's info dictionary is used to ensure that any changes to Info.plist
  // on disk due to pending updates do not result in a version of the data being
  // used that doesn't match the code signature of the running app.
  NSMutableDictionary* info_plist_dictionary =
      [base::apple::OuterBundle().infoDictionary mutableCopy];
  if (!info_plist_dictionary.count) {
    return {};
  }

  // NSBundle inserts CFBundleNumericVersion into its in-memory copy of the info
  // dictionary despite it not being present on disk. Remove it so that the
  // serialized dictionary matches the Info.plist that was present at signing
  // time.
  info_plist_dictionary[base::apple::CFToNSPtrCast(
      _kCFBundleNumericVersionKey)] = nil;

  NSData* data = [NSPropertyListSerialization
      dataWithPropertyList:info_plist_dictionary
                    format:NSPropertyListXMLFormat_v1_0
                   options:0
                     error:nullptr];
  base::span<const uint8_t> span = apple::NSDataToSpan(data);
  return {span.begin(), span.end()};
}

}  // namespace base::mac
