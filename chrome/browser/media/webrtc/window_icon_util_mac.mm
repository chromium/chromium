// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/window_icon_util.h"

#import <Cocoa/Cocoa.h>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/feature_list.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

// Enables the fallback rasterization path for window icons that use internal
// formats incompatible with the strict requirements of libyuv::ABGRToARGB.
BASE_FEATURE(kMacWindowIconRasterization,
             "MacWindowIconRasterization",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Checks if CGImageRef image is compatible with libyuv::ABGRToARGB.
BOOL IsValidCGImage(CGImageRef image) {
  if (!image) {
    return NO;
  }

  // 1. Must be 32 bits per pixel (8 bits per component * 4 components)
  if (CGImageGetBitsPerPixel(image) != 32 ||
      CGImageGetBitsPerComponent(image) != 8) {
    return NO;
  }

  // 2. Must use premultiplied alpha
  if (CGImageGetAlphaInfo(image) != kCGImageAlphaPremultipliedLast) {
    return NO;
  }

  // 3. Must use a compatible byte order
  CGBitmapInfo byte_order =
      CGImageGetBitmapInfo(image) & kCGBitmapByteOrderInfoMask;
  if (byte_order != kCGImageByteOrderDefault &&
      byte_order != kCGImageByteOrder32Big) {
    return NO;
  }

  return YES;
}

// Forces an NSImage to rasterize into a standard 32-bit RGBA bitmap.
// This ensures that any incompatible source representation is converted to
// meet the strict 8 bpc requirements of libyuv::ABGRToARGB.
NSImage* CreateRasterizedIcon(NSImage* original_icon) {
  if (!original_icon) {
    return nil;
  }

  // Define the target rect (128x128 is a good balance for thumbnails)
  NSRect icon_rect = NSMakeRect(0, 0, 128.0, 128.0);

  // Create a strictly configured bitmap representation
  NSBitmapImageRep* rep = [[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes:NULL
                    pixelsWide:icon_rect.size.width
                    pixelsHigh:icon_rect.size.height
                 bitsPerSample:8
               samplesPerPixel:4
                      hasAlpha:YES
                      isPlanar:NO
                colorSpaceName:NSCalibratedRGBColorSpace
                  bitmapFormat:0
                   bytesPerRow:0
                  bitsPerPixel:0];

  // Draw the original icon into this new representation
  [NSGraphicsContext saveGraphicsState];
  NSGraphicsContext* context =
      [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
  [NSGraphicsContext setCurrentContext:context];

  [original_icon drawInRect:icon_rect
                   fromRect:NSZeroRect
                  operation:NSCompositingOperationCopy
                   fraction:1.0f];

  [context flushGraphics];
  [NSGraphicsContext restoreGraphicsState];

  NSImage* rasterized_icon = [[NSImage alloc] initWithSize:icon_rect.size];
  [rasterized_icon addRepresentation:rep];

  return rasterized_icon;
}

}  // namespace

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

  if (!icon_image) {
    return gfx::ImageSkia();
  }

  NSRect proposed_rect = NSMakeRect(0, 0, 128, 128);
  CGImageRef cg_icon_image = [icon_image CGImageForProposedRect:&proposed_rect
                                                        context:nil
                                                          hints:nil];

  // The CGImage returned for some applications may use internal formats that
  // are incompatible with the strict 8 bpc / 32 bpp requirement of
  // libyuv::ABGRToARGB. If the fast path yields an incompatible format, we
  // must force a rasterization into a standardized 32-bit bitmap.
  if (!IsValidCGImage(cg_icon_image)) {
    if (!base::FeatureList::IsEnabled(kMacWindowIconRasterization)) {
      return gfx::ImageSkia();
    }

    icon_image = CreateRasterizedIcon(icon_image);
    cg_icon_image = [icon_image CGImageForProposedRect:&proposed_rect
                                               context:nil
                                                 hints:nil];

    if (!IsValidCGImage(cg_icon_image)) {
      return gfx::ImageSkia();
    }
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
