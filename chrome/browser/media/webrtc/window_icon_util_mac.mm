// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/window_icon_util.h"

#import <Cocoa/Cocoa.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/check_op.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

gfx::ImageSkia GetWindowIcon(content::DesktopMediaID id) {
  DCHECK_EQ(content::DesktopMediaID::TYPE_WINDOW, id.type);

  // CGWindowListCreateDescriptionFromArray takes a CFArray that contains raw
  // CGWindowID values (not NS/CFNumbers), so create an array that has null
  // callbacks rather than the usual kCFTypeArrayCallBacks.
  CGWindowID window_id = id.id;
  base::apple::ScopedCFTypeRef<CFArrayRef> window_ids(CFArrayCreate(
      /*allocator=*/nullptr,
      /*values=*/reinterpret_cast<const void**>(&window_id), /*numValues=*/1,
      /*callBacks=*/nullptr));

  base::apple::ScopedCFTypeRef<CFArrayRef> windows_cf(
      CGWindowListCreateDescriptionFromArray(window_ids.get()));
  NSArray* windows = base::apple::CFToNSPtrCast(windows_cf.get());

  NSDictionary* window_dictionary =
      base::apple::ObjCCast<NSDictionary>(windows.firstObject);

  NSNumber* pid = base::apple::ObjCCast<NSNumber>(
      window_dictionary[base::apple::CFToNSPtrCast(kCGWindowOwnerPID)]);
  if (!pid) {
    // This file relies heavily on the "send a message to nil and get a zero
    // back" behavior of Objective-C, to avoid constant nil-checks. However,
    // will NSRunningApplication return an empty image if given a zero/error
    // pid? Explicitly nil-check for paranoia's sake.
    return gfx::ImageSkia();
  }

  NSImage* icon_image =
      [NSRunningApplication
          runningApplicationWithProcessIdentifier:pid.intValue]
          .icon;
  // Something has gone wrong; theoretically the app (and window) could have
  // terminated while getting the icon, or perhaps there is some other issue
  // with Launch Services. These icons are low-stakes, so an early return is
  // fine.
  if (!icon_image) {
    return gfx::ImageSkia();
  }

  return gfx::Image(icon_image).AsImageSkia();
}
