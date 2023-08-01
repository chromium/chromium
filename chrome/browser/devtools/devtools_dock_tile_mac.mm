// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_dock_tile.h"

#import <Cocoa/Cocoa.h>

#include "base/strings/sys_string_conversions.h"

// static
void DevToolsDockTile::Update(const std::string& label, gfx::Image image) {
  NSDockTile* dockTile = NSApplication.sharedApplication.dockTile;
  if (!image.IsEmpty()) {
    NSRect imageFrame = NSMakeRect(0, 0, 0, 0);
    NSImageView* imageView = [[NSImageView alloc] initWithFrame:imageFrame];
    imageView.image = image.ToNSImage();
    dockTile.contentView = imageView;
  }
  dockTile.badgeLabel = base::SysUTF8ToNSString(label);
  [dockTile display];
}
