// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_H_
#define CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_H_

#include <stdint.h>

#include "content/public/browser/browser_context.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ash {

// The interface that is implemented by the Media App (AKA Backlight) which is
// used by the accessibility layer to talk to it.
class AXMediaApp {
 public:
  AXMediaApp(const AXMediaApp&) = delete;
  AXMediaApp& operator=(const AXMediaApp&) = delete;
  virtual ~AXMediaApp() = default;

  virtual void OcrServiceEnabledChanged(bool enabled) = 0;
  virtual void AccessibilityEnabledChanged(bool enabled) = 0;
  virtual content::BrowserContext* GetBrowserContext() const = 0;
  virtual SkBitmap RequestBitmap(const std::string& page_id) = 0;
  virtual void SetViewport(const gfx::RectF& viewport_box) = 0;

 protected:
  AXMediaApp() = default;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_H_
