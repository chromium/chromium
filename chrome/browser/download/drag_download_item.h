// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_ITEM_H_
#define CHROME_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_ITEM_H_

#include "ui/gfx/native_widget_types.h"

namespace download {
class DownloadItem;
}

namespace gfx {
class Image;
}

// Helper function for download views to use when acting as a drag source for a
// DownloadItem. If |icon| is NULL, no image will be accompany the drag. |view|
// is only required for Mac OS X, elsewhere it can be NULL.
void DragDownloadItem(const download::DownloadItem* download,
                      const gfx::Image* icon,
                      gfx::NativeView view);

#endif  // CHROME_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_ITEM_H_
