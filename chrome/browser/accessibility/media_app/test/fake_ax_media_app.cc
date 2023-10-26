// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/test/fake_ax_media_app.h"

#include <utility>

namespace ash::test {

FakeAXMediaApp::FakeAXMediaApp() = default;
FakeAXMediaApp::~FakeAXMediaApp() = default;

void FakeAXMediaApp::OcrServiceEnabledChanged(bool enabled) {
  ocr_service_enabled_ = enabled;
}

void FakeAXMediaApp::AccessibilityEnabledChanged(bool enabled) {
  accessibility_enabled_ = enabled;
}

content::BrowserContext* FakeAXMediaApp::GetBrowserContext() const {
  return nullptr;
}

SkBitmap FakeAXMediaApp::RequestBitmap(uint64_t page_index) {
  page_indices_with_bitmap_.push_back(page_index);
  SkBitmap fake_bitmap;
  fake_bitmap.allocN32Pixels(/*width=*/1, /*height=*/1, /*isOpaque=*/false);
  return fake_bitmap;
}

void FakeAXMediaApp::SetViewport(const gfx::Insets& viewport_box) {
  viewport_box_ = viewport_box;
}

}  // namespace ash::test
