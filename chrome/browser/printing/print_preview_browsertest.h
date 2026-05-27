// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_PREVIEW_BROWSERTEST_H_
#define CHROME_BROWSER_PRINTING_PRINT_PREVIEW_BROWSERTEST_H_

#include "chrome/test/base/in_process_browser_test.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include <memory>

#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif

namespace printing {

class PrintBackendServiceTestImpl;
class TestPrintBackend;

class PrintPreviewBrowserTest : public InProcessBrowserTest {
 public:
  PrintPreviewBrowserTest();
  ~PrintPreviewBrowserTest() override;

  // InProcessBrowserTest overrides:
  void SetUp() override;
  void SetUpOnMainThread() override;
  void TearDown() override;

 protected:
  scoped_refptr<TestPrintBackend> test_print_backend_;
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  mojo::Remote<mojom::PrintBackendService> test_remote_;
  std::unique_ptr<PrintBackendServiceTestImpl> print_backend_service_;
#endif
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_BROWSERTEST_H_
