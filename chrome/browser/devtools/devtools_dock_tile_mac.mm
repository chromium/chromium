// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/devtools/devtools_dock_tile.h"

#import <Cocoa/Cocoa.h>

// static
void DevToolsDockTile::Update(const std::string& label, gfx::Image image) {
  NSDockTile* dockTile = [[NSApplication sharedApplication] dockTile];
  if (!image.IsEmpty()) {
    NSRect imageFrame = NSMakeRect(0, 0, 0, 0);
    base::scoped_nsobject<NSImageView> imageView(
        [[NSImageView alloc] initWithFrame:imageFrame]);
    NSImage* nsImage = image.ToNSImage();
    [imageView setImage:nsImage];
    [dockTile setContentView:imageView];
  }
  [dockTile setBadgeLabel:base::SysUTF8ToNSString(label)];
  [dockTile display];
}
