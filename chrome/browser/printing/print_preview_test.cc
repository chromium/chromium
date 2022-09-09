// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_test.h"

#include "chrome/test/base/dialog_test_browser_window.h"

PrintPreviewTest::PrintPreviewTest() {}
PrintPreviewTest::~PrintPreviewTest() {}

std::unique_ptr<BrowserWindow> PrintPreviewTest::CreateBrowserWindow() {
  return std::make_unique<DialogTestBrowserWindow>();
}
