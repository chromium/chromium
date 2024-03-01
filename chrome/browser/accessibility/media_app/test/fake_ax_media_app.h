// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_TEST_FAKE_AX_MEDIA_APP_H_
#define CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_TEST_FAKE_AX_MEDIA_APP_H_

#include <vector>

#include "chrome/browser/accessibility/media_app/ax_media_app.h"
#include "content/public/browser/browser_context.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ash::test {

// Used for testing. A fake Media App (AKA Backlight) that implements only the
// API used by the accessibility layer.
class FakeAXMediaApp final : public ash::AXMediaApp {
 public:
  FakeAXMediaApp();
  FakeAXMediaApp(const FakeAXMediaApp&) = delete;
  FakeAXMediaApp& operator=(const FakeAXMediaApp&) = delete;
  ~FakeAXMediaApp() override;

  bool IsOcrServiceEnabled() const { return ocr_service_enabled_; }
  bool IsAccessibilityEnabled() const { return accessibility_enabled_; }
  const std::vector<std::string>& PageIdsWithBitmap() const {
    return page_ids_with_bitmap_;
  }
  const gfx::RectF& ViewportBox() const { return viewport_box_; }

  // `AXMediaApp`:
  void OcrServiceEnabledChanged(bool enabled) override;
  void AccessibilityEnabledChanged(bool enabled) override;
  content::BrowserContext* GetBrowserContext() const override;
  SkBitmap RequestBitmap(const std::string& page_id) override;
  void SetViewport(const gfx::RectF& viewport_box) override;

 private:
  bool ocr_service_enabled_ = false;
  bool accessibility_enabled_ = false;
  std::vector<std::string> page_ids_with_bitmap_;
  gfx::RectF viewport_box_;
};

}  // namespace ash::test

#endif  // CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_TEST_FAKE_AX_MEDIA_APP_H_
