// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/bundle_locations.h"

#include "base/check.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"

namespace base::apple {

namespace {

NSBundle* g_override_framework_bundle = nil;
NSBundle* g_override_outer_bundle = nil;

}  // namespace

NSBundle* MainBundle() {
  return NSBundle.mainBundle;
}

NSURL* MainBundleURL() {
  return MainBundle().bundleURL;
}

FilePath MainBundlePath() {
  return mac::NSStringToFilePath(MainBundle().bundlePath);
}

NSBundle* OuterBundle() {
  if (g_override_outer_bundle) {
    return g_override_outer_bundle;
  }
  return NSBundle.mainBundle;
}

NSURL* OuterBundleURL() {
  return OuterBundle().bundleURL;
}

FilePath OuterBundlePath() {
  return mac::NSStringToFilePath(OuterBundle().bundlePath);
}

NSBundle* FrameworkBundle() {
  if (g_override_framework_bundle) {
    return g_override_framework_bundle;
  }
  return NSBundle.mainBundle;
}

FilePath FrameworkBundlePath() {
  return mac::NSStringToFilePath(FrameworkBundle().bundlePath);
}

namespace {

NSBundle* BundleFromPath(const FilePath& file_path) {
  if (file_path.empty()) {
    return nil;
  }

  NSBundle* bundle = [NSBundle bundleWithURL:mac::FilePathToNSURL(file_path)];
  CHECK(bundle) << "Failed to load the bundle at " << file_path.value();

  return bundle;
}

}  // namespace

void SetOverrideOuterBundle(NSBundle* bundle) {
  g_override_outer_bundle = bundle;
}

void SetOverrideFrameworkBundle(NSBundle* bundle) {
  g_override_framework_bundle = bundle;
}

void SetOverrideOuterBundlePath(const FilePath& file_path) {
  NSBundle* bundle = BundleFromPath(file_path);
  g_override_outer_bundle = bundle;
}

void SetOverrideFrameworkBundlePath(const FilePath& file_path) {
  NSBundle* bundle = BundleFromPath(file_path);
  g_override_framework_bundle = bundle;
}

}  // namespace base::apple
