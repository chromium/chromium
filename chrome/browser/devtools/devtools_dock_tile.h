// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_DOCK_TILE_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_DOCK_TILE_H_

#include "ui/gfx/image/image.h"

class DevToolsDockTile {
 public:
  DevToolsDockTile(const DevToolsDockTile&) = delete;
  DevToolsDockTile& operator=(const DevToolsDockTile&) = delete;

  static void Update(const std::string& label, gfx::Image image);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_DOCK_TILE_H_
