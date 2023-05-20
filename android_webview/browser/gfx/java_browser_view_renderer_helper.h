// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_JAVA_BROWSER_VIEW_RENDERER_HELPER_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_JAVA_BROWSER_VIEW_RENDERER_HELPER_H_

#include <jni.h>

#include <memory>

#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

class SkCanvas;
struct AwDrawSWFunctionTable;

namespace android_webview {

// Lifetime: Temporary
class SoftwareCanvasHolder {
 public:
  static std::unique_ptr<SoftwareCanvasHolder> Create(
      jobject java_canvas,
      const gfx::Point& scroll_correction,
      const gfx::Size& auxiliary_bitmap_size,
      bool force_auxiliary_bitmap);

  virtual ~SoftwareCanvasHolder() {}

  // The returned SkCanvas is still owned by this holder.
  virtual SkCanvas* GetCanvas() = 0;
};

void RasterHelperSetAwDrawSWFunctionTable(AwDrawSWFunctionTable* table);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_JAVA_BROWSER_VIEW_RENDERER_HELPER_H_
