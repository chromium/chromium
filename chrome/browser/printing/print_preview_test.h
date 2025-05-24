// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_PREVIEW_TEST_H_
#define CHROME_BROWSER_PRINTING_PRINT_PREVIEW_TEST_H_

#include "base/memory/scoped_refptr.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include <memory>

#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif

namespace printing {
class PrintBackendServiceTestImpl;
class TestPrintBackend;
}  // namespace printing

class PrintPreviewTest : public BrowserWithTestWindowTest {
 public:
  PrintPreviewTest();

  PrintPreviewTest(const PrintPreviewTest&) = delete;
  PrintPreviewTest& operator=(const PrintPreviewTest&) = delete;

  ~PrintPreviewTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  // Create a browser window to provide parenting for web contents modal dialog.
  std::unique_ptr<BrowserWindow> CreateBrowserWindow() override;

 private:
  scoped_refptr<printing::TestPrintBackend> test_print_backend_;
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  mojo::Remote<printing::mojom::PrintBackendService> test_remote_;
  std::unique_ptr<printing::PrintBackendServiceTestImpl> print_backend_service_;
#endif
};

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_TEST_H_
