// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/window_icon_util.h"

#import <Cocoa/Cocoa.h>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"
#include "third_party/skia/include/core/SkBitmap.h"

gfx::ImageSkia GetWindowIcon(content::DesktopMediaID id) {
  DCHECK(id.type == content::DesktopMediaID::TYPE_WINDOW);

  CGWindowID ids[1];
  ids[0] = id.id;
  base::apple::ScopedCFTypeRef<CFArrayRef> window_id_array(CFArrayCreate(
      nullptr, reinterpret_cast<const void**>(&ids), std::size(ids), nullptr));
  base::apple::ScopedCFTypeRef<CFArrayRef> window_array(
      CGWindowListCreateDescriptionFromArray(window_id_array.get()));
  if (!window_array || 0 == CFArrayGetCount(window_array.get())) {
    return gfx::ImageSkia();
  }

  CFDictionaryRef window = base::apple::CFCastStrict<CFDictionaryRef>(
      CFArrayGetValueAtIndex(window_array.get(), 0));
  CFNumberRef pid_ref = base::apple::GetValueFromDictionary<CFNumberRef>(
      window, kCGWindowOwnerPID);

  int pid;
  CFNumberGetValue(pid_ref, kCFNumberIntType, &pid);

  NSImage* icon_image =
      [[NSRunningApplication runningApplicationWithProcessIdentifier:pid] icon];

  // Icon's NSImage defaults to the smallest which can be only 32x32.
  NSRect proposed_rect = NSMakeRect(0, 0, 128, 128);
  CGImageRef cg_icon_image =
      [icon_image CGImageForProposedRect:&proposed_rect context:nil hints:nil];

  // 4 components of 8 bits each.
  if (CGImageGetBitsPerPixel(cg_icon_image) != 32 ||
      CGImageGetBitsPerComponent(cg_icon_image) != 8) {
    return gfx::ImageSkia();
  }

  // Premultiplied alpha and last (alpha channel is next to the blue channel)
  if (CGImageGetAlphaInfo(cg_icon_image) != kCGImageAlphaPremultipliedLast) {
    return gfx::ImageSkia();
  }

  // Ensure BGR like.
  int byte_order = CGImageGetBitmapInfo(cg_icon_image) & kCGBitmapByteOrderMask;
  if (byte_order != kCGBitmapByteOrderDefault &&
      byte_order != kCGBitmapByteOrder32Big) {
    return gfx::ImageSkia();
  }

  CGDataProviderRef provider = CGImageGetDataProvider(cg_icon_image);
  base::apple::ScopedCFTypeRef<CFDataRef> cf_data(
      CGDataProviderCopyData(provider));

  int width = CGImageGetWidth(cg_icon_image);
  int height = CGImageGetHeight(cg_icon_image);
  int src_stride = CGImageGetBytesPerRow(cg_icon_image);
  const uint8_t* src_data = CFDataGetBytePtr(cf_data.get());

  SkBitmap result;
  result.allocN32Pixels(width, height, false /* no-premultiplied */);

  uint8_t* pixels_data = reinterpret_cast<uint8_t*>(result.getPixels());

  libyuv::ABGRToARGB(src_data, src_stride, pixels_data, result.rowBytes(),
                     width, height);

  return gfx::ImageSkia::CreateFrom1xBitmap(result);
}
