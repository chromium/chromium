// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_brand.h"

#include <string>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/channel_info.h"

namespace google_brand {

namespace {

std::string ReadBrandFile(NSString* path) {
  NSURL* path_url = [NSURL fileURLWithPath:path];
  NSDictionary* brand_dictionary =
      [NSDictionary dictionaryWithContentsOfURL:path_url error:nil];
  return base::SysNSStringToUTF8(
      base::apple::ObjCCast<NSString>(brand_dictionary[@"KSBrandID"]));
}

std::string GetBrandInternal() {
  // Non-side-by-side dev and beta do not have a brand code.
  if (!chrome::IsSideBySideCapable()) {
    return {};
  }

  // If there is a system brand file, use it.
  NSFileManager* fm = NSFileManager.defaultManager;
  NSString* system_brand_file =
      [@"/Library/Google/Google Chrome Brand.plist" stringByStandardizingPath];
  if ([fm fileExistsAtPath:system_brand_file]) {
    return ReadBrandFile(system_brand_file);
  }

  // Otherwise, use the brand code from within the app, if present.
  // If this mismatches a user brand code file, the updater will fix it on the
  // next update.
  NSString* app_bundle_brand_id = base::apple::ObjCCast<NSString>(
      base::apple::OuterBundle().infoDictionary[@"KSBrandID"]);
  if (app_bundle_brand_id) {
    return base::SysNSStringToUTF8(app_bundle_brand_id);
  }

  // Otherwise, use the user brand code file, if present.
  NSString* user_brand_file =
      [@"~/Library/Google/Google Chrome Brand.plist" stringByStandardizingPath];
  if ([fm fileExistsAtPath:user_brand_file]) {
    return ReadBrandFile(user_brand_file);
  }

  // Otherwise, there is no brand code.
  return {};
}

}  // namespace

bool GetBrand(std::string* brand) {
  if (g_brand_for_testing) {
    *brand = g_brand_for_testing;
    return true;
  }

  static base::NoDestructor<std::string> s_brand_code(GetBrandInternal());
  *brand = *s_brand_code;
  return true;
}

bool GetReactivationBrand(std::string* brand) {
  brand->clear();
  return true;
}

}  // namespace google_brand
