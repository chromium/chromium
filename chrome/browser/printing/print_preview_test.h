// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_PREVIEW_TEST_H_
#define CHROME_BROWSER_PRINTING_PRINT_PREVIEW_TEST_H_

#include "chrome/test/base/browser_with_test_window_test.h"

class PrintPreviewTest : public BrowserWithTestWindowTest {
 public:
  PrintPreviewTest();

  PrintPreviewTest(const PrintPreviewTest&) = delete;
  PrintPreviewTest& operator=(const PrintPreviewTest&) = delete;

  ~PrintPreviewTest() override;

 protected:
  // Create a browser window to provide parenting for web contents modal dialog.
  std::unique_ptr<BrowserWindow> CreateBrowserWindow() override;
};

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_TEST_H_
