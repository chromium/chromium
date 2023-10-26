// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_TEST_FAKE_AX_MEDIA_APP_H_
#define CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_TEST_FAKE_AX_MEDIA_APP_H_

#include <stdint.h>

#include <vector>

#include "chrome/browser/accessibility/media_app/ax_media_app.h"
#include "content/public/browser/browser_context.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/insets.h"

namespace ash::test {

// Used for testing: A fake Media App (AKA Backlight) that implements only the
// API used by the accessibility layer.
class FakeAXMediaApp final : public ash::AXMediaApp {
 public:
  FakeAXMediaApp();
  FakeAXMediaApp(const FakeAXMediaApp&) = delete;
  FakeAXMediaApp& operator=(const FakeAXMediaApp&) = delete;
  ~FakeAXMediaApp() override;

  bool IsOcrServiceEnabled() const { return ocr_service_enabled_; }
  bool IsAccessibilityEnabled() const { return accessibility_enabled_; }
  const std::vector<uint64_t>& PageIndicesWithBitmap() const {
    return page_indices_with_bitmap_;
  }
  const gfx::Insets& GetViewportBox() const { return viewport_box_; }

  // `AXMediaApp`:
  void OcrServiceEnabledChanged(bool enabled) override;
  void AccessibilityEnabledChanged(bool enabled) override;
  content::BrowserContext* GetBrowserContext() const override;
  SkBitmap RequestBitmap(uint64_t page_index) override;
  void SetViewport(const gfx::Insets& viewport_box) override;

 private:
  bool ocr_service_enabled_ = false;
  bool accessibility_enabled_ = false;
  std::vector<uint64_t> page_indices_with_bitmap_;
  gfx::Insets viewport_box_;
};

}  // namespace ash::test

#endif  // CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_TEST_FAKE_AX_MEDIA_APP_H_
